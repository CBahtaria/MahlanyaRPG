#include "UPerformanceAutoTuner.h"
#include "MahlanyaPerformanceCVars.h"
#include "Core/MahlanyaLogChannels.h"
#include "UHardwareAdaptiveScaler.h"
#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

// GGameThreadTime is in seconds (platform time units); convert to ms for frame time
// We use FApp::GetDeltaTime() which is readily available

void UPerformanceAutoTuner::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Read tier from HardwareAdaptiveScaler to set bounds and target
    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        if (UHardwareAdaptiveScaler* Scaler = GI->GetSubsystem<UHardwareAdaptiveScaler>())
        {
            EHardwareTier Tier = Scaler->GetDetectedTier();
            SetTargetFPSFromTier(Tier);
            SetBoundsFromTier(Tier);
        }
    }

    FrameTimeHistory.Reserve(120);
    UE_LOG(LogMahlanyaPerformance, Log,
        TEXT("PerformanceAutoTuner initialized. TargetFPS=%.0f ShadowRange=[%d,%d] ScreenPct=[%.0f,%.0f]"),
        TargetFPS, MinShadowQuality, MaxShadowQuality, MinScreenPct, MaxScreenPct);
}

void UPerformanceAutoTuner::Deinitialize()
{
    Super::Deinitialize();
    FrameTimeHistory.Empty();
}

bool UPerformanceAutoTuner::IsTickable() const
{
    return !IsTemplate() && GetWorld() && GetWorld()->IsGameWorld();
}

TStatId UPerformanceAutoTuner::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UPerformanceAutoTuner, STATGROUP_Tickables);
}

void UPerformanceAutoTuner::Tick(float DeltaTime)
{
    // Collect frame time sample
    if (FrameTimeHistory.Num() >= 120)
        FrameTimeHistory.RemoveAt(0);
    FrameTimeHistory.Add(DeltaTime * 1000.f); // ms

    EvalAccumulator += DeltaTime;
    if (EvalAccumulator >= EvalIntervalSec)
    {
        EvalAccumulator = 0.f;
        EvaluateAndAdjust();
    }
}

void UPerformanceAutoTuner::EvaluateAndAdjust()
{
    if (FrameTimeHistory.Num() < 10) return;

    float Sum = 0.f;
    for (float T : FrameTimeHistory) Sum += T;
    float AvgMs = Sum / FrameTimeHistory.Num();
    CurrentFPS = 1000.f / FMath::Max(AvgMs, 0.001f);

    const float TargetMs = 1000.f / TargetFPS;
    const float BadThreshold    = TargetMs * 1.15f;  // 15% over target = bad
    const float GoodThreshold   = TargetMs * 0.80f;  // 20% under target = good (headroom)

    if (AvgMs > BadThreshold)
    {
        ++ReduceConsecutive;
        RecoverConsecutive = 0;
        if (ReduceConsecutive >= ReduceThreshold)
        {
            ReduceConsecutive = 0;
            TuneDirection = EAutoTuneDirection::Reducing;
            ReduceOneQualityStep();
        }
    }
    else if (AvgMs < GoodThreshold)
    {
        ++RecoverConsecutive;
        ReduceConsecutive = 0;
        if (RecoverConsecutive >= RecoverThreshold)
        {
            RecoverConsecutive = 0;
            TuneDirection = EAutoTuneDirection::Recovering;
            RecoverOneQualityStep();
        }
    }
    else
    {
        ReduceConsecutive = 0;
        RecoverConsecutive = 0;
        TuneDirection = EAutoTuneDirection::Stable;
    }
}

void UPerformanceAutoTuner::ReduceOneQualityStep()
{
    // Priority: shadow → screen percentage → back to shadow
    if (CurrentShadowQuality > MinShadowQuality)
    {
        ApplyShadowQuality(CurrentShadowQuality - 1);
        UE_LOG(LogMahlanyaPerformance, Warning,
            TEXT("AutoTuner: FPS %.1f < target %.0f — reduced sg.ShadowQuality to %d"),
            CurrentFPS, TargetFPS, CurrentShadowQuality);
    }
    else if (CurrentScreenPct > MinScreenPct)
    {
        ApplyScreenPercentage(FMath::Max(CurrentScreenPct - 5.f, MinScreenPct));
        UE_LOG(LogMahlanyaPerformance, Warning,
            TEXT("AutoTuner: FPS %.1f — reduced r.ScreenPercentage to %.0f"),
            CurrentFPS, CurrentScreenPct);
    }
}

void UPerformanceAutoTuner::RecoverOneQualityStep()
{
    // Recover in reverse priority: screen percentage first, then shadow
    if (CurrentScreenPct < MaxScreenPct)
    {
        ApplyScreenPercentage(FMath::Min(CurrentScreenPct + 5.f, MaxScreenPct));
        UE_LOG(LogMahlanyaPerformance, Log,
            TEXT("AutoTuner: FPS %.1f — headroom, raised r.ScreenPercentage to %.0f"),
            CurrentFPS, CurrentScreenPct);
    }
    else if (CurrentShadowQuality < MaxShadowQuality)
    {
        ApplyShadowQuality(CurrentShadowQuality + 1);
        UE_LOG(LogMahlanyaPerformance, Log,
            TEXT("AutoTuner: FPS %.1f — headroom, raised sg.ShadowQuality to %d"),
            CurrentFPS, CurrentShadowQuality);
    }
}

void UPerformanceAutoTuner::ApplyShadowQuality(int32 Quality)
{
    CurrentShadowQuality = FMath::Clamp(Quality, MinShadowQuality, MaxShadowQuality);
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.ShadowQuality")))
        CVar->Set(CurrentShadowQuality, ECVF_SetByCode);
}

void UPerformanceAutoTuner::ApplyScreenPercentage(float Pct)
{
    CurrentScreenPct = FMath::Clamp(Pct, MinScreenPct, MaxScreenPct);
    if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
        CVar->Set(CurrentScreenPct, ECVF_SetByCode);
}

void UPerformanceAutoTuner::SetTargetFPSFromTier(EHardwareTier Tier)
{
    switch (Tier)
    {
    case EHardwareTier::UltraLowEnd: TargetFPS = 30.f; break;
    case EHardwareTier::LowEnd:      TargetFPS = 30.f; break;
    case EHardwareTier::MidRange:    TargetFPS = 60.f; break;
    case EHardwareTier::HighEnd:     TargetFPS = 60.f; break;
    case EHardwareTier::Ultra:       TargetFPS = 60.f; break; // Ultra aims for stable 60
    }
    CurrentFPS = TargetFPS;
}

void UPerformanceAutoTuner::SetBoundsFromTier(EHardwareTier Tier)
{
    switch (Tier)
    {
    case EHardwareTier::UltraLowEnd:
        MinShadowQuality = 0; MaxShadowQuality = 0;
        MinScreenPct = 40.f; MaxScreenPct = 60.f;
        CurrentShadowQuality = 0; CurrentScreenPct = 50.f;
        break;
    case EHardwareTier::LowEnd:
        MinShadowQuality = 0; MaxShadowQuality = 1;
        MinScreenPct = 50.f; MaxScreenPct = 75.f;
        CurrentShadowQuality = 0; CurrentScreenPct = 60.f;
        break;
    case EHardwareTier::MidRange:
        MinShadowQuality = 0; MaxShadowQuality = 2;
        MinScreenPct = 60.f; MaxScreenPct = 90.f;
        CurrentShadowQuality = 1; CurrentScreenPct = 78.f;
        break;
    case EHardwareTier::HighEnd:
        MinShadowQuality = 1; MaxShadowQuality = 3;
        MinScreenPct = 75.f; MaxScreenPct = 100.f;
        CurrentShadowQuality = 2; CurrentScreenPct = 90.f;
        break;
    case EHardwareTier::Ultra:
        MinShadowQuality = 2; MaxShadowQuality = 3;
        MinScreenPct = 90.f; MaxScreenPct = 100.f;
        CurrentShadowQuality = 3; CurrentScreenPct = 100.f;
        break;
    }
}
