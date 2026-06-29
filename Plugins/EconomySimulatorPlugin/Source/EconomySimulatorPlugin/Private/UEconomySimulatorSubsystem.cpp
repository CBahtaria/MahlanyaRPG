// Copyright Charles Bartaria. All Rights Reserved.

#include "UEconomySimulatorSubsystem.h"
#include "SimulationBusSubsystem.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/World.h"

// ── Constants ────────────────────────────────────────────────────────────────

static constexpr float GameDaysPerMonth    = 30.f;
static constexpr float DroughtRaidThreshold    = 0.7f;
static constexpr int32 CattleRaidThreshold     = 50;
static constexpr float ColonialBetrayalThreshold   = 0.6f;
static constexpr float StrengthBetrayalThreshold   = 0.4f;

// ── UWorldSubsystem interface ────────────────────────────────────────────────

void UEconomySimulatorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    AccumulatedDays = 0.f;

    // Seed the four founding Swazi clans. All start at equal standing —
    // 100 cattle, political strength 0.5, no pressure or concessions.
    const TArray<FName> FoundingClans = {
        FName("Dlamini"),
        FName("Nkosi"),
        FName("Mlangeni"),
        FName("Motsa")
    };

    for (const FName& ClanName : FoundingClans)
    {
        FClanEconomicState State;
        State.ClanID            = ClanName;
        State.CattleCount       = 100;
        State.PoliticalStrength = 0.5f;
        State.ColonialPressure  = 0.f;
        State.DroughtStress     = 0.f;

        ClanStates.Add(ClanName, State);
    }
}

void UEconomySimulatorSubsystem::Deinitialize()
{
    ClanStates.Empty();
    Super::Deinitialize();
}

// ── Clan registration ────────────────────────────────────────────────────────

void UEconomySimulatorSubsystem::RegisterClan(const FClanEconomicState& InitialState)
{
    ClanStates.Add(InitialState.ClanID, InitialState);
}

// ── Simulation tick ──────────────────────────────────────────────────────────

void UEconomySimulatorSubsystem::SimulateEconomyTick(float GameDayDelta)
{
    if (GameDayDelta <= 0.f)
    {
        return;
    }

    AccumulatedDays += GameDayDelta;

    // Fire monthly processing for each complete month accumulated.
    // More than one month could accumulate if the caller paused ticking.
    while (AccumulatedDays >= GameDaysPerMonth)
    {
        AccumulatedDays -= GameDaysPerMonth;
        ProcessMonthlyEconomy();
    }
}

// ── External data feeds ──────────────────────────────────────────────────────

void UEconomySimulatorSubsystem::SetDroughtStress(FName ClanID, float DroughtIndex)
{
    if (FClanEconomicState* State = ClanStates.Find(ClanID))
    {
        State->DroughtStress = FMath::Clamp(DroughtIndex, 0.f, 1.f);
    }
}

// ── Economic transactions ────────────────────────────────────────────────────

void UEconomySimulatorSubsystem::RecordLobolaTransaction(
    FName FromClan, FName ToClan, int32 CattleCount)
{
    if (CattleCount <= 0)
    {
        return;
    }

    FClanEconomicState* Payer    = ClanStates.Find(FromClan);
    FClanEconomicState* Receiver = ClanStates.Find(ToClan);

    if (!Payer || !Receiver)
    {
        return;
    }

    // Clamp transfer so payer cannot go negative.
    const int32 ActualTransfer = FMath::Min(CattleCount, Payer->CattleCount);
    Payer->CattleCount    -= ActualTransfer;
    Receiver->CattleCount += ActualTransfer;

    // Recompute political strength for both parties immediately since the
    // cattle count changed — lobola is a significant political act.
    Payer->PoliticalStrength    = Payer->ComputePoliticalStrength();
    Receiver->PoliticalStrength = Receiver->ComputePoliticalStrength();

    // Notify SibayaEngine to recompute Voronoi settlement boundaries.
    if (USimulationBusSubsystem* Bus =
            GetWorld()->GetSubsystem<USimulationBusSubsystem>())
    {
        // NewWifeCount parameter carries the transferred cattle count as the
        // demographic signal (new wives join the receiver's homestead).
        Bus->BroadcastSettlementDemographicChanged(FromClan,  -ActualTransfer);
        Bus->BroadcastSettlementDemographicChanged(ToClan,     ActualTransfer);
    }
}

void UEconomySimulatorSubsystem::RecordCattleRaid(
    FName RaiderClan, FName VictimClan, int32 CattleTransferred)
{
    if (CattleTransferred <= 0)
    {
        return;
    }

    FClanEconomicState* Raider = ClanStates.Find(RaiderClan);
    FClanEconomicState* Victim = ClanStates.Find(VictimClan);

    if (!Raider || !Victim)
    {
        return;
    }

    // Cannot take more cattle than the victim owns.
    const int32 ActualTransfer = FMath::Min(CattleTransferred, Victim->CattleCount);
    Victim->CattleCount  -= ActualTransfer;
    Raider->CattleCount  += ActualTransfer;

    // Political strength shifts with the cattle transfer.
    Raider->PoliticalStrength = Raider->ComputePoliticalStrength();
    Victim->PoliticalStrength = Victim->ComputePoliticalStrength();
}

// ── Queries ──────────────────────────────────────────────────────────────────

FClanEconomicState UEconomySimulatorSubsystem::GetClanState(FName ClanID) const
{
    if (const FClanEconomicState* State = ClanStates.Find(ClanID))
    {
        return *State;
    }
    return FClanEconomicState{};
}

TArray<FName> UEconomySimulatorSubsystem::GetAllClanIDs() const
{
    TArray<FName> Keys;
    ClanStates.GetKeys(Keys);
    return Keys;
}

float UEconomySimulatorSubsystem::ComputeConcessionAcceptanceProbability(FName ClanID) const
{
    const FClanEconomicState* State = ClanStates.Find(ClanID);
    if (!State)
    {
        return 0.f;
    }

    // A clan under drought, weakened politically, and already surrounded by
    // colonial pressure is most likely to capitulate and sign a concession.
    const float P = State->DroughtStress
                  * (1.f - State->PoliticalStrength)
                  * State->ColonialPressure;

    return FMath::Clamp(P, 0.f, 1.f);
}

// ── Monthly processing ────────────────────────────────────────────────────────

void UEconomySimulatorSubsystem::ProcessMonthlyEconomy()
{
    // 1. Recompute political strength for every clan based on current cattle
    //    count, concession accumulation, and colonial pressure.
    for (auto& Pair : ClanStates)
    {
        FClanEconomicState& State = Pair.Value;
        State.PoliticalStrength = State.ComputePoliticalStrength();
    }

    // 2. Apply tribute flows: for each clan, iterate its outbound flows and
    //    transfer the monthly cattle amount.
    for (auto& Pair : ClanStates)
    {
        FClanEconomicState& Payer = Pair.Value;

        for (auto& FlowPair : Payer.TributeFlows)
        {
            const FName  ReceiverID = FlowPair.Key;
            const float  Flow       = FlowPair.Value;   // cattle/month (positive = outbound)

            if (Flow <= 0.f)
            {
                continue;
            }

            FClanEconomicState* Receiver = ClanStates.Find(ReceiverID);
            if (!Receiver)
            {
                continue;
            }

            const int32 Transfer = FMath::Min(
                FMath::FloorToInt(Flow),
                Payer.CattleCount);

            Payer.CattleCount    -= Transfer;
            Receiver->CattleCount += Transfer;
        }
    }

    // 3. Spread colonial concession pressure across clan borders.
    SpreadConcessionPressure();

    // 4. After pressure updates, recompute strength again so quest triggers
    //    see the freshest values.
    for (auto& Pair : ClanStates)
    {
        FClanEconomicState& State = Pair.Value;
        State.PoliticalStrength = State.ComputePoliticalStrength();
    }

    // 5. Check whether any narrative trigger conditions have been crossed.
    CheckQuestTriggers();
}

void UEconomySimulatorSubsystem::SpreadConcessionPressure()
{
    // UConcessionSpreadComponent is stateless for its calculation; create a
    // transient instance to hold the configured SpreadRate.
    UConcessionSpreadComponent* SpreadCalc =
        NewObject<UConcessionSpreadComponent>(GetTransientPackage());
    SpreadCalc->SpreadRate = 0.15f;

    // Collect updated pressures first, then write back — avoids order dependence
    // (one clan's updated pressure should not affect another's calculation in
    // the same tick).
    TMap<FName, float> NewPressures;

    for (const auto& Pair : ClanStates)
    {
        const FName& ClanID      = Pair.Key;
        const FClanEconomicState& Target = Pair.Value;

        // Gather neighbour states (all other clans in the simplified model).
        TArray<FClanEconomicState> NeighbourStates;
        const TArray<FName> NeighbourIDs = FindNeighbourClans(ClanID);
        NeighbourStates.Reserve(NeighbourIDs.Num());

        for (const FName& NID : NeighbourIDs)
        {
            if (const FClanEconomicState* NState = ClanStates.Find(NID))
            {
                NeighbourStates.Add(*NState);
            }
        }

        NewPressures.Add(ClanID,
            SpreadCalc->ComputeSpreadPressure(Target, NeighbourStates));
    }

    // Write back updated pressures.
    for (auto& Pair : ClanStates)
    {
        if (const float* NewP = NewPressures.Find(Pair.Key))
        {
            Pair.Value.ColonialPressure = *NewP;
        }
    }
}

void UEconomySimulatorSubsystem::CheckQuestTriggers()
{
    USimulationBusSubsystem* Bus = GetWorld()
        ? GetWorld()->GetSubsystem<USimulationBusSubsystem>()
        : nullptr;

    if (!Bus)
    {
        return;
    }

    for (const auto& Pair : ClanStates)
    {
        const FClanEconomicState& State = Pair.Value;

        // "RaidOfNeed": desperate clan — severe drought and near-empty herds.
        // Signals to EmergentNarrativePlugin that a raid quest should activate.
        if (State.DroughtStress > DroughtRaidThreshold &&
            State.CattleCount   < CattleRaidThreshold)
        {
            Bus->BroadcastQuestTriggerConditionMet(
                FName("RaidOfNeed"),
                State.DroughtStress);   // condition value = severity proxy
        }

        // "ConcessionBetrayal": clan is being squeezed into signing away land
        // under colonial pressure while too weak to resist.
        if (State.ColonialPressure  > ColonialBetrayalThreshold &&
            State.PoliticalStrength < StrengthBetrayalThreshold)
        {
            Bus->BroadcastQuestTriggerConditionMet(
                FName("ConcessionBetrayal"),
                State.ColonialPressure);  // condition value = pressure level
        }
    }
}

TArray<FName> UEconomySimulatorSubsystem::FindNeighbourClans(FName ClanID) const
{
    // Simplified model: every other registered clan is considered adjacent.
    // Future work: replace with a spatial adjacency graph loaded from the
    // world map data asset.
    TArray<FName> Neighbours;
    Neighbours.Reserve(ClanStates.Num() - 1);

    for (const auto& Pair : ClanStates)
    {
        if (Pair.Key != ClanID)
        {
            Neighbours.Add(Pair.Key);
        }
    }

    return Neighbours;
}
