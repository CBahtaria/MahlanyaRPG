#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UHistoricalCalendarSubsystem.generated.h"

// Historical events fire in sequence via BFS dependency resolution
USTRUCT(BlueprintType)
struct EMERGENTNARRATIVEPLUGIN_API FHistoricalEventNode
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName EventID;
    UPROPERTY(BlueprintReadOnly) int32 Year = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FName> Prerequisites;
    UPROPERTY(BlueprintReadOnly) float NarrativeWeight = 1.f;
    UPROPERTY(BlueprintReadOnly) bool bFired = false;
};

UCLASS()
class EMERGENTNARRATIVEPLUGIN_API UHistoricalCalendarSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Advance the in-game date by GameDayDelta days. Fires events when year threshold crossed.
    UFUNCTION(BlueprintCallable, Category="Historical Calendar")
    void AdvanceTime(float GameDayDelta);

    // Set the starting in-game year (default 1750).
    UFUNCTION(BlueprintCallable, Category="Historical Calendar")
    void SetStartYear(int32 Year);

    UFUNCTION(BlueprintCallable, Category="Historical Calendar")
    int32 GetCurrentYear() const { return CurrentYear; }

    UFUNCTION(BlueprintCallable, Category="Historical Calendar")
    float GetCurrentDayOfYear() const { return DayOfYear; }

    // Returns true if the event with EventID has fired already.
    UFUNCTION(BlueprintCallable, Category="Historical Calendar")
    bool HasEventFired(FName EventID) const;

    // Returns all events eligible to fire at current year (prerequisites met, not yet fired).
    UFUNCTION(BlueprintCallable, Category="Historical Calendar")
    TArray<FHistoricalEventNode> GetPendingEvents() const;

private:
    int32 CurrentYear = 1750;
    float DayOfYear = 0.f;  // 0-365
    TArray<FHistoricalEventNode> EventChain;
    TSet<FName> FiredEvents;

    void RegisterHistoricalEvents();
    void TryFireEvents();
    bool PrerequisitesMet(const FHistoricalEventNode& Node) const;
};
