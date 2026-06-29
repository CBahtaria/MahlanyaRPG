// Copyright Charles Bartaria. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FClanEconomicState.h"
#include "UConcessionSpreadComponent.generated.h"

/**
 * UConcessionSpreadComponent
 *
 * Stateless calculation component that models the lateral spread of British
 * colonial concession pressure between adjacent Swazi clans. When a border
 * chief signs a concession the resulting legal precedent and diplomatic
 * leverage raises pressure on all neighbouring clans.
 *
 * Attach to any Actor that needs to run concession-spread calculations
 * (typically the WorldSettings actor or a dedicated economy manager Actor).
 * The subsystem creates a transient instance internally for tick calculations.
 */
UCLASS(ClassGroup = ("EconomySimulator"), meta = (BlueprintSpawnableComponent))
class ECONOMYSIMULATORPLUGIN_API UConcessionSpreadComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /**
     * Pressure increase added per neighbouring clan that has accepted at
     * least one concession. Units: colonial-pressure-points per game-month.
     * Default 0.15 means three concession-bearing neighbours push a clan to
     * maximum pressure within ~2 in-game years.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy|Concession")
    float SpreadRate = 0.15f;

    /**
     * Returns the new ColonialPressure value for TargetClan after applying
     * one month of spread from its Neighbours.
     *
     * For each neighbour that has accepted at least one concession,
     * SpreadRate is added to the current pressure, then the result is
     * clamped to [0, 1].
     *
     * @param TargetClan   The clan whose pressure we are updating.
     * @param Neighbours   All clans that share a border with TargetClan.
     * @return             New ColonialPressure value clamped to [0, 1].
     */
    UFUNCTION(BlueprintCallable, Category = "Economy|Concession")
    float ComputeSpreadPressure(const FClanEconomicState& TargetClan,
                                const TArray<FClanEconomicState>& Neighbours) const;
};
