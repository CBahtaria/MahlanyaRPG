// Copyright Charles Bartaria. All Rights Reserved.

#include "UYearChangeOrchestrator.h"
#include "Performance/UHardwareAdaptiveScaler.h"
#include "Performance/MahlanyaPerformanceCVars.h"
#include "Core/MahlanyaLogChannels.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"

void UYearChangeOrchestrator::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CalendarRef = GetWorld()->GetSubsystem<UHistoricalCalendarSubsystem>();

    // Zero the pool slots on init (bInUse = false for all)
    for (FEventPoolSlot& Slot : EventPool)
        Slot.bInUse = false;

    UE_LOG(LogMahlanyaYearChange, Log,
           TEXT("UYearChangeOrchestrator initialized. Pool capacity: %d events"),
           MaxPendingEvents);
}

void UYearChangeOrchestrator::Deinitialize()
{
    FlushPool();
    OrchestratorState = EYearChangeState::Idle;
    PendingIndices.Reset();
    Super::Deinitialize();
}

// ── FTickableGameObject ───────────────────────────────────────────────────────

bool UYearChangeOrchestrator::IsTickable() const
{
    return OrchestratorState != EYearChangeState::Idle;
}

TStatId UYearChangeOrchestrator::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UYearChangeOrchestrator, STATGROUP_Tickables);
}

void UYearChangeOrchestrator::Tick(float DeltaTime)
{
    switch (OrchestratorState)
    {
    case EYearChangeState::CollectingEvents:
    {
        // Collect unlocked events from the calendar into the pool
        UHistoricalCalendarSubsystem* Cal = CalendarRef.Get();
        if (!Cal)
        {
            OrchestratorState = EYearChangeState::Idle;
            return;
        }

        // AdvanceYear enqueues events in the calendar's internal graph.
        // We call it here (once) to populate GetEventsFiredThisYear().
        Cal->AdvanceYear(TargetYear);

        // Now enqueue those events into our pool for paced processing.
        const TArray<FName>& FiredIDs = Cal->GetEventsFiredThisYear();
        for (const FName& EID : FiredIDs)
        {
            FEventPoolSlot* Slot = AllocateEventSlot();
            if (!Slot)
            {
                UE_LOG(LogMahlanyaYearChange, Warning,
                       TEXT("Event pool full — dropping event %s"), *EID.ToString());
                break;
            }
            Slot->Event.EventID = EID;
            Slot->Event.Year    = TargetYear;
            Slot->bInUse        = true;
            PendingIndices.Add(static_cast<int32>(Slot - EventPool));
        }

        OnYearChangeBegin.Broadcast(TargetYear);
        OrchestratorState = EYearChangeState::ProcessingEvents;
        break;
    }

    case EYearChangeState::ProcessingEvents:
    {
        if (PendingIndices.IsEmpty())
        {
            OrchestratorState = EYearChangeState::Syncing;
            return;
        }

        // Read per-tick budget from hardware scaler (or fall back to CVar default)
        float BudgetMs = MahlanyaPerformanceCVars::YearChangeBudgetMs.GetValueOnGameThread();
        if (UGameInstance* GI = GetWorld()->GetGameInstance())
        {
            if (UHardwareAdaptiveScaler* Scaler = GI->GetSubsystem<UHardwareAdaptiveScaler>())
                BudgetMs = Scaler->GetSimulationConfig().SimulationTickBudgetMs;
        }

        const int32 MaxEvents =
            MahlanyaPerformanceCVars::MaxEventsPerTick.GetValueOnGameThread();

        const double TickStart = FPlatformTime::Seconds();
        int32 Processed = 0;

        while (!PendingIndices.IsEmpty() && Processed < MaxEvents)
        {
            const double Elapsed = (FPlatformTime::Seconds() - TickStart) * 1000.0;
            if (Elapsed > BudgetMs)
                break;  // defer remainder to next tick

            ProcessNextEvent();
            ++Processed;
        }
        break;
    }

    case EYearChangeState::Syncing:
        OnYearChangeComplete.Broadcast(TargetYear);
        OrchestratorState = EYearChangeState::Idle;
        break;

    case EYearChangeState::Idle:
    default:
        break;
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void UYearChangeOrchestrator::StartYearChange(int32 NewYear)
{
    if (OrchestratorState != EYearChangeState::Idle)
    {
        UE_LOG(LogMahlanyaYearChange, Warning,
               TEXT("StartYearChange(%d) called while orchestrator busy (state=%d) — ignored"),
               NewYear, (int32)OrchestratorState);
        return;
    }

    TargetYear = NewYear;
    PendingIndices.Reset();
    OrchestratorState = EYearChangeState::CollectingEvents;

    UE_LOG(LogMahlanyaYearChange, Log, TEXT("Year change to %d started"), NewYear);
}

// ── Event pool ────────────────────────────────────────────────────────────────

UYearChangeOrchestrator::FEventPoolSlot* UYearChangeOrchestrator::AllocateEventSlot()
{
    for (FEventPoolSlot& Slot : EventPool)
    {
        if (!Slot.bInUse)
        {
            Slot.bInUse = true;
            return &Slot;
        }
    }
    return nullptr;  // pool full
}

void UYearChangeOrchestrator::ReleaseEventSlot(int32 PoolIndex)
{
    if (PoolIndex >= 0 && PoolIndex < MaxPendingEvents)
        EventPool[PoolIndex].bInUse = false;
}

void UYearChangeOrchestrator::ProcessNextEvent()
{
    if (PendingIndices.IsEmpty())
        return;

    const int32 Idx = PendingIndices[0];
    PendingIndices.RemoveAt(0, 1, /*bAllowShrinking=*/false);

    // The actual event was already fired by Cal->AdvanceYear() in CollectingEvents;
    // here we just log and release the slot (future: run per-event side effects).
    if (Idx >= 0 && Idx < MaxPendingEvents && EventPool[Idx].bInUse)
    {
        UE_LOG(LogMahlanyaYearChange, Verbose,
               TEXT("Processing event %s (year %d)"),
               *EventPool[Idx].Event.EventID.ToString(), EventPool[Idx].Event.Year);
        ReleaseEventSlot(Idx);
    }
}

void UYearChangeOrchestrator::FlushPool()
{
    for (int32 i = 0; i < MaxPendingEvents; ++i)
    {
        if (EventPool[i].bInUse)
            ReleaseEventSlot(i);
    }
}
