#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EconomySimulatorSubsystem.generated.h"

// ---------------------------------------------------------------------------
// ECattleFlowType
//
// Classifies the nature of a cattle transfer between two settlements.
// Used by the node-based production graph to determine processing rules
// (pattern: adepierre/ficsit-companion).
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class ECattleFlowType : uint8
{
    /** Mandatory periodic payment flowing upward in the political hierarchy. */
    Tribute  UMETA(DisplayName = "Tribute (Mandatory)"),

    /** Voluntary exchange agreed between clans. */
    Trade    UMETA(DisplayName = "Trade (Voluntary)"),

    /** Hostile one-time seizure; does not repeat unless re-triggered. */
    Raid     UMETA(DisplayName = "Raid (Hostile Seizure)"),

    /**
     * Bride-price payment; triggers a settlement demographic recompute
     * which may cascade into Voronoi layout recalculation via SibayaEngine.
     */
    Lobola   UMETA(DisplayName = "Lobola (Bride Price)"),
};

// ---------------------------------------------------------------------------
// FCattleFlowEdge
//
// A directed edge in the cattle-economy node graph.
// Represents cattle flowing from one settlement to another at a given rate.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FCattleFlowEdge
{
    GENERATED_BODY()

    /** Source settlement identifier (must match a FClanEconomicState.ClanID). */
    UPROPERTY(EditAnywhere, Category = "Economy")
    FName FromSettlement;

    /** Destination settlement identifier. */
    UPROPERTY(EditAnywhere, Category = "Economy")
    FName ToSettlement;

    /**
     * Flow rate expressed as cattle per game-month (30 game-days).
     * ApplyTributeFlows() pro-rates this to the current GameDayDelta.
     */
    UPROPERTY(EditAnywhere, Category = "Economy")
    int32 CattleCount = 0;

    /** Semantic type of the transfer; drives processing rules and event queuing. */
    UPROPERTY(EditAnywhere, Category = "Economy")
    ECattleFlowType FlowType = ECattleFlowType::Trade;
};

// ---------------------------------------------------------------------------
// FClanEconomicState
//
// Full economic and political snapshot of a single clan/settlement node.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FClanEconomicState
{
    GENERATED_BODY()

    /** Unique clan identifier, e.g. "Clan_Dlamini". */
    UPROPERTY(EditAnywhere, Category = "Economy")
    FName ClanID;

    /** Current herd size. May never drop below zero. */
    UPROPERTY(EditAnywhere, Category = "Economy")
    int32 CattleCount = 0;

    /**
     * Political strength in [0, 1].
     * High values reduce raid probability and concession acceptance.
     * Corresponds to royal lineage prestige, alliance depth, and military
     * readiness in the Swazi context.
     */
    UPROPERTY(EditAnywhere, Category = "Economy")
    float PoliticalStrength = 0.5f;

    /**
     * Colonial pressure in [0, 1].
     * Driven by proximity to concession-holders and British/Transvaal
     * administrative reach. Fed into ComputeConcessionProbability().
     */
    UPROPERTY(EditAnywhere, Category = "Economy")
    float ColonialPressure = 0.0f;

    /**
     * Drought stress in [0, 1].
     * Injected externally by MicroclimateEngine (via SimulationBusSubsystem).
     * Drives raid probability: starving clans raid more aggressively.
     */
    UPROPERTY(EditAnywhere, Category = "Economy")
    float DroughtStress = 0.0f;

    /**
     * Concessions accepted by this clan.
     * Each entry is an irreversible grant of land, mineral, or trading rights.
     * Appended by CheckConcessionTriggers(); never removed.
     */
    UPROPERTY(EditAnywhere, Category = "Economy")
    TArray<FName> Concessions;

    /**
     * Outbound cattle flow edges originating from this settlement.
     * Iterated each tick by ApplyTributeFlows() (Tribute edges) and
     * referenced by raid/lobola event processing.
     */
    UPROPERTY(EditAnywhere, Category = "Economy")
    TArray<FCattleFlowEdge> OutboundFlows;
};

// ---------------------------------------------------------------------------
// EEconomyEventType
//
// Discrete event types buffered per tick.
// Pattern: RCVolus/league-prod-toolkit discrete event buffer.
// ---------------------------------------------------------------------------
UENUM()
enum class EEconomyEventType : uint8
{
    CattleRaid,
    Lobola,
    Tribute,
    Drought,
};

// ---------------------------------------------------------------------------
// FEconomyEvent
//
// A single queued discrete economy event.
// Events are accumulated in EventBuffer during a tick and processed
// atomically at the start of the next tick by ProcessEventBuffer().
// ---------------------------------------------------------------------------
USTRUCT()
struct FEconomyEvent
{
    GENERATED_BODY()

    /** Type of economy transition this event represents. */
    EEconomyEventType Type = EEconomyEventType::Tribute;

    /** Clan initiating the transfer (raider, payer, etc.). */
    FName SourceClan;

    /** Clan receiving or suffering the transfer. */
    FName TargetClan;

    /**
     * Signed cattle delta applied to TargetClan (+gain / -loss).
     * The symmetric negative is applied to SourceClan.
     */
    int32 CattleDelta = 0;

    /** Game-time stamp (in game-days) when the event was queued. */
    float Timestamp = 0.0f;
};

// ---------------------------------------------------------------------------
// UEconomySimulatorSubsystem
//
// World subsystem implementing the Swazi cattle economy node graph.
//
// Architecture:
//   - Node graph: each FClanEconomicState is a node; FCattleFlowEdges are
//     directed edges (adepierre/ficsit-companion production-graph pattern).
//   - Discrete event buffer: economy events are queued and resolved in batch
//     (RCVolus/league-prod-toolkit pattern) to avoid mid-tick state corruption.
//   - Stochastic triggers: raid and concession probabilities are computed
//     from per-clan stress metrics and sampled each tick.
// ---------------------------------------------------------------------------
UCLASS()
class MAHLANYARPG_API UEconomySimulatorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // -----------------------------------------------------------------------
    // UWorldSubsystem interface
    // -----------------------------------------------------------------------

    /**
     * Seeds the clan node graph with the three initial Swazi clans and
     * initialises the game-time counter.
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // -----------------------------------------------------------------------
    // Tick entry point
    // -----------------------------------------------------------------------

    /**
     * Advance the economy simulation by one game-day increment.
     * Call this once per in-game day from the game-mode or a timer.
     *
     * Order of operations:
     *   1. Accumulate GameTime.
     *   2. ProcessEventBuffer()  — resolve queued discrete events.
     *   3. ApplyTributeFlows()   — pro-rated tribute transfers.
     *   4. CheckRaidTriggers()   — stochastic raid event queuing.
     *   5. CheckConcessionTriggers() — stochastic concession acceptance.
     */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Economy")
    void SimulateEconomyTick(float GameDayDelta);

    // -----------------------------------------------------------------------
    // Probability queries
    // -----------------------------------------------------------------------

    /**
     * P(raid) = DroughtStress * (1 - PoliticalStrength) * 0.3
     * Returns 0 if the clan is not found.
     */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Economy")
    float ComputeRaidProbability(FName ClanID) const;

    /**
     * P(concession) = ColonialPressure * (1 - PoliticalStrength) * 0.4
     * Returns 0 if the clan is not found.
     */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Economy")
    float ComputeConcessionProbability(FName ClanID) const;

    // -----------------------------------------------------------------------
    // Graph mutation
    // -----------------------------------------------------------------------

    /**
     * Add a directed cattle flow edge to the node graph.
     * The edge is stored in the OutboundFlows array of the source clan.
     * If the source clan does not exist, the edge is discarded with a warning.
     */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Economy")
    void AddCattleFlow(const FCattleFlowEdge& Edge);

    // -----------------------------------------------------------------------
    // State queries
    // -----------------------------------------------------------------------

    /**
     * Return a copy of the economic state for the named clan.
     * Returns a default-constructed FClanEconomicState if not found.
     */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Economy")
    FClanEconomicState GetClanState(FName ClanID) const;

    // -----------------------------------------------------------------------
    // Event queuing (called by internal subsystems or external plugins)
    // -----------------------------------------------------------------------

    /**
     * Enqueue a discrete economy event for processing in the next
     * SimulateEconomyTick() call. Thread-safe within the game thread.
     */
    void QueueEvent(const FEconomyEvent& Event);

    // -----------------------------------------------------------------------
    // Delegates
    // -----------------------------------------------------------------------

    /**
     * Fired when a clan's cattle count drops below the critical threshold
     * (currently: fewer than 20 head).
     * @param ClanID        Identifier of the affected clan.
     * @param CattleCount   Current herd size at the moment of the broadcast.
     */
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCattleCritical, FName /*ClanID*/, int32 /*CattleCount*/);
    FOnCattleCritical OnCattleCritical;

    /**
     * Fired when a clan accepts a colonial concession.
     * @param ClanID   Identifier of the clan that accepted the concession.
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnConcessionAccepted, FName /*ClanID*/);
    FOnConcessionAccepted OnConcessionAccepted;

protected:
    // -----------------------------------------------------------------------
    // Internal state
    // -----------------------------------------------------------------------

    /** All clan economic nodes. Seeded by LoadInitialClans(). */
    UPROPERTY()
    TArray<FClanEconomicState> ClanStates;

    /**
     * Discrete event buffer (RCVolus/league-prod-toolkit pattern).
     * Events are appended throughout the tick and flushed atomically by
     * ProcessEventBuffer() at the start of SimulateEconomyTick().
     */
    TArray<FEconomyEvent> EventBuffer;

    /** Accumulated game time in game-days since simulation start. */
    float GameTime = 0.0f;

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    /**
     * Process all events in EventBuffer, apply cattle deltas, log each event,
     * then clear the buffer.
     */
    void ProcessEventBuffer();

    /**
     * For each clan, iterate its OutboundFlows of type Tribute and transfer
     * the pro-rated cattle amount (CattleCount / 30 * GameDayDelta) from
     * source to destination.
     */
    void ApplyTributeFlows(float GameDayDelta);

    /**
     * For each clan, compute P(raid) and compare against a uniform random
     * sample scaled by GameDayDelta. If triggered, queue a CattleRaid event
     * against a randomly chosen different clan.
     */
    void CheckRaidTriggers(float GameDayDelta);

    /**
     * For each clan, compute P(concession) and compare against a uniform
     * random sample. If triggered, append a new concession to the clan's
     * Concessions array and broadcast OnConcessionAccepted.
     */
    void CheckConcessionTriggers();

    /**
     * Seed the node graph with the three founding Swazi clans.
     * Called once from Initialize().
     */
    void LoadInitialClans();

    // -----------------------------------------------------------------------
    // Utility
    // -----------------------------------------------------------------------

    /** Find a mutable pointer to a clan state by ID; nullptr if not found. */
    FClanEconomicState* FindClan(FName ClanID);

    /** Find a const pointer to a clan state by ID; nullptr if not found. */
    const FClanEconomicState* FindClan(FName ClanID) const;

    /**
     * Apply a signed cattle delta to a clan, clamping to zero from below.
     * Broadcasts OnCattleCritical if the count crosses the critical threshold.
     */
    void ApplyCattleDelta(FName ClanID, int32 Delta);

    /** Cattle count at or below which OnCattleCritical is broadcast. */
    static constexpr int32 CattleCriticalThreshold = 20;
};
