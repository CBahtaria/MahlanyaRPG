// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "GameFramework/MahlanyaGameClockSubsystem.h"
#include "GameFramework/MahlanyaGameState.h"
#include "Core/MahlanyaLogChannels.h"
#include "UEconomySimulatorSubsystem.h"
#include "UEcologySimulatorSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMahlanyaGameClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    CurrentGameYear = StartingYear;
    CurrentGameDay  = 0;
    DayOfYear       = 0;

    // Only the authority drives time. Clients receive year/weather updates
    // through replicated properties on AMahlanyaGameState.
    if (UWorld* World = GetWorld())
    {
        const ENetMode NetMode = World->GetNetMode();
        if (NetMode != NM_Client)
        {
            StartClock();
            UE_LOG(LogMahlanyaSimulation, Log,
                TEXT("GameClock: started at year %d (%.2f s/day)"),
                CurrentGameYear, SecondsPerGameDay);
        }
    }
}

void UMahlanyaGameClockSubsystem::Deinitialize()
{
    StopClock();
    Super::Deinitialize();
}

void UMahlanyaGameClockSubsystem::StartClock()
{
    UWorld* World = GetWorld();
    if (!World) return;

    World->GetTimerManager().SetTimer(
        ClockTimerHandle,
        this,
        &UMahlanyaGameClockSubsystem::OnClockTick,
        SecondsPerGameDay,
        /*bLoop=*/true);
}

void UMahlanyaGameClockSubsystem::StopClock()
{
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(ClockTimerHandle);
}

void UMahlanyaGameClockSubsystem::OnClockTick()
{
    UWorld* World = GetWorld();
    if (!World) return;

    ++CurrentGameDay;
    ++DayOfYear;

    // ── Economy: every game-day ───────────────────────────────────────────────
    if (UEconomySimulatorSubsystem* Economy =
            World->GetSubsystem<UEconomySimulatorSubsystem>())
    {
        Economy->SimulateEconomyTick(1.0f);
    }

    // ── Ecology: every game-week ──────────────────────────────────────────────
    if (DayOfYear % DaysPerWeek == 0)
    {
        if (UEcologySimulatorSubsystem* Ecology =
                World->GetSubsystem<UEcologySimulatorSubsystem>())
        {
            Ecology->SimulateEcologyTick(static_cast<float>(DaysPerWeek));
        }
    }

    OnGameDayAdvanced.Broadcast(CurrentGameDay);

    // ── Year wrap ─────────────────────────────────────────────────────────────
    if (DayOfYear >= DaysPerYear)
    {
        DayOfYear = 0;
        ++CurrentGameYear;

        // Routes through UYearChangeOrchestrator to spread BFS events across ticks
        if (AMahlanyaGameState* GS = World->GetGameState<AMahlanyaGameState>())
        {
            GS->ServerAdvanceYear(1);
        }

        UE_LOG(LogMahlanyaSimulation, Log,
            TEXT("GameClock: year %d begins (day %d)"),
            CurrentGameYear, CurrentGameDay);

        OnGameYearAdvanced.Broadcast(CurrentGameYear);
    }
}
