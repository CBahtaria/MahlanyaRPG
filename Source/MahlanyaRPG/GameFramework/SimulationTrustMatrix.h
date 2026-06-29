// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "SimulationTrustMatrix.generated.h"

UENUM(BlueprintType)
enum class EValidationSeverity : uint8
{
    Valid        = 0,   // Server data received this tick
    Extrapolated = 1,   // Linearly extrapolated from last known value
    Degraded     = 2,   // Extrapolation diverging; prediction error rising
    Stale        = 3,   // No update for an extended period
    Invalid      = 4    // Trust expired; value unsafe to use
};

USTRUCT(BlueprintType)
struct MAHLANYARPG_API FSimulationPropertyState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName PropertyName;

    UPROPERTY(BlueprintReadOnly)
    float LastKnownValue = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float ExtrapolatedValue = 0.f;

    UPROPERTY(BlueprintReadOnly)
    int32 LastServerTick = 0;

    UPROPERTY(BlueprintReadOnly)
    float PredictionError = 0.f;

    UPROPERTY(BlueprintReadOnly)
    EValidationSeverity Severity = EValidationSeverity::Valid;

    // Returns a 0–1 trust factor based on how many ticks have elapsed since
    // the last server update, using exponential decay at DecayRate per tick.
    float CalculateTrustFactor(int32 CurrentTick, float DecayRate) const;

    // Returns true when the gap from LastServerTick to CurrentTick is within
    // MaxTickDrift, making linear extrapolation safe.
    bool IsSafeToExtrapolate(int32 CurrentTick, int32 MaxTickDrift) const;
};

/**
 * Tracks trust state for all replicated simulation properties on the client.
 * Thread-safe: UpdateProperty() is called from the network thread (RPC delivery);
 * ExtrapolateProperties() and GetOverallTrustScore() are called from the game thread.
 * All public methods acquire CriticalSection internally.
 */
USTRUCT(BlueprintType)
struct MAHLANYARPG_API FSimulationTrustMatrix
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TMap<FName, FSimulationPropertyState> PropertyStates;

    UPROPERTY(BlueprintReadOnly)
    int32 LastFullSyncTick = 0;

    UPROPERTY(BlueprintReadOnly)
    float OverallTrustScore = 1.f;

    // Called from the network thread when an RPC arrives with fresh server data.
    void UpdateProperty(FName PropertyName, float ServerValue, int32 ServerTick);

    // Called from the game thread each Tick to advance extrapolated values
    // and update severity/trust for all tracked properties.
    void ExtrapolateProperties(int32 CurrentTick, float DeltaTime);

    // Thread-safe read of OverallTrustScore (acquires lock).
    float GetOverallTrustScore() const;

private:
    // FCriticalSection is not a UPROPERTY — it is not UE4-reflected.
    mutable FCriticalSection CriticalSection;

    // Recalculates OverallTrustScore from current property severities.
    // MUST be called while CriticalSection is already locked.
    void RecalculateTrustScore_Locked();

    static constexpr float SeverityWeight_Valid        = 1.0f;
    static constexpr float SeverityWeight_Extrapolated = 0.7f;
    static constexpr float SeverityWeight_Degraded     = 0.4f;
    static constexpr float SeverityWeight_Stale        = 0.1f;
    static constexpr float SeverityWeight_Invalid      = 0.0f;
};
