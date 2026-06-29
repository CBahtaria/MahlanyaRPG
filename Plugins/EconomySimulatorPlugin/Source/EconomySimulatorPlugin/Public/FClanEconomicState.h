// Copyright Charles Bartaria. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FClanEconomicState.generated.h"

/**
 * FClanEconomicState
 *
 * Snapshot of a single Swazi clan's economic position at one simulation tick.
 * Cattle are the primary store of wealth, political legitimacy, and military
 * capability. Colonial concession acceptance is irreversible and permanently
 * reduces political strength. Drought stress (injected by MicroclimateEngine)
 * drives raid probability.
 */
USTRUCT(BlueprintType)
struct ECONOMYSIMULATORPLUGIN_API FClanEconomicState
{
    GENERATED_BODY()

    /** Unique identifier matching the settlement/clan record in SibayaEngine. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    FName ClanID;

    /** Head of cattle owned. Primary wealth unit; floor is 0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    int32 CattleCount = 100;

    /**
     * Political strength 0–1. Stored here as the last-computed value so
     * Blueprint reads are cheap; recomputed each monthly tick via
     * ComputePoliticalStrength().
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    float PoliticalStrength = 0.5f;

    /** Accumulated colonial pressure 0–1, spread from adjacent clans. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    float ColonialPressure = 0.f;

    /**
     * Drought stress 0–1 injected by MicroclimateEngine each tick.
     * Higher values raise raid probability.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    float DroughtStress = 0.f;

    /**
     * Net cattle-per-month tribute flows to/from allied clans.
     * Key = ClanID of the other party; positive value = cattle sent to them,
     * negative = cattle received from them.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    TMap<FName, float> TributeFlows;

    /**
     * List of colonial concession titles accepted. Each entry is irreversible
     * and carries an 8% political-strength penalty via ComputePoliticalStrength().
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
    TArray<FName> AcceptedConcessions;

    // ── Derived calculations (recomputed each monthly tick) ──────────────────

    /**
     * P(raid this month) = DroughtStress * (1 - PoliticalStrength) * 0.3
     * Uses the stored PoliticalStrength; call ComputePoliticalStrength() first
     * to ensure freshness.
     */
    float ComputeRaidProbability() const;

    /**
     * Derives political strength from cattle count (log scale), concession
     * penalty, and colonial pressure suppression:
     *   strength = clamp(log(CattleCount+1)/10 - concessions*0.08 - ColonialPressure*0.3, 0, 1)
     */
    float ComputePoliticalStrength() const;
};
