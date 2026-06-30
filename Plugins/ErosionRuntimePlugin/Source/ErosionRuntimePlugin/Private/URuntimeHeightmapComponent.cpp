// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "URuntimeHeightmapComponent.h"
#include "SimulationBusSubsystem.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TimerManager.h"

// ── RHI / RDG includes ────────────────────────────────────────────────────────
#include "RHI.h"
#include "RHICommandList.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "PipelineStateCache.h"

// ── Compute shader declaration ────────────────────────────────────────────────

class FErosionCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FErosionCS);
    SHADER_USE_PARAMETER_STRUCT(FErosionCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, HeightmapUAV)
        SHADER_PARAMETER(int32, PatchResolution)
        SHADER_PARAMETER(float, ErosionRate)
    END_SHADER_PARAMETER_STRUCT()

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Params)
    {
        return IsFeatureLevelSupported(Params.Platform, ERHIFeatureLevel::SM5);
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& Params,
        FShaderCompilerEnvironment& OutEnv)
    {
        FGlobalShader::ModifyCompilationEnvironment(Params, OutEnv);
        OutEnv.SetDefine(TEXT("THREAD_GROUP_SIZE"), 8);
    }
};

IMPLEMENT_GLOBAL_SHADER(FErosionCS,
    "/Plugin/ErosionRuntimePlugin/ErosionCS.usf",
    "MainCS",
    SF_Compute);

// ── Render-thread dispatch helper ─────────────────────────────────────────────

static void DispatchErosionCS_RenderThread(
    FRHICommandListImmediate& RHICmdList,
    FRHITexture*              RTTexture,
    float                     ErosionRate,
    int32                     PatchRes)
{
    FRDGBuilder GraphBuilder(RHICmdList);

    FRDGTextureRef RDGTexture = GraphBuilder.RegisterExternalTexture(
        CreateRenderTarget(RTTexture, TEXT("ErosionDisplacementRT")));

    FRDGTextureUAVRef UAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RDGTexture));

    TShaderMapRef<FErosionCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

    FErosionCS::FParameters* Params = GraphBuilder.AllocParameters<FErosionCS::FParameters>();
    Params->HeightmapUAV     = UAV;
    Params->PatchResolution  = PatchRes;
    Params->ErosionRate      = ErosionRate;

    const int32 GroupSize = 8;
    FComputeShaderUtils::AddPass(
        GraphBuilder,
        RDG_EVENT_NAME("Mahlanya_ErosionCS"),
        Shader,
        Params,
        FIntVector(
            FMath::DivideAndRoundUp(PatchRes, GroupSize),
            FMath::DivideAndRoundUp(PatchRes, GroupSize),
            1));

    GraphBuilder.Execute();
}

// ── URuntimeHeightmapComponent ────────────────────────────────────────────────

URuntimeHeightmapComponent::URuntimeHeightmapComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void URuntimeHeightmapComponent::BeginPlay()
{
    Super::BeginPlay();

    // Create the displacement render target
    DisplacementTarget = NewObject<UTextureRenderTarget2D>(this);
    DisplacementTarget->RenderTargetFormat = RTF_R16f;
    DisplacementTarget->InitAutoFormat(PatchResolution, PatchResolution);
    DisplacementTarget->UpdateResourceImmediate(true);

    // Subscribe to rain events via SimulationBus
    if (UWorld* World = GetWorld())
    {
        if (USimulationBusSubsystem* Bus = World->GetSubsystem<USimulationBusSubsystem>())
        {
            RainDelegateHandle = Bus->OnRainIntensityChanged.AddUObject(
                this, &URuntimeHeightmapComponent::OnRainIntensityChanged);
        }
    }
}

void URuntimeHeightmapComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ResetToBaseline();

    // Unsubscribe from SimulationBus
    if (UWorld* World = GetWorld())
    {
        if (USimulationBusSubsystem* Bus = World->GetSubsystem<USimulationBusSubsystem>())
        {
            Bus->OnRainIntensityChanged.Remove(RainDelegateHandle);
        }
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ErosionTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void URuntimeHeightmapComponent::OnRainIntensityChanged(float IntensityMmPerHr)
{
    CurrentRainIntensity = IntensityMmPerHr;

    UWorld* World = GetWorld();
    if (!World) return;

    if (IntensityMmPerHr > 0.f)
    {
        // Start erosion timer if not already running
        if (!ErosionTimerHandle.IsValid())
        {
            const float Interval = 1.f / FMath::Max(ErosionIterationsPerSecond, 1.f);
            World->GetTimerManager().SetTimer(
                ErosionTimerHandle,
                this,
                &URuntimeHeightmapComponent::DispatchErosionIteration,
                Interval,
                /*bLoop=*/true);
        }
    }
    else
    {
        World->GetTimerManager().ClearTimer(ErosionTimerHandle);
    }
}

void URuntimeHeightmapComponent::DispatchErosionIteration()
{
    if (!DisplacementTarget || CurrentRainIntensity <= 0.f) return;

    FTextureRenderTargetResource* Resource =
        DisplacementTarget->GameThread_GetRenderTargetResource();
    if (!Resource) return;

    const float ErosionRate = CurrentRainIntensity * 0.0001f; // scale mm/hr → per-texel rate
    const int32 Res = PatchResolution;

    ENQUEUE_RENDER_COMMAND(MahlanyaErosionCS)(
        [Resource, ErosionRate, Res](FRHICommandListImmediate& RHICmdList)
        {
            FRHITexture* RTTexture = Resource->GetRenderTargetTexture();
            if (RTTexture)
            {
                DispatchErosionCS_RenderThread(RHICmdList, RTTexture, ErosionRate, Res);
            }
        });

    // Accumulate saturation: more rain → higher saturation
    AccumulatedSaturation = FMath::Clamp(
        AccumulatedSaturation + CurrentRainIntensity * 0.0002f, 0.f, 1.f);

    // Broadcast back to SimulationBus (MicroclimateEngine reads this for fog feedback)
    if (UWorld* World = GetWorld())
    {
        if (USimulationBusSubsystem* Bus = World->GetSubsystem<USimulationBusSubsystem>())
        {
            Bus->BroadcastTerrainSaturationChanged(AccumulatedSaturation);
        }
    }
}

void URuntimeHeightmapComponent::ApplyFootprint(
    FVector WorldLocation, float Depth, int32 RadiusTexels)
{
    if (!DisplacementTarget) return;

    // Convert world XY → texel coordinate
    const float PatchSizeCm = PatchRadiusCm * 2.f;
    const float U = (WorldLocation.X - PatchOriginXY.X) / PatchSizeCm;
    const float V = (WorldLocation.Y - PatchOriginXY.Y) / PatchSizeCm;

    const int32 TexelX = FMath::Clamp(FMath::FloorToInt(U * PatchResolution), 0, PatchResolution - 1);
    const int32 TexelY = FMath::Clamp(FMath::FloorToInt(V * PatchResolution), 0, PatchResolution - 1);

    // Build a small CPU-side kernel and upload via RHIUpdateTexture2D
    const int32 KernelSize = RadiusTexels * 2 + 1;
    TArray<FFloat16> Kernel;
    Kernel.SetNumZeroed(KernelSize * KernelSize);

    for (int32 dy = -RadiusTexels; dy <= RadiusTexels; ++dy)
    {
        for (int32 dx = -RadiusTexels; dx <= RadiusTexels; ++dx)
        {
            const float Dist = FMath::Sqrt((float)(dx * dx + dy * dy));
            const float Weight = FMath::Max(0.f, 1.f - Dist / (float)RadiusTexels);
            const int32 Idx = (dy + RadiusTexels) * KernelSize + (dx + RadiusTexels);
            Kernel[Idx] = FFloat16(Depth * Weight);
        }
    }

    const int32 WriteX = FMath::Clamp(TexelX - RadiusTexels, 0, PatchResolution - 1);
    const int32 WriteY = FMath::Clamp(TexelY - RadiusTexels, 0, PatchResolution - 1);
    const int32 WriteW = FMath::Min(KernelSize, PatchResolution - WriteX);
    const int32 WriteH = FMath::Min(KernelSize, PatchResolution - WriteY);

    FUpdateTextureRegion2D Region(WriteX, WriteY, 0, 0, WriteW, WriteH);
    const uint32 Pitch = KernelSize * sizeof(FFloat16);

    FTextureRenderTargetResource* Resource =
        DisplacementTarget->GameThread_GetRenderTargetResource();

    TArray<FFloat16> KernelCopy = Kernel;
    ENQUEUE_RENDER_COMMAND(MahlanyaFootprintWrite)(
        [Resource, Region, Pitch, KernelCopy = MoveTemp(KernelCopy)](
            FRHICommandListImmediate& RHICmdList) mutable
        {
            if (FRHITexture* Tex = Resource ? Resource->GetRenderTargetTexture() : nullptr)
            {
                RHICmdList.UpdateTexture2D(
                    Tex, 0, Region,
                    Pitch,
                    reinterpret_cast<const uint8*>(KernelCopy.GetData()));
            }
        });
}

void URuntimeHeightmapComponent::ResetToBaseline()
{
    AccumulatedSaturation = 0.f;

    if (!DisplacementTarget) return;

    FTextureRenderTargetResource* Resource =
        DisplacementTarget->GameThread_GetRenderTargetResource();

    ENQUEUE_RENDER_COMMAND(MahlanyaErosionReset)(
        [Resource](FRHICommandListImmediate& RHICmdList)
        {
            if (FRHITexture* Tex = Resource ? Resource->GetRenderTargetTexture() : nullptr)
            {
                RHICmdList.TransitionResource(
                    ERHIAccess::UAVMask, Tex);
                RHICmdList.ClearUAVFloat(
                    Tex->GetDefaultView(),
                    FVector4f(0.f, 0.f, 0.f, 0.f));
            }
        });
}
