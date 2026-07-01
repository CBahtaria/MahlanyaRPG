// Copyright Charles Bartaria. All Rights Reserved.

#include "UProximityLODSubsystem.h"
#include "MahlanyaPerformanceCVars.h"
#include "Core/MahlanyaLogChannels.h"
#include "UHardwareAdaptiveScaler.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/StaticMesh.h"

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UProximityLODSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (MahlanyaPerformanceCVars::ProximityLODEnabled.GetValueOnGameThread() == 0)
    {
        bSubsystemEnabled = false;
        return;
    }

    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        if (UHardwareAdaptiveScaler* Scaler = GI->GetSubsystem<UHardwareAdaptiveScaler>())
        {
            EHardwareTier Tier = Scaler->GetDetectedTier();
            // Override CVars for tier — tighter focus on lower-end hardware
            IConsoleManager& CM = IConsoleManager::Get();
            auto SetCVar = [&](const TCHAR* Name, float Val) {
                if (IConsoleVariable* V = CM.FindConsoleVariable(Name))
                    V->Set(Val, ECVF_SetByCode);
            };
            switch (Tier)
            {
            case EHardwareTier::UltraLowEnd:
                SetCVar(TEXT("mahlanya.ProximityLOD.FocusConeAngle"),        15.f);
                SetCVar(TEXT("mahlanya.ProximityLOD.BackgroundDotThreshold"), 0.0f);
                SetCVar(TEXT("mahlanya.ProximityLOD.EvalIntervalSeconds"),    0.05f);
                break;
            case EHardwareTier::LowEnd:
                SetCVar(TEXT("mahlanya.ProximityLOD.FocusConeAngle"),        20.f);
                SetCVar(TEXT("mahlanya.ProximityLOD.BackgroundDotThreshold"),-0.1f);
                SetCVar(TEXT("mahlanya.ProximityLOD.EvalIntervalSeconds"),   0.08f);
                break;
            case EHardwareTier::HighEnd:
                SetCVar(TEXT("mahlanya.ProximityLOD.FocusConeAngle"),        30.f);
                SetCVar(TEXT("mahlanya.ProximityLOD.BackgroundDotThreshold"),-0.4f);
                break;
            case EHardwareTier::Ultra:
                SetCVar(TEXT("mahlanya.ProximityLOD.FocusConeAngle"),        35.f);
                SetCVar(TEXT("mahlanya.ProximityLOD.BackgroundDotThreshold"),-0.5f);
                break;
            default: break; // MidRange uses defaults
            }
        }
    }

    UE_LOG(LogMahlanyaPerformance, Verbose, TEXT("ProximityLOD subsystem initialized"));
}

void UProximityLODSubsystem::Deinitialize()
{
    RestoreAllComponents();
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// FTickableGameObject
// ─────────────────────────────────────────────────────────────────────────────

bool UProximityLODSubsystem::IsTickable() const
{
    return bSubsystemEnabled && !IsTemplate();
}

TStatId UProximityLODSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UProximityLODSubsystem, STATGROUP_Tickables);
}

void UProximityLODSubsystem::Tick(float DeltaTime)
{
    if (!bSubsystemEnabled)
        return;

    EvalAccumulator += DeltaTime;

    const float Interval = MahlanyaPerformanceCVars::ProximityLODInterval.GetValueOnGameThread();
    if (EvalAccumulator >= Interval)
    {
        EvaluateAll();
        EvalAccumulator = 0.f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void UProximityLODSubsystem::SetInteractionFocus(UPrimitiveComponent* Component, bool bFocused)
{
    if (!Component)
        return;

    TWeakObjectPtr<UPrimitiveComponent> WeakComp(Component);
    if (bFocused)
    {
        PinnedFocusComponents.Add(WeakComp);
    }
    else
    {
        PinnedFocusComponents.Remove(WeakComp);
    }
}

EProximityLODTier UProximityLODSubsystem::GetComponentTier(UPrimitiveComponent* Component) const
{
    if (!Component)
        return EProximityLODTier::Visible;

    const EProximityLODTier* Found = TierCache.Find(TWeakObjectPtr<UPrimitiveComponent>(Component));
    return Found ? *Found : EProximityLODTier::Visible;
}

// ─────────────────────────────────────────────────────────────────────────────
// Core evaluation
// ─────────────────────────────────────────────────────────────────────────────

void UProximityLODSubsystem::EvaluateAll()
{
    UWorld* World = GetWorld();
    if (!World)
        return;

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC)
        return;

    FVector  ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const FVector ViewForward = ViewRotation.Vector();

    const float FocusConeAngle = MahlanyaPerformanceCVars::ProximityLODFocusConeAngle.GetValueOnGameThread();
    const float FocusCosAngle  = FMath::Cos(FMath::DegreesToRadians(FocusConeAngle));

    // Identify the player pawn so we can skip its own components
    APawn* PlayerPawn = PC->GetPawn();

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!Actor || Actor == PlayerPawn)
            continue;

        // ── Static mesh components ────────────────────────────────────────
        TArray<UStaticMeshComponent*> SMCs;
        Actor->GetComponents<UStaticMeshComponent>(SMCs);
        for (UStaticMeshComponent* SMC : SMCs)
        {
            if (!SMC || SMC->bIsEditorOnly)
                continue;

            const FVector CompLocation = SMC->GetComponentLocation();
            EProximityLODTier Tier = ClassifyComponent(CompLocation, ViewLocation, ViewForward, FocusCosAngle);

            // Interaction-pinned components always use Focus
            if (PinnedFocusComponents.Contains(TWeakObjectPtr<UPrimitiveComponent>(SMC)))
                Tier = EProximityLODTier::Focus;

            TWeakObjectPtr<UPrimitiveComponent> WeakSMC(SMC);
            const EProximityLODTier* Cached = TierCache.Find(WeakSMC);
            if (!Cached || *Cached != Tier)
            {
                ApplyTierToStaticMesh(SMC, Tier);
                TierCache.Add(WeakSMC, Tier);
            }
        }

        // ── Skeletal mesh components ──────────────────────────────────────
        TArray<USkeletalMeshComponent*> SKCs;
        Actor->GetComponents<USkeletalMeshComponent>(SKCs);
        for (USkeletalMeshComponent* SKC : SKCs)
        {
            if (!SKC || SKC->bIsEditorOnly)
                continue;

            const FVector CompLocation = SKC->GetComponentLocation();
            EProximityLODTier Tier = ClassifyComponent(CompLocation, ViewLocation, ViewForward, FocusCosAngle);

            if (PinnedFocusComponents.Contains(TWeakObjectPtr<UPrimitiveComponent>(SKC)))
                Tier = EProximityLODTier::Focus;

            TWeakObjectPtr<UPrimitiveComponent> WeakSKC(SKC);
            const EProximityLODTier* Cached = TierCache.Find(WeakSKC);
            if (!Cached || *Cached != Tier)
            {
                ApplyTierToSkeletalMesh(SKC, Tier);
                TierCache.Add(WeakSKC, Tier);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Classification
// ─────────────────────────────────────────────────────────────────────────────

EProximityLODTier UProximityLODSubsystem::ClassifyComponent(
    const FVector& CompLocation, const FVector& ViewOrigin,
    const FVector& ViewForward, float FocusCosAngle) const
{
    FVector ToComp = (CompLocation - ViewOrigin);
    float Dist = ToComp.Size();
    if (Dist < 1.f) return EProximityLODTier::Focus; // too close to classify
    ToComp /= Dist;
    float Dot = FVector::DotProduct(ViewForward, ToComp);
    if (Dot >= FocusCosAngle) return EProximityLODTier::Focus;
    if (Dot > -0.2f)          return EProximityLODTier::Visible;
    return EProximityLODTier::Background;
}

// ─────────────────────────────────────────────────────────────────────────────
// Tier application
// ─────────────────────────────────────────────────────────────────────────────

void UProximityLODSubsystem::ApplyTierToStaticMesh(UStaticMeshComponent* SMC, EProximityLODTier Tier)
{
    if (!SMC || !SMC->GetStaticMesh()) return;
    switch (Tier)
    {
    case EProximityLODTier::Focus:
        SMC->ForcedLodModel = 1;   // Force LOD0 (UE5: 0=auto, 1=LOD0, 2=LOD1...)
        SMC->SetCastShadow(true);
        break;
    case EProximityLODTier::Visible:
        SMC->ForcedLodModel = 0;   // Auto distance-based LOD
        SMC->SetCastShadow(true);
        break;
    case EProximityLODTier::Background:
        {
            const int32 NumLODs = SMC->GetStaticMesh()->GetNumLODs();
            SMC->ForcedLodModel = NumLODs; // Force lowest-detail LOD
            SMC->SetCastShadow(false);
        }
        break;
    }
}

void UProximityLODSubsystem::ApplyTierToSkeletalMesh(USkeletalMeshComponent* SKC, EProximityLODTier Tier)
{
    if (!SKC) return;
    switch (Tier)
    {
    case EProximityLODTier::Focus:
        SKC->SetVisibilityBasedAnimTickOption(
            EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones);
        SKC->ForcedLodModel = 1;
        SKC->SetCastShadow(true);
        break;
    case EProximityLODTier::Visible:
        SKC->SetVisibilityBasedAnimTickOption(
            EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered);
        SKC->ForcedLodModel = 0;
        SKC->SetCastShadow(true);
        break;
    case EProximityLODTier::Background:
        SKC->SetVisibilityBasedAnimTickOption(
            EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered);
        if (SKC->GetSkeletalMeshAsset())
        {
            const int32 NumLODs = SKC->GetSkeletalMeshAsset()->GetLODNum();
            SKC->ForcedLodModel = NumLODs;
        }
        SKC->SetCastShadow(false);
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Restore on shutdown
// ─────────────────────────────────────────────────────────────────────────────

void UProximityLODSubsystem::RestoreAllComponents()
{
    for (auto& Pair : TierCache)
    {
        UPrimitiveComponent* Comp = Pair.Key.Get();
        if (!Comp)
            continue;

        if (UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp))
        {
            SMC->ForcedLodModel = 0;
            SMC->SetCastShadow(true);
        }
        else if (USkeletalMeshComponent* SKC = Cast<USkeletalMeshComponent>(Comp))
        {
            SKC->ForcedLodModel = 0;
            SKC->SetCastShadow(true);
            SKC->SetVisibilityBasedAnimTickOption(
                EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones);
        }
    }

    TierCache.Empty();
    PinnedFocusComponents.Empty();
}
