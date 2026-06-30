// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "GameFramework/MahlanyaGameClockSubsystem.h"
#include "GameFramework/MahlanyaGameState.h"
#include "Core/MahlanyaLogChannels.h"
#include "UEconomySimulatorSubsystem.h"
#include "UEcologySimulatorSubsystem.h"
#include "UMicroclimateSubsystem.h"
#include "SimulationBusSubsystem.h"
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
            // Wire terrain saturation changes into drought stress on all clans
            if (USimulationBusSubsystem* Bus = World->GetSubsystem<USimulationBusSubsystem>())
            {
                DroughtWireHandle = Bus->OnTerrainSaturationChanged.AddWeakLambda(
                    this, [this](float SaturationFraction)
                    {
                        UWorld* W = GetWorld();
                        if (!W) return;
                        UEconomySimulatorSubsystem* Economy = W->GetSubsystem<UEconomySimulatorSubsystem>();
                        if (!Economy) return;
                        const float DroughtIndex = FMath::Max(0.f, 1.f - SaturationFraction);
                        for (const FName& ClanID : Economy->GetAllClanIDs())
                            Economy->SetDroughtStress(ClanID, DroughtIndex);
                    });
            }

            StartClock();
            UE_LOG(LogMahlanyaSimulation, Log,
                TEXT("GameClock: started at year %d (%.2f s/day)"),
                CurrentGameYear, SecondsPerGameDay);
        }
    }
}

void UMahlanyaGameClockSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        if (USimulationBusSubsystem* Bus = World->GetSubsystem<USimulationBusSubsystem>())
            Bus->OnTerrainSaturationChanged.Remove(DroughtWireHandle);
    }
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

    // ── Replication push: weather + clan snapshots every 30 game-days ─────────
    if (DayOfYear % 30 == 0)
    {
        if (AMahlanyaGameState* GS = World->GetGameState<AMahlanyaGameState>())
        {
            if (UMicroclimateSubsystem* Clim = World->GetSubsystem<UMicroclimateSubsystem>())
            {
                const FMicroclimateState& MS = Clim->GetCurrentState();
                FReplicatedWeatherState WS;
                WS.PressureHPa            = MS.PressureHPa;
                WS.PrecipitationIntensity = MS.PrecipitationIntensity;
                WS.WindSpeed_ms           = MS.WindSpeed_ms;
                WS.WindBearing_deg        = MS.WindBearing_deg;
                WS.TurbidityParam         = MS.TurbidityParam;
                WS.FogBaseAltitude_m      = MS.FogBaseAltitude_m;
                GS->ServerUpdateWeather(WS);
            }

            if (UEconomySimulatorSubsystem* Economy = World->GetSubsystem<UEconomySimulatorSubsystem>())
            {
                for (const FName& ClanID : Economy->GetAllClanIDs())
                {
                    const FClanEconomicState& CS = Economy->GetClanState(ClanID);
                    FReplicatedClanSnapshot Snap;
                    Snap.ClanID            = ClanID;
                    Snap.CattleCount       = CS.CattleCount;
                    Snap.PoliticalStrength = CS.PoliticalStrength;
                    Snap.ColonialPressure  = CS.ColonialPressure;
                    GS->ServerUpdateClanSnapshot(Snap);
                }
            }
        }
    }

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
