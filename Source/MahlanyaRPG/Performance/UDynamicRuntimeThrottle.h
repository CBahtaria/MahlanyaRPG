// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UHardwareAdaptiveScaler.h"
#include "UDynamicRuntimeThrottle.generated.h"

UENUM(BlueprintType)
enum class EThrottleState : uint8
{
    Normal      = 0,  // < 70% of tick budget
    Moderate    = 1,  // 70–85%: reduce replication and audio rays
    Aggressive  = 2,  // 85–95%: zero out runtime audio, halve replication
    Emergency   = 3   // > 95%:  5Hz replication, 10 NPCs max
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThrottleStateChanged,
    EThrottleState, NewState, float, PerformanceHeadroom);

UCLASS()
class MAHLANYARPG_API UDynamicRuntimeThrottle : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(BlueprintAssignable, Category="Mahlanya|Performance")
    FOnThrottleStateChanged OnThrottleStateChanged;

    UFUNCTION(BlueprintPure, Category="Mahlanya|Performance")
    EThrottleState GetCurrentState() const { return CurrentState; }

private:
    void EvaluateThrottle();

    FTimerHandle EvalTimer;
    EThrottleState CurrentState = EThrottleState::Normal;
    TArray<float> FrameTimeHistory;

    // Hysteresis: de-escalate only after this many consecutive frames below threshold
    int32 ConsecutiveFramesBelowThreshold = 0;
    static constexpr int32 HysteresisFrameCount = 15;

    static constexpr int32 HistoryCapacity = 60;
};
