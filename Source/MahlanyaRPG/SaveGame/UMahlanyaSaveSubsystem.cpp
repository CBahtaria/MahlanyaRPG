// Copyright Charles Bartaria. All Rights Reserved.

#include "UMahlanyaSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UEconomySimulatorSubsystem.h"
#include "UHistoricalCalendarSubsystem.h"
#include "USibayaEngineSubsystem.h"
#include "Core/MahlanyaLogChannels.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

FString UMahlanyaSaveSubsystem::MakeSlotName(int32 SlotIndex) const
{
    return FString::Printf(TEXT("MahlanyaSlot_%d"), SlotIndex);
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

void UMahlanyaSaveSubsystem::SaveGame(int32 SlotIndex)
{
    UMahlanyaSaveGame* Save = NewObject<UMahlanyaSaveGame>(this);
    if (!Save)
    {
        UE_LOG(LogMahlanyaSimulation, Error,
               TEXT("SaveGame: Failed to allocate UMahlanyaSaveGame for slot %d"), SlotIndex);
        OnSaveComplete.Broadcast(false, SlotIndex);
        return;
    }

    Save->Meta.SlotIndex    = SlotIndex;
    Save->Meta.SaveTimestamp = FDateTime::UtcNow();

    SnapshotWorldState(Save);

    const FString SlotName = MakeSlotName(SlotIndex);
    const bool bSuccess = UGameplayStatics::SaveGameToSlot(Save, SlotName, SlotIndex);

    if (bSuccess)
    {
        ActiveSave = Save;
        UE_LOG(LogMahlanyaSimulation, Log,
               TEXT("SaveGame: Slot %d (%s) saved successfully — Year %d, %d clans, %d events"),
               SlotIndex, *SlotName,
               Save->CurrentGameYear,
               Save->ClanStates.Num(),
               Save->FiredHistoricalEvents.Num());
    }
    else
    {
        UE_LOG(LogMahlanyaSimulation, Error,
               TEXT("SaveGame: UGameplayStatics::SaveGameToSlot failed for slot %d (%s)"),
               SlotIndex, *SlotName);
    }

    OnSaveComplete.Broadcast(bSuccess, SlotIndex);
}

// ---------------------------------------------------------------------------
// Load
// ---------------------------------------------------------------------------

void UMahlanyaSaveSubsystem::LoadGame(int32 SlotIndex)
{
    const FString SlotName = MakeSlotName(SlotIndex);
    UE_LOG(LogMahlanyaSimulation, Log,
           TEXT("LoadGame: Requesting async load for slot %d (%s)"), SlotIndex, *SlotName);

    FAsyncLoadGameFromSlotDelegate LoadDelegate;
    LoadDelegate.BindUObject(this, &UMahlanyaSaveSubsystem::OnLoadFinished);
    UGameplayStatics::AsyncLoadGameFromSlot(SlotName, SlotIndex, LoadDelegate);
}

void UMahlanyaSaveSubsystem::OnLoadFinished(const FString& SlotName,
                                             const int32   UserIndex,
                                             USaveGame*    SaveGameObject)
{
    UMahlanyaSaveGame* LoadedSave = Cast<UMahlanyaSaveGame>(SaveGameObject);
    if (!LoadedSave)
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("OnLoadFinished: Save for slot '%s' (user %d) is null or wrong type — load aborted"),
               *SlotName, UserIndex);
        OnLoadComplete.Broadcast(false, UserIndex);
        return;
    }

    ActiveSave = LoadedSave;

    UE_LOG(LogMahlanyaSimulation, Log,
           TEXT("OnLoadFinished: Slot '%s' loaded — Year %d, %d clans, %d events"),
           *SlotName,
           LoadedSave->CurrentGameYear,
           LoadedSave->ClanStates.Num(),
           LoadedSave->FiredHistoricalEvents.Num());

    RestoreWorldState(ActiveSave);

    OnLoadComplete.Broadcast(true, UserIndex);
}

// ---------------------------------------------------------------------------
// SnapshotWorldState
// ---------------------------------------------------------------------------

void UMahlanyaSaveSubsystem::SnapshotWorldState(UMahlanyaSaveGame* Save) const
{
    if (!Save)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("SnapshotWorldState: GameInstance is null — world state not captured"));
        return;
    }

    UWorld* World = GI->GetWorld();
    if (!World)
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("SnapshotWorldState: World is null — world state not captured"));
        return;
    }

    // ── Economy subsystem ─────────────────────────────────────────────────────
    UEconomySimulatorSubsystem* EconSS = World->GetSubsystem<UEconomySimulatorSubsystem>();
    if (EconSS)
    {
        const TArray<FName> ClanIDs = EconSS->GetAllClanIDs();
        Save->ClanStates.Reset(ClanIDs.Num());
        for (const FName& ClanID : ClanIDs)
        {
            const FClanEconomicState ClanState = EconSS->GetClanState(ClanID);
            FSavedClanState Saved;
            Saved.ClanID            = ClanState.ClanID;
            Saved.CattleCount       = ClanState.CattleCount;
            Saved.PoliticalStrength = ClanState.PoliticalStrength;
            Saved.ColonialPressure  = ClanState.ColonialPressure;
            Save->ClanStates.Add(Saved);
        }
        UE_LOG(LogMahlanyaSimulation, Verbose,
               TEXT("SnapshotWorldState: Captured %d clan states"), Save->ClanStates.Num());
    }
    else
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("SnapshotWorldState: UEconomySimulatorSubsystem not available — clan states not captured"));
    }

    // ── Historical calendar subsystem ─────────────────────────────────────────
    UHistoricalCalendarSubsystem* CalSS = World->GetSubsystem<UHistoricalCalendarSubsystem>();
    if (CalSS)
    {
        Save->CurrentGameYear = CalSS->GetCurrentYear();
        Save->CurrentGameDay  = FMath::FloorToInt(CalSS->GetCurrentDayOfYear());
        Save->Meta.GameYear   = Save->CurrentGameYear;
        Save->Meta.GameDay    = Save->CurrentGameDay;

        // Derive the fired-events list from the known event chain IDs by
        // querying HasEventFired() for each. The full chain is defined in
        // UHistoricalCalendarSubsystem::RegisterHistoricalEvents().
        static const TArray<FName> KnownEventIDs =
        {
            FName("EV_NgwaneKingdom"),
            FName("EV_Mfecane"),
            FName("EV_LubomboBattle"),
            FName("EV_ConcessionRush"),
            FName("EV_BhunuTrial"),
            FName("EV_Partition"),
        };

        Save->FiredHistoricalEvents.Reset();
        for (const FName& EventID : KnownEventIDs)
        {
            if (CalSS->HasEventFired(EventID))
            {
                Save->FiredHistoricalEvents.Add(EventID);
            }
        }
        UE_LOG(LogMahlanyaSimulation, Verbose,
               TEXT("SnapshotWorldState: Year %d captured, %d events fired"),
               Save->CurrentGameYear, Save->FiredHistoricalEvents.Num());
    }
    else
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("SnapshotWorldState: UHistoricalCalendarSubsystem not available — calendar not captured"));
    }

    // ── Sibaya settlement subsystem ───────────────────────────────────────────
    USibayaEngineSubsystem* SibayaSS = World->GetSubsystem<USibayaEngineSubsystem>();
    if (SibayaSS)
    {
        // Settlement wife counts are stored in the save as SettlementID→NumWives.
        // We iterate clan states to resolve matching settlement IDs (clan IDs map
        // 1-to-1 to settlement IDs by convention in MahlanyaRPG).
        Save->SettlementWiveCounts.Reset();
        for (const FSavedClanState& ClanSave : Save->ClanStates)
        {
            FSettlementID SID;
            SID.ID = ClanSave.ClanID;
            const FVoronoiLayout Layout = SibayaSS->GetLayout(SID);
            // NumWives is encoded as the number of hut positions in the layout
            // (each wife occupies one hut position in the Voronoi tessellation).
            Save->SettlementWiveCounts.Add(ClanSave.ClanID,
                                            Layout.HutPositions.Num());
        }
        UE_LOG(LogMahlanyaSimulation, Verbose,
               TEXT("SnapshotWorldState: Captured %d settlement wife counts"),
               Save->SettlementWiveCounts.Num());
    }
    else
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("SnapshotWorldState: USibayaEngineSubsystem not available — settlement data not captured"));
    }
}

// ---------------------------------------------------------------------------
// RestoreWorldState
// ---------------------------------------------------------------------------

void UMahlanyaSaveSubsystem::RestoreWorldState(const UMahlanyaSaveGame* Save) const
{
    if (!Save)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("RestoreWorldState: GameInstance is null — world state not restored"));
        return;
    }

    UWorld* World = GI->GetWorld();
    if (!World)
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("RestoreWorldState: World is null — world state not restored"));
        return;
    }

    // ── Economy subsystem ─────────────────────────────────────────────────────
    UEconomySimulatorSubsystem* EconSS = World->GetSubsystem<UEconomySimulatorSubsystem>();
    if (EconSS)
    {
        for (const FSavedClanState& ClanSave : Save->ClanStates)
        {
            FClanEconomicState State;
            State.ClanID            = ClanSave.ClanID;
            State.CattleCount       = ClanSave.CattleCount;
            State.PoliticalStrength = ClanSave.PoliticalStrength;
            State.ColonialPressure  = ClanSave.ColonialPressure;
            EconSS->RegisterClan(State);
        }
        UE_LOG(LogMahlanyaSimulation, Log,
               TEXT("RestoreWorldState: Restored %d clan states"), Save->ClanStates.Num());
    }
    else
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("RestoreWorldState: UEconomySimulatorSubsystem not available — clan states not restored"));
    }

    // ── Historical calendar subsystem ─────────────────────────────────────────
    // SetStartYear resets the year and DayOfYear. Events will re-fire naturally
    // as AdvanceTime is called, since TryFireEvents checks CurrentYear >= Node.Year.
    // The FiredHistoricalEvents array is stored for external systems that need
    // to know which events have already occurred (e.g. narrative systems).
    UHistoricalCalendarSubsystem* CalSS = World->GetSubsystem<UHistoricalCalendarSubsystem>();
    if (CalSS)
    {
        CalSS->SetStartYear(Save->CurrentGameYear);
        // Advance a fractional day to trigger TryFireEvents so that all events
        // whose Year <= CurrentGameYear fire immediately upon restore, bringing
        // the event set back into the correct state.
        const float RestoredDayFraction = FMath::Max(0.f,
            static_cast<float>(Save->CurrentGameDay));
        if (RestoredDayFraction > 0.f)
        {
            CalSS->AdvanceTime(RestoredDayFraction);
        }
        UE_LOG(LogMahlanyaSimulation, Log,
               TEXT("RestoreWorldState: Calendar restored to Year %d, Day %d"),
               Save->CurrentGameYear, Save->CurrentGameDay);
    }
    else
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("RestoreWorldState: UHistoricalCalendarSubsystem not available — calendar not restored"));
    }

    // ── Sibaya settlement subsystem ───────────────────────────────────────────
    USibayaEngineSubsystem* SibayaSS = World->GetSubsystem<USibayaEngineSubsystem>();
    if (SibayaSS)
    {
        for (const TPair<FName, int32>& Entry : Save->SettlementWiveCounts)
        {
            FSettlementID SID;
            SID.ID = Entry.Key;
            // Restore the settlement with NumWives from snapshot.
            // Remaining demographic fields default to 0 as they will be
            // re-derived by the subsystem on the next simulation tick.
            SibayaSS->RegisterSettlement(SID,
                /*NumWives=*/      Entry.Value,
                /*NumSons=*/       0,
                /*NumDependents=*/ 0,
                /*CattleCount=*/   0,
                /*TerrainGradient=*/0.f);
        }
        UE_LOG(LogMahlanyaSimulation, Log,
               TEXT("RestoreWorldState: Restored %d settlements"),
               Save->SettlementWiveCounts.Num());
    }
    else
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
               TEXT("RestoreWorldState: USibayaEngineSubsystem not available — settlements not restored"));
    }
}
