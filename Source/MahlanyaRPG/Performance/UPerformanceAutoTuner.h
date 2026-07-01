// Copyright Charles Bartaria. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "UHardwareAdaptiveScaler.h"
#include "UPerformanceAutoTuner.generated.h"

UENUM(BlueprintType)
enum class EAutoTuneDirection : uint8 { Stable, Reducing, Recovering };

UCLASS()
class MAHLANYARPG_API UPerformanceAutoTuner : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    virtual bool IsTickableInEditor() const override { return false; }
    virtual bool IsTickableWhenPaused() const override { return false; }

    UFUNCTION(BlueprintPure, Category="Mahlanya|Performance")
    float GetCurrentFPS() const { return CurrentFPS; }

    UFUNCTION(BlueprintPure, Category="Mahlanya|Performance")
    EAutoTuneDirection GetTuneDirection() const { return TuneDirection; }

    UFUNCTION(BlueprintPure, Category="Mahlanya|Performance")
    int32 GetCurrentShadowQuality() const { return CurrentShadowQuality; }

    UFUNCTION(BlueprintPure, Category="Mahlanya|Performance")
    float GetCurrentScreenPercentage() const { return CurrentScreenPct; }

private:
    TArray<float> FrameTimeHistory;   // rolling 120 samples
    float EvalAccumulator = 0.f;
    float TargetFPS = 60.f;
    float CurrentFPS = 60.f;
    EAutoTuneDirection TuneDirection = EAutoTuneDirection::Stable;

    int32 CurrentShadowQuality = 1;
    int32 MinShadowQuality = 0;
    int32 MaxShadowQuality = 3;

    float CurrentScreenPct = 80.f;
    float MinScreenPct = 50.f;
    float MaxScreenPct = 100.f;

    int32 ReduceConsecutive = 0;
    int32 RecoverConsecutive = 0;

    static constexpr int32 ReduceThreshold = 3;    // 3 bad checks → reduce
    static constexpr int32 RecoverThreshold = 15;  // 15 good checks → try recover
    static constexpr float EvalIntervalSec = 1.0f;

    void EvaluateAndAdjust();
    void ReduceOneQualityStep();
    void RecoverOneQualityStep();
    void ApplyShadowQuality(int32 Quality);
    void ApplyScreenPercentage(float Pct);
    void SetTargetFPSFromTier(EHardwareTier Tier);
    void SetBoundsFromTier(EHardwareTier Tier);
};
