// Copyright Charles Bartaria. All Rights Reserved.

#include "UDynamicRuntimeThrottle.h"
#include "MahlanyaPerformanceCVars.h"
#include "Core/MahlanyaLogChannels.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"

extern ENGINE_API float GGameThreadTime;   // UE core timing — ms spent on game thread last frame

void UDynamicRuntimeThrottle::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FrameTimeHistory.Reserve(HistoryCapacity);

    const float Interval =
        MahlanyaPerformanceCVars::ThrottleEvaluationInterval.GetValueOnGameThread();

    GetWorld()->GetTimerManager().SetTimer(
        EvalTimer,
        this,
        &UDynamicRuntimeThrottle::EvaluateThrottle,
        Interval,
        /*bLoop=*/ true);

    UE_LOG(LogMahlanyaThrottle, Log,
           TEXT("Dynamic runtime throttle initialized. Eval interval=%.2fs"), Interval);
}

void UDynamicRuntimeThrottle::Deinitialize()
{
    if (GetWorld())
        GetWorld()->GetTimerManager().ClearTimer(EvalTimer);

    Super::Deinitialize();
}

void UDynamicRuntimeThrottle::EvaluateThrottle()
{
    // Sample current game-thread frame time (milliseconds).
    const float CurrentFrameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);

    // Maintain rolling history (ring buffer without allocations after initial reserve).
    if (FrameTimeHistory.Num() >= HistoryCapacity)
        FrameTimeHistory.RemoveAt(0, 1, /*bAllowShrinking=*/false);
    FrameTimeHistory.Add(CurrentFrameMs);

    if (FrameTimeHistory.IsEmpty())
        return;

    // Compute rolling average frame time.
    float Sum = 0.f;
    for (float T : FrameTimeHistory)
        Sum += T;
    const float AvgMs = Sum / static_cast<float>(FrameTimeHistory.Num());

    // Retrieve the tick budget from the hardware scaler.
    float BudgetMs = 2.0f;  // sensible default before scaler is available
    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        if (UHardwareAdaptiveScaler* Scaler =
                GI->GetSubsystem<UHardwareAdaptiveScaler>())
        {
            BudgetMs = Scaler->GetSimulationConfig().SimulationTickBudgetMs;
        }
    }

    const float UsageFraction = AvgMs / FMath::Max(BudgetMs, 0.001f);
    const float Headroom      = 1.f - UsageFraction;

    // Determine the target throttle state.
    EThrottleState TargetState;
    if (UsageFraction >= 0.95f)
        TargetState = EThrottleState::Emergency;
    else if (UsageFraction >= 0.85f)
        TargetState = EThrottleState::Aggressive;
    else if (UsageFraction >= 0.70f)
        TargetState = EThrottleState::Moderate;
    else
        TargetState = EThrottleState::Normal;

    // Hysteresis: escalate immediately; de-escalate only after sustained improvement.
    if (TargetState > CurrentState)
    {
        ConsecutiveFramesBelowThreshold = 0;
        CurrentState = TargetState;
    }
    else if (TargetState < CurrentState)
    {
        ++ConsecutiveFramesBelowThreshold;
        if (ConsecutiveFramesBelowThreshold < HysteresisFrameCount)
            return;  // not yet — keep current state
        ConsecutiveFramesBelowThreshold = 0;
        CurrentState = TargetState;
    }
    else
    {
        ConsecutiveFramesBelowThreshold = 0;
        return;  // same state, nothing to broadcast
    }

    // Build a throttled config and apply it via the scaler.
    UGameInstance* GI = GetWorld()->GetGameInstance();
    UHardwareAdaptiveScaler* Scaler =
        GI ? GI->GetSubsystem<UHardwareAdaptiveScaler>() : nullptr;

    if (Scaler)
    {
        FAdaptiveSimulationConfig Cfg = Scaler->GetSimulationConfig();
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

        switch (CurrentState)
        {
        case EThrottleState::Moderate:
            Cfg.ReplicationUpdateFrequency *= 0.7f;
            Cfg.MaxActiveNPCs = FMath::FloorToInt(Cfg.MaxActiveNPCs * 0.8f);
            break;
        case EThrottleState::Aggressive:
            Cfg.ReplicationUpdateFrequency *= 0.5f;
            Cfg.MaxActiveNPCs = FMath::FloorToInt(Cfg.MaxActiveNPCs * 0.5f);
            SetFloat(TEXT("r.ScreenPercentage"), 65.f);
            SetInt  (TEXT("sg.ShadowQuality"),   0);
            break;
        case EThrottleState::Emergency:
            Cfg.ReplicationUpdateFrequency = 5.f;
            Cfg.MaxActiveNPCs = 10;
            SetFloat(TEXT("r.ScreenPercentage"), 50.f);
            SetInt  (TEXT("sg.ShadowQuality"),   0);
            SetInt  (TEXT("sg.EffectsQuality"),  0);
            SetInt  (TEXT("r.BloomQuality"),     0);
            break;
        case EThrottleState::Normal:
        default:
            // Restore rendering CVars to tier defaults
            if (UHardwareAdaptiveScaler* TierScaler = Scaler)
                TierScaler->WriteCVarsForCurrentTier();
            break;
        }
        Scaler->DynamicApplyConfig(Cfg);
    }

    UE_LOG(LogMahlanyaThrottle, Warning,
           TEXT("Throttle state → %d (avg %.1fms / budget %.1fms, headroom %.0f%%)"),
           (int32)CurrentState, AvgMs, BudgetMs, Headroom * 100.f);

    OnThrottleStateChanged.Broadcast(CurrentState, Headroom);
}
