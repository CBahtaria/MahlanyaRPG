// Copyright Charles Bartaria. All Rights Reserved.

#include "SimulationTrustMatrix.h"
#include "HAL/CriticalSection.h"

// ── FSimulationPropertyState ─────────────────────────────────────────────────

float FSimulationPropertyState::CalculateTrustFactor(
    int32 CurrentTick, float DecayRate) const
{
    const int32 TicksElapsed = FMath::Max(0, CurrentTick - LastServerTick);
    return FMath::Exp(-DecayRate * static_cast<float>(TicksElapsed));
}

bool FSimulationPropertyState::IsSafeToExtrapolate(
    int32 CurrentTick, int32 MaxTickDrift) const
{
    return (CurrentTick - LastServerTick) <= MaxTickDrift;
}

// ── FSimulationTrustMatrix ───────────────────────────────────────────────────

void FSimulationTrustMatrix::UpdateProperty(
    FName PropertyName, float ServerValue, int32 ServerTick)
{
    FScopeLock Lock(&CriticalSection);

    FSimulationPropertyState& State = PropertyStates.FindOrAdd(PropertyName);

    // Track prediction error before overwriting the extrapolated value
    if (State.LastServerTick > 0)
    {
        State.PredictionError = FMath::Abs(State.ExtrapolatedValue - ServerValue);
    }

    State.PropertyName       = PropertyName;
    State.LastKnownValue     = ServerValue;
    State.ExtrapolatedValue  = ServerValue;
    State.LastServerTick     = ServerTick;
    State.Severity           = EValidationSeverity::Valid;

    LastFullSyncTick = FMath::Max(LastFullSyncTick, ServerTick);

    RecalculateTrustScore_Locked();
}

void FSimulationTrustMatrix::ExtrapolateProperties(int32 CurrentTick, float DeltaTime)
{
    FScopeLock Lock(&CriticalSection);

    static constexpr int32 DegradedTickThreshold = 10;
    static constexpr int32 StaleTickThreshold    = 30;
    static constexpr int32 InvalidTickThreshold  = 60;

    for (auto& Pair : PropertyStates)
    {
        FSimulationPropertyState& State = Pair.Value;
        const int32 TicksElapsed = CurrentTick - State.LastServerTick;

        // Advance the extrapolated value by holding last known (zeroth-order hold).
        // Higher-order extrapolation would require velocity tracking which adds complexity;
        // zeroth-order is safe and prevents wild divergence on noisy signals.
        State.ExtrapolatedValue = State.LastKnownValue;

        if (TicksElapsed == 0)
            State.Severity = EValidationSeverity::Valid;
        else if (TicksElapsed <= DegradedTickThreshold)
            State.Severity = EValidationSeverity::Extrapolated;
        else if (TicksElapsed <= StaleTickThreshold)
            State.Severity = EValidationSeverity::Degraded;
        else if (TicksElapsed <= InvalidTickThreshold)
            State.Severity = EValidationSeverity::Stale;
        else
            State.Severity = EValidationSeverity::Invalid;
    }

    RecalculateTrustScore_Locked();
}

float FSimulationTrustMatrix::GetOverallTrustScore() const
{
    FScopeLock Lock(&CriticalSection);
    return OverallTrustScore;
}

void FSimulationTrustMatrix::RecalculateTrustScore_Locked()
{
    // Caller must hold CriticalSection.
    if (PropertyStates.IsEmpty())
    {
        OverallTrustScore = 1.f;
        return;
    }

    float TotalWeight = 0.f;
    for (const auto& Pair : PropertyStates)
    {
        switch (Pair.Value.Severity)
        {
        case EValidationSeverity::Valid:        TotalWeight += SeverityWeight_Valid;        break;
        case EValidationSeverity::Extrapolated: TotalWeight += SeverityWeight_Extrapolated; break;
        case EValidationSeverity::Degraded:     TotalWeight += SeverityWeight_Degraded;     break;
        case EValidationSeverity::Stale:        TotalWeight += SeverityWeight_Stale;        break;
        case EValidationSeverity::Invalid:      TotalWeight += SeverityWeight_Invalid;      break;
        default:                                                                             break;
        }
    }

    OverallTrustScore = TotalWeight / static_cast<float>(PropertyStates.Num());
}
