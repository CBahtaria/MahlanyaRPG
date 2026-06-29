// Copyright Charles Bartaria. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FClanEconomicState.h"
#include "UConcessionSpreadComponent.h"
#include "UEconomySimulatorSubsystem.generated.h"

/**
 * UEconomySimulatorSubsystem
 *
 * UWorldSubsystem that owns the per-clan economic simulation for Mahlanya RPG.
 *
 * Tick cadence: 1 real second = 1 game day. Every 30 accumulated game-days
 * (≈ 1 game-month) ProcessMonthlyEconomy() fires.
 *
 * External inputs:
 *   - MicroclimateEngine calls SetDroughtStress() each frame to push drought index.
 *   - Gameplay systems call RecordLobolaTransaction() and RecordCattleRaid().
 *
 * Outputs (via SimulationBusPlugin):
 *   - FOnSettlementDemographicChanged  → SibayaEngine (Voronoi recompute)
 *   - FOnQuestTriggerConditionMet      → EmergentNarrativePlugin
 *       Tags: "RaidOfNeed", "ConcessionBetrayal"
 */
UCLASS()
class ECONOMYSIMULATORPLUGIN_API UEconomySimulatorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ── UWorldSubsystem interface ────────────────────────────────────────────

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Clan registration ────────────────────────────────────────────────────

    /**
     * Registers a clan with its initial economic state. Call during world load
     * or from GameMode::BeginPlay before the first SimulateEconomyTick.
     * Re-registering the same ClanID overwrites the previous state.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy|Clans")
    void RegisterClan(const FClanEconomicState& InitialState);

    // ── Simulation tick ──────────────────────────────────────────────────────

    /**
     * Advances the economy by GameDayDelta game-days. Intended to be called
     * once per real second (where 1 s = 1 game-day). When accumulated days
     * reach 30 a full monthly economy cycle fires.
     *
     * @param GameDayDelta   Elapsed game-days since last call (typically 1.0).
     */
    UFUNCTION(BlueprintCallable, Category = "Economy|Simulation")
    void SimulateEconomyTick(float GameDayDelta);

    // ── External data feeds ──────────────────────────────────────────────────

    /**
     * Updates a clan's drought stress index. Called by MicroclimateEngine
     * whenever its drought metric changes.
     *
     * @param ClanID       Target clan.
     * @param DroughtIndex Drought severity 0–1 (0 = none, 1 = extreme).
     */
    UFUNCTION(BlueprintCallable, Category = "Economy|Environment")
    void SetDroughtStress(FName ClanID, float DroughtIndex);

    // ── Economic transactions ────────────────────────────────────────────────

    /**
     * Records a lobola (bride-wealth) cattle transfer.
     * Subtracts CattleCount from FromClan, adds to ToClan, then broadcasts
     * FOnSettlementDemographicChanged on the SimulationBus for both clans.
     *
     * @param FromClan    Groom's clan (pays cattle).
     * @param ToClan      Bride's clan (receives cattle).
     * @param CattleCount Head of cattle transferred.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy|Transactions")
    void RecordLobolaTransaction(FName FromClan, FName ToClan, int32 CattleCount);

    /**
     * Records the outcome of a cattle raid. Transfers up to CattleTransferred
     * head from VictimClan to RaiderClan (clamped to victim's available stock).
     *
     * @param RaiderClan        Clan that initiated the raid.
     * @param VictimClan        Clan that was raided.
     * @param CattleTransferred Cattle the raider intends to take.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy|Transactions")
    void RecordCattleRaid(FName RaiderClan, FName VictimClan, int32 CattleTransferred);

    // ── Queries ──────────────────────────────────────────────────────────────

    /** Returns a copy of the current economic state for the given clan.
     *  Returns a default-constructed state if the clan is unknown. */
    UFUNCTION(BlueprintCallable, Category = "Economy|Queries")
    FClanEconomicState GetClanState(FName ClanID) const;

    /** Returns all registered clan IDs in an unordered array. */
    UFUNCTION(BlueprintCallable, Category = "Economy|Queries")
    TArray<FName> GetAllClanIDs() const;

    /**
     * Probability that ClanID accepts a colonial concession this tick.
     * P = DroughtStress * (1 - PoliticalStrength) * ColonialPressure, clamped 0–1.
     * Returns 0 if the clan is unknown.
     */
    UFUNCTION(BlueprintCallable, Category = "Economy|Queries")
    float ComputeConcessionAcceptanceProbability(FName ClanID) const;

private:
    // ── Internal state ───────────────────────────────────────────────────────

    /** Master registry: ClanID → current economic state. */
    TMap<FName, FClanEconomicState> ClanStates;

    /** Game-days accumulated since last monthly cycle. Resets at 30. */
    float AccumulatedDays = 0.f;

    // ── Monthly processing steps ─────────────────────────────────────────────

    /** Fires every 30 accumulated game-days; runs all monthly economy steps. */
    void ProcessMonthlyEconomy();

    /** Updates each clan's ColonialPressure via UConcessionSpreadComponent. */
    void SpreadConcessionPressure();

    /**
     * Checks all clans for narrative trigger conditions and broadcasts on
     * SimulationBus if thresholds are exceeded:
     *   "RaidOfNeed"        – DroughtStress > 0.7 AND CattleCount < 50
     *   "ConcessionBetrayal" – ColonialPressure > 0.6 AND PoliticalStrength < 0.4
     */
    void CheckQuestTriggers();

    /**
     * Returns all clan IDs considered neighbours of ClanID.
     * Current implementation: all other registered clans (simplified graph).
     */
    TArray<FName> FindNeighbourClans(FName ClanID) const;
};
