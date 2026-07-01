// Copyright Charles Bartaria. All Rights Reserved.

#include "UHardwareAdaptiveScaler.h"
#include "MahlanyaPerformanceCVars.h"
#include "Core/MahlanyaLogChannels.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProperties.h"
#include "RHI.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "DeviceProfiles/DeviceProfile.h"

// CVar names for the 5 simulation CVars registered by UMahlanyaScalabilitySubsystem
static const TCHAR* CVar_RuntimeErosion  = TEXT("mahlanya.RuntimeErosion.Enabled");
static const TCHAR* CVar_RuntimeVoronoi  = TEXT("mahlanya.RuntimeVoronoi.Enabled");
static const TCHAR* CVar_AudioRayCount   = TEXT("mahlanya.GeometricAudio.RayCount");
static const TCHAR* CVar_AudioMaxBounces = TEXT("mahlanya.GeometricAudio.MaxBounces");
static const TCHAR* CVar_DrawDistance    = TEXT("mahlanya.DrawDistance.Km");

static bool IsIntegratedGPU(const FString& AdapterName)
{
    const FString Lower = AdapterName.ToLower();
    if (Lower.Contains(TEXT("intel")))
        return !Lower.Contains(TEXT("arc")); // Intel Arc = discrete; everything else = iGPU
    if (Lower.Contains(TEXT("vega")) && !Lower.Contains(TEXT("rx")))
        return true; // AMD Ryzen iGPU (Radeon Vega Graphics, Vega 8, Vega 11)
    if (Lower.Contains(TEXT("amd")) && Lower.Contains(TEXT("radeon graphics")) && !Lower.Contains(TEXT("rx")))
        return true; // AMD Radeon Graphics (Ryzen 4000+ iGPU)
    return false;
}

void UHardwareAdaptiveScaler::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Developer override: skip detection when ForceHardwareTier != -1
    const int32 ForcedTier = MahlanyaPerformanceCVars::ForceHardwareTier.GetValueOnGameThread();
    if (ForcedTier >= 0 && ForcedTier <= 4)
    {
        DetectedTier = static_cast<EHardwareTier>(ForcedTier);
        bProfileDetected = true;
        UE_LOG(LogMahlanyaHardware, Log,
               TEXT("Hardware tier forced to %d via mahlanya.ForceHardwareTier"), ForcedTier);
    }
    else
    {
        DetectedTier = DetectHardwareTier();
        bProfileDetected = true;
    }

    CurrentConfig = BuildConfigForTier(DetectedTier);
    ApplyDeviceProfileOverride();
    WriteCVarsForTier(DetectedTier);

    UE_LOG(LogMahlanyaHardware, Log,
           TEXT("Hardware adaptive scaler initialized. Tier=%d Cores=%d RAM=%.1fGB VRAM=%.1fGB"),
           (int32)DetectedTier, HardwareProfile.CoreCount, HardwareProfile.RAM_GB, HardwareProfile.VRAM_GB);
}

EHardwareTier UHardwareAdaptiveScaler::DetectHardwareTier()
{
    HardwareProfile.CoreCount = FPlatformMisc::NumberOfCores();
    HardwareProfile.bIsMobile = FPlatformProperties::IsServer() ? false :
        (FString(FPlatformProperties::IniPlatformName()).Contains(TEXT("Android")) ||
         FString(FPlatformProperties::IniPlatformName()).Contains(TEXT("IOS")));
    HardwareProfile.bIsConsole = FPlatformProperties::IsConsole();

    // RAM
    const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    HardwareProfile.RAM_GB = static_cast<float>(MemStats.TotalPhysical) / (1024.f * 1024.f * 1024.f);

    // VRAM: get adapter name from RHI; leave VRAM_GB = 0 as a safe cross-platform fallback.
    // The scoring formula (cores * 2 + RAM) is the primary signal; VRAM is additive when
    // a version-stable RHI query API is available in a future task.
    HardwareProfile.VRAM_GB = 0.f;
    if (GDynamicRHI)
    {
        HardwareProfile.AdapterName = FString(GDynamicRHI->RHIGetAdapterName());
    }

    HardwareProfile.bIsIntegratedGPU = IsIntegratedGPU(HardwareProfile.AdapterName);

    // Integrated GPU → UltraLowEnd regardless of CPU/RAM score
    if (HardwareProfile.bIsIntegratedGPU)
    {
        UE_LOG(LogMahlanyaHardware, Warning,
               TEXT("Integrated GPU detected (%s) — forcing UltraLowEnd tier. Nanite/Lumen disabled."),
               *HardwareProfile.AdapterName);
        return EHardwareTier::UltraLowEnd;
    }

    // Consoles: always Ultra
    if (HardwareProfile.bIsConsole)
    {
        return EHardwareTier::Ultra;
    }

    // Score: cores * 2 + RAM_GB + VRAM_GB
    const float Score = HardwareProfile.CoreCount * 2.f
                      + HardwareProfile.RAM_GB
                      + HardwareProfile.VRAM_GB;

    EHardwareTier Tier;
    if (Score >= 28.f)
        Tier = EHardwareTier::Ultra;
    else if (Score >= 16.f)
        Tier = EHardwareTier::HighEnd;
    else if (Score >= 8.f)
        Tier = EHardwareTier::MidRange;
    else
        Tier = EHardwareTier::LowEnd;

    // Mobile caps at MidRange regardless of score
    if (HardwareProfile.bIsMobile && Tier > EHardwareTier::MidRange)
    {
        Tier = EHardwareTier::MidRange;
    }

    return Tier;
}

FAdaptiveSimulationConfig UHardwareAdaptiveScaler::BuildConfigForTier(EHardwareTier Tier) const
{
    FAdaptiveSimulationConfig Cfg;
    switch (Tier)
    {
    case EHardwareTier::UltraLowEnd:
        Cfg.ReplicationUpdateFrequency = 5.f;
        Cfg.MaxReplicatedProperties    = 15;
        Cfg.SimulationTickBudgetMs     = 0.5f;
        Cfg.TrustUpdateRateHz          = 2.f;
        Cfg.MaxActiveNPCs              = 5;
        break;
    case EHardwareTier::LowEnd:
        Cfg.ReplicationUpdateFrequency = 10.f;
        Cfg.MaxReplicatedProperties    = 30;
        Cfg.SimulationTickBudgetMs     = 1.2f;
        Cfg.TrustUpdateRateHz          = 5.f;
        Cfg.MaxActiveNPCs              = 25;
        break;
    case EHardwareTier::MidRange:
        Cfg.ReplicationUpdateFrequency = 20.f;
        Cfg.MaxReplicatedProperties    = 60;
        Cfg.SimulationTickBudgetMs     = 2.0f;
        Cfg.TrustUpdateRateHz          = 10.f;
        Cfg.MaxActiveNPCs              = 50;
        break;
    case EHardwareTier::HighEnd:
        Cfg.ReplicationUpdateFrequency = 30.f;
        Cfg.MaxReplicatedProperties    = 150;
        Cfg.SimulationTickBudgetMs     = 3.0f;
        Cfg.TrustUpdateRateHz          = 15.f;
        Cfg.MaxActiveNPCs              = 100;
        break;
    case EHardwareTier::Ultra:
        Cfg.ReplicationUpdateFrequency = 60.f;
        Cfg.MaxReplicatedProperties    = 250;
        Cfg.SimulationTickBudgetMs     = 4.5f;
        Cfg.TrustUpdateRateHz          = 30.f;
        Cfg.MaxActiveNPCs              = 200;
        break;
    default:
        break;
    }
    return Cfg;
}

void UHardwareAdaptiveScaler::WriteCVarsForTier(EHardwareTier Tier)
{
    IConsoleManager& CM = IConsoleManager::Get();

    auto SetInt = [&](const TCHAR* Name, int32 Val)
    {
        if (IConsoleVariable* CVar = CM.FindConsoleVariable(Name))
            CVar->Set(Val, ECVF_SetByCode);
    };
    auto SetFloat = [&](const TCHAR* Name, float Val)
    {
        if (IConsoleVariable* CVar = CM.FindConsoleVariable(Name))
            CVar->Set(Val, ECVF_SetByCode);
    };

    switch (Tier)
    {
    case EHardwareTier::UltraLowEnd:
        SetInt  (CVar_RuntimeErosion,  0);
        SetInt  (CVar_RuntimeVoronoi,  0);
        SetInt  (CVar_AudioRayCount,   0);
        SetInt  (CVar_AudioMaxBounces, 0);
        SetFloat(CVar_DrawDistance,    0.5f);
        // Renderer downgrades — disable all UE5 advanced features that require DX12
        SetInt  (TEXT("r.Nanite"),                   0);
        SetInt  (TEXT("r.Lumen.Enabled"),            0);
        SetInt  (TEXT("r.VolumetricCloud"),          0);
        SetInt  (TEXT("r.SkyAtmosphere"),            0);
        SetFloat(TEXT("r.ScreenPercentage"),        50.f);
        SetInt  (TEXT("r.BloomQuality"),             0);
        SetInt  (TEXT("r.DepthOfFieldQuality"),      0);
        SetInt  (TEXT("r.MotionBlurQuality"),        0);
        SetInt  (TEXT("r.AmbientOcclusionLevels"),   0);
        SetInt  (TEXT("r.MaterialQualityLevel"),     0);
        SetInt  (TEXT("r.ReflectionCaptureResolution"), 64);
        SetInt  (TEXT("sg.ShadowQuality"),           0);
        SetInt  (TEXT("sg.TextureQuality"),          0);
        SetInt  (TEXT("sg.EffectsQuality"),          0);
        SetInt  (TEXT("sg.PostProcessQuality"),      0);
        break;
    case EHardwareTier::LowEnd:
        // Simulation CVars
        SetInt  (CVar_RuntimeErosion,  0);
        SetInt  (CVar_RuntimeVoronoi,  0);
        SetInt  (CVar_AudioRayCount,   0);
        SetInt  (CVar_AudioMaxBounces, 0);
        SetFloat(CVar_DrawDistance,    1.0f);
        // Renderer
        SetInt  (TEXT("r.Nanite"),                   0);
        SetInt  (TEXT("r.Lumen.Enabled"),            0);
        SetInt  (TEXT("r.VolumetricCloud"),          0);
        SetInt  (TEXT("r.SkyAtmosphere"),            1);
        SetInt  (TEXT("r.DynamicRes.Enabled"),       1);
        SetFloat(TEXT("r.DynamicRes.TargetedScreenPercentage"), 60.f);
        SetFloat(TEXT("r.DynamicRes.MinScreenPercentage"),      50.f);
        SetFloat(TEXT("t.MaxFPS"),                  30.f);
        SetInt  (TEXT("r.BloomQuality"),             0);
        SetInt  (TEXT("r.DepthOfFieldQuality"),      0);
        SetInt  (TEXT("r.MotionBlurQuality"),        0);
        SetInt  (TEXT("r.AmbientOcclusionLevels"),   0);
        SetInt  (TEXT("r.MaterialQualityLevel"),     0);
        SetInt  (TEXT("r.ReflectionCaptureResolution"), 128);
        SetInt  (TEXT("sg.ShadowQuality"),           0);
        SetInt  (TEXT("sg.TextureQuality"),          1);
        SetInt  (TEXT("sg.EffectsQuality"),          0);
        SetInt  (TEXT("sg.PostProcessQuality"),      0);
        SetFloat(TEXT("r.Streaming.MipBias"),        2.f);
        break;
    case EHardwareTier::MidRange:
        // Simulation CVars
        SetInt  (CVar_RuntimeErosion,  0);
        SetInt  (CVar_RuntimeVoronoi,  0);
        SetInt  (CVar_AudioRayCount,   0);
        SetInt  (CVar_AudioMaxBounces, 0);
        SetFloat(CVar_DrawDistance,    2.0f);
        // Renderer
        SetInt  (TEXT("r.Nanite"),                   1);
        SetInt  (TEXT("r.Lumen.Enabled"),            0);  // Lumen too expensive for mid
        SetInt  (TEXT("r.VolumetricCloud"),          0);
        SetInt  (TEXT("r.SkyAtmosphere"),            1);
        SetInt  (TEXT("r.DynamicRes.Enabled"),       1);
        SetFloat(TEXT("r.DynamicRes.TargetedScreenPercentage"), 78.f);
        SetFloat(TEXT("r.DynamicRes.MinScreenPercentage"),      60.f);
        SetFloat(TEXT("t.MaxFPS"),                  60.f);
        SetInt  (TEXT("r.BloomQuality"),             2);
        SetInt  (TEXT("r.DepthOfFieldQuality"),      0);
        SetInt  (TEXT("r.MotionBlurQuality"),        1);
        SetInt  (TEXT("r.AmbientOcclusionLevels"),   1);
        SetInt  (TEXT("r.MaterialQualityLevel"),     1);
        SetInt  (TEXT("r.ReflectionCaptureResolution"), 256);
        SetInt  (TEXT("sg.ShadowQuality"),           1);
        SetInt  (TEXT("sg.TextureQuality"),          1);
        SetInt  (TEXT("sg.EffectsQuality"),          1);
        SetInt  (TEXT("sg.PostProcessQuality"),      1);
        SetFloat(TEXT("r.Streaming.MipBias"),        1.f);
        break;
    case EHardwareTier::HighEnd:
        // Simulation CVars
        SetInt  (CVar_RuntimeErosion,  1);
        SetInt  (CVar_RuntimeVoronoi,  1);
        SetInt  (CVar_AudioRayCount,   32);
        SetInt  (CVar_AudioMaxBounces, 4);
        SetFloat(CVar_DrawDistance,    6.0f);
        // Renderer
        SetInt  (TEXT("r.Nanite"),                   1);
        SetInt  (TEXT("r.Lumen.Enabled"),            1);
        SetInt  (TEXT("r.VolumetricCloud"),          1);
        SetInt  (TEXT("r.SkyAtmosphere"),            1);
        SetInt  (TEXT("r.DynamicRes.Enabled"),       1);
        SetFloat(TEXT("r.DynamicRes.TargetedScreenPercentage"), 90.f);
        SetFloat(TEXT("r.DynamicRes.MinScreenPercentage"),      75.f);
        SetFloat(TEXT("t.MaxFPS"),                  60.f);
        SetInt  (TEXT("r.BloomQuality"),             4);
        SetInt  (TEXT("r.DepthOfFieldQuality"),      1);
        SetInt  (TEXT("r.MotionBlurQuality"),        2);
        SetInt  (TEXT("r.AmbientOcclusionLevels"),   2);
        SetInt  (TEXT("r.MaterialQualityLevel"),     1);
        SetInt  (TEXT("r.ReflectionCaptureResolution"), 512);
        SetInt  (TEXT("sg.ShadowQuality"),           2);
        SetInt  (TEXT("sg.TextureQuality"),          2);
        SetInt  (TEXT("sg.EffectsQuality"),          2);
        SetInt  (TEXT("sg.PostProcessQuality"),      2);
        SetFloat(TEXT("r.Streaming.MipBias"),        0.f);
        break;
    case EHardwareTier::Ultra:
        // Simulation CVars
        SetInt  (CVar_RuntimeErosion,  1);
        SetInt  (CVar_RuntimeVoronoi,  1);
        SetInt  (CVar_AudioRayCount,   64);
        SetInt  (CVar_AudioMaxBounces, 6);
        SetFloat(CVar_DrawDistance,    8.0f);
        // Renderer
        SetInt  (TEXT("r.Nanite"),                   1);
        SetInt  (TEXT("r.Lumen.Enabled"),            1);
        SetInt  (TEXT("r.VolumetricCloud"),          1);
        SetInt  (TEXT("r.SkyAtmosphere"),            1);
        SetInt  (TEXT("r.DynamicRes.Enabled"),       0);  // Static 100% — no need to scale
        SetFloat(TEXT("r.ScreenPercentage"),       100.f);
        SetFloat(TEXT("t.MaxFPS"),                144.f);
        SetInt  (TEXT("r.BloomQuality"),             5);
        SetInt  (TEXT("r.DepthOfFieldQuality"),      4);
        SetInt  (TEXT("r.MotionBlurQuality"),        4);
        SetInt  (TEXT("r.AmbientOcclusionLevels"),   4);
        SetInt  (TEXT("r.MaterialQualityLevel"),     1);
        SetInt  (TEXT("r.ReflectionCaptureResolution"), 1024);
        SetInt  (TEXT("sg.ShadowQuality"),           3);
        SetInt  (TEXT("sg.TextureQuality"),          3);
        SetInt  (TEXT("sg.EffectsQuality"),          3);
        SetInt  (TEXT("sg.PostProcessQuality"),      3);
        SetFloat(TEXT("r.Streaming.MipBias"),        0.f);
        break;
    default:
        break;
    }
}

void UHardwareAdaptiveScaler::ApplyDeviceProfileOverride()
{
    // Stub: will be fully wired in Task 10.2 when USimulationDeviceSettings is created.
    // For now, log the active device profile name for diagnostics.
    const UDeviceProfileManager* DPMgr = UDeviceProfileManager::TryGet();
    if (!DPMgr)
        return;

    const UDeviceProfile* ActiveProfile = DPMgr->GetActiveProfile();
    if (!ActiveProfile)
        return;

    UE_LOG(LogMahlanyaHardware, Verbose,
           TEXT("Active device profile: %s"), *ActiveProfile->GetName());
}

void UHardwareAdaptiveScaler::DynamicApplyConfig(const FAdaptiveSimulationConfig& NewConfig)
{
    // The throttle system computes the reduced config externally and passes it in.
    // Simply store it — orchestrators and replication managers poll GetSimulationConfig().
    // The 5 simulation CVars were written at tier-detection time and are not re-written
    // here; they reflect hardware capability, not session throttle state.
    CurrentConfig = NewConfig;
}
