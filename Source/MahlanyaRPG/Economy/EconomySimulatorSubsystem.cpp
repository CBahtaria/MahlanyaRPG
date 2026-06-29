#include "Economy/EconomySimulatorSubsystem.h"
#include "SimulationBusSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogMahlanya, Log, All);

// ---------------------------------------------------------------------------
// UEconomySimulatorSubsystem — UWorldSubsystem interface
// ---------------------------------------------------------------------------

void UEconomySimulatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LoadInitialClans();

    UE_LOG(LogMahlanya, Log,
        TEXT("UEconomySimulatorSubsystem: Initialised with %d clans."),
        ClanStates.Num());
}

// ---------------------------------------------------------------------------
// Tick entry point
// ---------------------------------------------------------------------------

void UEconomySimulatorSubsystem::SimulateEconomyTick(float GameDayDelta)
{
    if (GameDayDelta <= 0.0f)
    {
        return;
    }

    // 1. Advance game time.
    GameTime += GameDayDelta;

    // 2. Process all queued discrete events before any flows or triggers,
    //    ensuring a consistent snapshot at the start of the tick.
    ProcessEventBuffer();

    // 3. Apply periodic tribute flows pro-rated to GameDayDelta.
    ApplyTributeFlows(GameDayDelta);

    // 4. Stochastic raid trigger: may queue new CattleRaid events (processed
    //    next tick so they don't interfere with this tick's tribute flows).
    CheckRaidTriggers(GameDayDelta);

    // 5. Stochastic concession trigger: may accept concessions immediately
    //    (irreversible state change; doesn't involve cattle transfer).
    CheckConcessionTriggers();
}

// ---------------------------------------------------------------------------
// Probability queries
// ---------------------------------------------------------------------------

float UEconomySimulatorSubsystem::ComputeRaidProbability(FName ClanID) const
{
    const FClanEconomicState* Clan = FindClan(ClanID);
    if (!Clan)
    {
        UE_LOG(LogMahlanya, Warning,
            TEXT("UEconomySimulatorSubsystem::ComputeRaidProbability: Clan '%s' not found."),
            *ClanID.ToString());
        return 0.0f;
    }

    // P(raid) = DroughtStress * (1 - PoliticalStrength) * 0.3
    return Clan->DroughtStress * (1.0f - Clan->PoliticalStrength) * 0.3f;
}

float UEconomySimulatorSubsystem::ComputeConcessionProbability(FName ClanID) const
{
    const FClanEconomicState* Clan = FindClan(ClanID);
    if (!Clan)
    {
        UE_LOG(LogMahlanya, Warning,
            TEXT("UEconomySimulatorSubsystem::ComputeConcessionProbability: Clan '%s' not found."),
            *ClanID.ToString());
        return 0.0f;
    }

    // P(concession) = ColonialPressure * (1 - PoliticalStrength) * 0.4
    return Clan->ColonialPressure * (1.0f - Clan->PoliticalStrength) * 0.4f;
}

// ---------------------------------------------------------------------------
// Graph mutation
// ---------------------------------------------------------------------------

void UEconomySimulatorSubsystem::AddCattleFlow(const FCattleFlowEdge& Edge)
{
    FClanEconomicState* Source = FindClan(Edge.FromSettlement);
    if (!Source)
    {
        UE_LOG(LogMahlanya, Warning,
            TEXT("UEconomySimulatorSubsystem::AddCattleFlow: Source clan '%s' not found. Edge discarded."),
            *Edge.FromSettlement.ToString());
        return;
    }

    Source->OutboundFlows.Add(Edge);

    UE_LOG(LogMahlanya, Log,
        TEXT("UEconomySimulatorSubsystem::AddCattleFlow: Added flow '%s' -> '%s', %d cattle/month, Type=%d."),
        *Edge.FromSettlement.ToString(), *Edge.ToSettlement.ToString(),
        Edge.CattleCount, static_cast<int32>(Edge.FlowType));
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

FClanEconomicState UEconomySimulatorSubsystem::GetClanState(FName ClanID) const
{
    const FClanEconomicState* Clan = FindClan(ClanID);
    if (Clan)
    {
        return *Clan;
    }

    UE_LOG(LogMahlanya, Warning,
        TEXT("UEconomySimulatorSubsystem::GetClanState: Clan '%s' not found. Returning default state."),
        *ClanID.ToString());
    return FClanEconomicState{};
}

// ---------------------------------------------------------------------------
// Event queuing
// ---------------------------------------------------------------------------

void UEconomySimulatorSubsystem::QueueEvent(const FEconomyEvent& Event)
{
    EventBuffer.Add(Event);
}

// ---------------------------------------------------------------------------
// Internal helpers — ProcessEventBuffer
// ---------------------------------------------------------------------------

void UEconomySimulatorSubsystem::ProcessEventBuffer()
{
    if (EventBuffer.Num() == 0)
    {
        return;
    }

    UE_LOG(LogMahlanya, Log,
        TEXT("UEconomySimulatorSubsystem::ProcessEventBuffer: Processing %d event(s) at GameTime=%.2f."),
        EventBuffer.Num(), GameTime);

    for (const FEconomyEvent& Event : EventBuffer)
    {
        switch (Event.Type)
        {
        case EEconomyEventType::CattleRaid:
            UE_LOG(LogMahlanya, Log,
                TEXT("  [CattleRaid] '%s' raids '%s' for %d cattle (T=%.2f)."),
                *Event.SourceClan.ToString(), *Event.TargetClan.ToString(),
                Event.CattleDelta, Event.Timestamp);
            // Remove cattle from raided clan; raiding clan gains them.
            ApplyCattleDelta(Event.TargetClan, -Event.CattleDelta);
            ApplyCattleDelta(Event.SourceClan, +Event.CattleDelta);
            break;

        case EEconomyEventType::Lobola:
            UE_LOG(LogMahlanya, Log,
                TEXT("  [Lobola] '%s' pays '%s' %d cattle as bride price (T=%.2f)."),
                *Event.SourceClan.ToString(), *Event.TargetClan.ToString(),
                Event.CattleDelta, Event.Timestamp);
            ApplyCattleDelta(Event.SourceClan, -Event.CattleDelta);
            ApplyCattleDelta(Event.TargetClan, +Event.CattleDelta);
            // Lobola triggers a demographic recompute; broadcast to SibayaEngine via
            // SimulationBusSubsystem so it can recompute Voronoi settlement boundaries.
            if (USimulationBusSubsystem* Bus =
                    GetWorld() ? GetWorld()->GetSubsystem<USimulationBusSubsystem>() : nullptr)
            {
                // NewWifeCount param carries the signed cattle delta as the demographic
                // signal: payer's homestead shrinks, receiver's grows.
                Bus->BroadcastSettlementDemographicChanged(Event.SourceClan, -Event.CattleDelta);
                Bus->BroadcastSettlementDemographicChanged(Event.TargetClan, +Event.CattleDelta);
            }
            break;

        case EEconomyEventType::Tribute:
            UE_LOG(LogMahlanya, Log,
                TEXT("  [Tribute] '%s' pays '%s' %d cattle (T=%.2f)."),
                *Event.SourceClan.ToString(), *Event.TargetClan.ToString(),
                Event.CattleDelta, Event.Timestamp);
            ApplyCattleDelta(Event.SourceClan, -Event.CattleDelta);
            ApplyCattleDelta(Event.TargetClan, +Event.CattleDelta);
            break;

        case EEconomyEventType::Drought:
            UE_LOG(LogMahlanya, Log,
                TEXT("  [Drought] '%s' loses %d cattle to drought stress (T=%.2f)."),
                *Event.SourceClan.ToString(), Event.CattleDelta, Event.Timestamp);
            ApplyCattleDelta(Event.SourceClan, -Event.CattleDelta);
            break;

        default:
            UE_LOG(LogMahlanya, Warning,
                TEXT("  [Unknown] Unhandled EEconomyEventType value %d. Skipped."),
                static_cast<int32>(Event.Type));
            break;
        }
    }

    EventBuffer.Reset();
}

// ---------------------------------------------------------------------------
// Internal helpers — ApplyTributeFlows
// ---------------------------------------------------------------------------

void UEconomySimulatorSubsystem::ApplyTributeFlows(float GameDayDelta)
{
    // Each flow specifies CattleCount cattle per 30-day game-month.
    // Pro-rate to the current tick delta: Transfer = CattleCount / 30 * GameDayDelta.
    // We use integer arithmetic here; fractional cattle are lost (rounded down).
    const float MonthFraction = GameDayDelta / 30.0f;

    for (FClanEconomicState& Source : ClanStates)
    {
        for (const FCattleFlowEdge& Edge : Source.OutboundFlows)
        {
            if (Edge.FlowType != ECattleFlowType::Tribute)
            {
                continue;
            }

            const int32 TransferAmount = FMath::FloorToInt(
                static_cast<float>(Edge.CattleCount) * MonthFraction);

            if (TransferAmount <= 0)
            {
                continue;
            }

            // Clamp: can't pay more than you have.
            const int32 Actual = FMath::Min(TransferAmount, Source.CattleCount);
            if (Actual <= 0)
            {
                UE_LOG(LogMahlanya, Log,
                    TEXT("UEconomySimulatorSubsystem::ApplyTributeFlows: '%s' cannot pay tribute to '%s' (CattleCount=0)."),
                    *Edge.FromSettlement.ToString(), *Edge.ToSettlement.ToString());
                continue;
            }

            ApplyCattleDelta(Edge.FromSettlement, -Actual);
            ApplyCattleDelta(Edge.ToSettlement, +Actual);

            UE_LOG(LogMahlanya, Log,
                TEXT("UEconomySimulatorSubsystem::ApplyTributeFlows: '%s' -> '%s': %d cattle tribute (tick delta=%.2f days)."),
                *Edge.FromSettlement.ToString(), *Edge.ToSettlement.ToString(),
                Actual, GameDayDelta);
        }
    }
}

// ---------------------------------------------------------------------------
// Internal helpers — CheckRaidTriggers
// ---------------------------------------------------------------------------

void UEconomySimulatorSubsystem::CheckRaidTriggers(float GameDayDelta)
{
    // Collect a snapshot of clan IDs so we can pick a random neighbour
    // without referencing the array while potentially modifying it.
    TArray<FName> AllClanIDs;
    AllClanIDs.Reserve(ClanStates.Num());
    for (const FClanEconomicState& Clan : ClanStates)
    {
        AllClanIDs.Add(Clan.ClanID);
    }

    for (const FClanEconomicState& Clan : ClanStates)
    {
        const float P = ComputeRaidProbability(Clan.ClanID);
        if (P <= 0.0f)
        {
            continue;
        }

        // Scale probability by time delta: higher delta -> higher chance this tick.
        const float Sample = FMath::FRand();
        if (Sample >= P * GameDayDelta)
        {
            continue;
        }

        // Pick a random target that is not the raiding clan itself.
        TArray<FName> Candidates;
        for (const FName& ID : AllClanIDs)
        {
            if (ID != Clan.ClanID)
            {
                Candidates.Add(ID);
            }
        }

        if (Candidates.Num() == 0)
        {
            continue;
        }

        const int32 TargetIdx = FMath::RandRange(0, Candidates.Num() - 1);
        const FName TargetClan = Candidates[TargetIdx];

        // Raid takes 10% of the raider's current herd (bounded by target's herd).
        const FClanEconomicState* Raider = FindClan(Clan.ClanID);
        const FClanEconomicState* Target = FindClan(TargetClan);
        if (!Raider || !Target)
        {
            continue;
        }

        const int32 RaidAmount = FMath::Max(1,
            FMath::Min(
                FMath::RandRange(5, FMath::Max(5, Raider->CattleCount / 10)),
                Target->CattleCount));

        FEconomyEvent RaidEvent;
        RaidEvent.Type        = EEconomyEventType::CattleRaid;
        RaidEvent.SourceClan  = Clan.ClanID;
        RaidEvent.TargetClan  = TargetClan;
        RaidEvent.CattleDelta = RaidAmount;
        RaidEvent.Timestamp   = GameTime;

        QueueEvent(RaidEvent);

        UE_LOG(LogMahlanya, Log,
            TEXT("UEconomySimulatorSubsystem::CheckRaidTriggers: '%s' queued raid on '%s' for %d cattle (P=%.3f)."),
            *Clan.ClanID.ToString(), *TargetClan.ToString(), RaidAmount, P);

        // Notify EmergentNarrativePlugin that a "RaidOfNeed" condition has been met.
        // ConditionValue = DroughtStress of the raiding clan as the severity proxy.
        if (USimulationBusSubsystem* Bus =
                GetWorld() ? GetWorld()->GetSubsystem<USimulationBusSubsystem>() : nullptr)
        {
            Bus->BroadcastQuestTriggerConditionMet(FName("RaidOfNeed"), Clan.DroughtStress);
        }
    }
}

// ---------------------------------------------------------------------------
// Internal helpers — CheckConcessionTriggers
// ---------------------------------------------------------------------------

void UEconomySimulatorSubsystem::CheckConcessionTriggers()
{
    for (FClanEconomicState& Clan : ClanStates)
    {
        const float P = ComputeConcessionProbability(Clan.ClanID);
        if (P <= 0.0f)
        {
            continue;
        }

        const float Sample = FMath::FRand();
        if (Sample >= P)
        {
            continue;
        }

        // Generate a unique concession name based on game time and clan.
        const FName ConcessionID = FName(
            FString::Printf(TEXT("Concession_%s_T%.0f"), *Clan.ClanID.ToString(), GameTime));

        Clan.Concessions.Add(ConcessionID);

        UE_LOG(LogMahlanya, Log,
            TEXT("UEconomySimulatorSubsystem::CheckConcessionTriggers: '%s' accepted concession '%s' (P=%.3f, ColonialPressure=%.2f, PoliticalStrength=%.2f)."),
            *Clan.ClanID.ToString(), *ConcessionID.ToString(),
            P, Clan.ColonialPressure, Clan.PoliticalStrength);

        OnConcessionAccepted.Broadcast(Clan.ClanID);

        // Notify EmergentNarrativePlugin that a "ConcessionBetrayal" condition has
        // been met. ConditionValue = ColonialPressure as the pressure level proxy.
        if (USimulationBusSubsystem* Bus =
                GetWorld() ? GetWorld()->GetSubsystem<USimulationBusSubsystem>() : nullptr)
        {
            Bus->BroadcastQuestTriggerConditionMet(FName("ConcessionBetrayal"), Clan.ColonialPressure);
        }
    }
}

// ---------------------------------------------------------------------------
// Internal helpers — LoadInitialClans
// ---------------------------------------------------------------------------

void UEconomySimulatorSubsystem::LoadInitialClans()
{
    ClanStates.Reset();

    // -----------------------------------------------------------------------
    // Clan_Dlamini — Royal clan; largest herd, dominant political standing,
    // minimal colonial exposure in the early-game period.
    // -----------------------------------------------------------------------
    {
        FClanEconomicState State;
        State.ClanID            = FName("Clan_Dlamini");
        State.CattleCount       = 500;
        State.PoliticalStrength = 0.9f;
        State.ColonialPressure  = 0.1f;
        State.DroughtStress     = 0.0f;
        ClanStates.Add(State);
    }

    // -----------------------------------------------------------------------
    // Clan_Nkosi — Mid-tier clan; moderate herd, moderate political strength,
    // moderate colonial pressure from proximity to concession routes.
    // -----------------------------------------------------------------------
    {
        FClanEconomicState State;
        State.ClanID            = FName("Clan_Nkosi");
        State.CattleCount       = 200;
        State.PoliticalStrength = 0.6f;
        State.ColonialPressure  = 0.3f;
        State.DroughtStress     = 0.0f;
        ClanStates.Add(State);
    }

    // -----------------------------------------------------------------------
    // Clan_Mkhonta — Peripheral clan; small herd, low political standing,
    // high colonial pressure due to frontier position and limited royal ties.
    // -----------------------------------------------------------------------
    {
        FClanEconomicState State;
        State.ClanID            = FName("Clan_Mkhonta");
        State.CattleCount       = 80;
        State.PoliticalStrength = 0.3f;
        State.ColonialPressure  = 0.6f;
        State.DroughtStress     = 0.0f;
        ClanStates.Add(State);
    }

    UE_LOG(LogMahlanya, Log,
        TEXT("UEconomySimulatorSubsystem::LoadInitialClans: Seeded %d clans."),
        ClanStates.Num());
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

FClanEconomicState* UEconomySimulatorSubsystem::FindClan(FName ClanID)
{
    for (FClanEconomicState& State : ClanStates)
    {
        if (State.ClanID == ClanID)
        {
            return &State;
        }
    }
    return nullptr;
}

const FClanEconomicState* UEconomySimulatorSubsystem::FindClan(FName ClanID) const
{
    for (const FClanEconomicState& State : ClanStates)
    {
        if (State.ClanID == ClanID)
        {
            return &State;
        }
    }
    return nullptr;
}

void UEconomySimulatorSubsystem::ApplyCattleDelta(FName ClanID, int32 Delta)
{
    FClanEconomicState* Clan = FindClan(ClanID);
    if (!Clan)
    {
        return;
    }

    const int32 Before = Clan->CattleCount;
    Clan->CattleCount  = FMath::Max(0, Clan->CattleCount + Delta);

    if (Clan->CattleCount != Before)
    {
        UE_LOG(LogMahlanya, Log,
            TEXT("UEconomySimulatorSubsystem::ApplyCattleDelta: '%s' cattle %d -> %d (delta=%+d)."),
            *ClanID.ToString(), Before, Clan->CattleCount, Delta);
    }

    // Broadcast critical-cattle warning when the herd crosses below the threshold.
    // Only fire on the transition (Before was above, After is at or below).
    if (Clan->CattleCount <= CattleCriticalThreshold && Before > CattleCriticalThreshold)
    {
        UE_LOG(LogMahlanya, Log,
            TEXT("UEconomySimulatorSubsystem: CRITICAL — '%s' cattle count dropped to %d (threshold=%d)."),
            *ClanID.ToString(), Clan->CattleCount, CattleCriticalThreshold);

        OnCattleCritical.Broadcast(ClanID, Clan->CattleCount);
    }
}
