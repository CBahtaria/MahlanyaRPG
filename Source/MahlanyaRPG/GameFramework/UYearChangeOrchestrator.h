// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "UHistoricalCalendarSubsystem.h"
#include "UYearChangeOrchestrator.generated.h"

UENUM()
enum class EYearChangeState : uint8
{
    Idle,
    CollectingEvents,
    ProcessingEvents,
    Syncing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYearChangeBegin,    int32, Year);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnYearChangeComplete, int32, Year);

/**
 * Wraps UHistoricalCalendarSubsystem::AdvanceYear() to spread BFS event
 * processing across multiple ticks, preventing frame spikes on complex years
 * (Mfecane 1815, Lubombo 1846 etc).
 *
 * Uses a pre-allocated event pool (no heap allocation per year change) and
 * implements FTickableGameObject so it ticks even during PIE pause.
 */
UCLASS()
class MAHLANYARPG_API UYearChangeOrchestrator : public UWorldSubsystem,
                                                 public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // FTickableGameObject interface
    virtual void    Tick(float DeltaTime) override;
    virtual bool    IsTickable() const override;
    virtual TStatId GetStatId() const override;

    // AMahlanyaGameState::ServerAdvanceYear() calls this instead of
    // calling the calendar directly. Enqueues event nodes from the BFS graph
    // and transitions to CollectingEvents state.
    UFUNCTION(BlueprintCallable, Category="Mahlanya|History")
    void StartYearChange(int32 NewYear);

    UPROPERTY(BlueprintAssignable, Category="Mahlanya|History")
    FOnYearChangeBegin    OnYearChangeBegin;

    UPROPERTY(BlueprintAssignable, Category="Mahlanya|History")
    FOnYearChangeComplete OnYearChangeComplete;

private:
    EYearChangeState OrchestratorState = EYearChangeState::Idle;
    int32 TargetYear = 0;

    TWeakObjectPtr<UHistoricalCalendarSubsystem> CalendarRef;

    // ── Pre-allocated event pool (no heap allocation per event) ─────────────
    static constexpr int32 MaxPendingEvents = 256;

    struct FEventPoolSlot
    {
        FHistoricalEventNode Event;
        bool bInUse = false;
    };

    FEventPoolSlot EventPool[MaxPendingEvents];
    TArray<int32>  PendingIndices;   // indices into EventPool of events to process

    FEventPoolSlot* AllocateEventSlot();
    void            ReleaseEventSlot(int32 PoolIndex);
    void            ProcessNextEvent();
    void            FlushPool();
};
