#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimulationBusSubsystem.generated.h"

/**
 * USimulationBusSubsystem
 *
 * Typed multicast delegate event bus for Mahlanya simulation systems.
 * Loaded PreDefault (first plugin in the load order) so that every
 * simulation plugin can subscribe to delegates in its own BeginPlay/Init
 * without worrying about ordering.
 *
 * No other Mahlanya plugin is listed as a dependency here.
 * This is the only plugin in the dependency graph with zero inbound edges.
 *
 * ── Delegate registry ──────────────────────────────────────────────────────
 *
 * Publisher                → Subscriber(s)
 * MicroclimateEngine       → ErosionRuntimePlugin, LocomotionPhysicsPlugin
 * ErosionRuntimePlugin     → MicroclimateEngine
 * EconomySimulatorPlugin   → SibayaEngine
 * Any plugin               → EmergentNarrativePlugin
 * MicroclimateEngine       → GeometricAudioPlugin, EmergentNarrativePlugin
 * EcologySimulatorPlugin   → EmergentNarrativePlugin
 */
UCLASS()
class SIMULATIONBUSPLUGIN_API USimulationBusSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ── Delegate declarations ────────────────────────────────────────────────

    /** Published by MicroclimateEngine. Intensity in mm/hr.
     *  Subscribed by: ErosionRuntimePlugin, LocomotionPhysicsPlugin. */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnRainIntensityChanged, float /*IntensityMmPerHr*/);
    FOnRainIntensityChanged OnRainIntensityChanged;

    /** Published by ErosionRuntimePlugin. Saturation 0–1.
     *  Subscribed by: MicroclimateEngine (feedback for fog density). */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnTerrainSaturationChanged, float /*SaturationFraction*/);
    FOnTerrainSaturationChanged OnTerrainSaturationChanged;

    /** Published by EconomySimulatorPlugin on demographic change.
     *  Subscribed by: SibayaEngine (recomputes Voronoi layout). */
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSettlementDemographicChanged,
        FName /*SettlementID*/, int32 /*NewWifeCount*/);
    FOnSettlementDemographicChanged OnSettlementDemographicChanged;

    /** Published by any simulation subsystem when a trigger condition is met.
     *  Subscribed by: EmergentNarrativePlugin.
     *  ConditionTag example: "DroughtStress_Critical", "ColonialPressure_High" */
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnQuestTriggerConditionMet,
        FName /*ConditionTag*/, float /*ConditionValue*/);
    FOnQuestTriggerConditionMet OnQuestTriggerConditionMet;

    /** Published by EcologySimulatorPlugin when prey/predator population
     *  falls below a critical threshold. Triggers NPC dialogue and quests.
     *  Subscribed by: EmergentNarrativePlugin. */
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSpeciesPopulationCritical,
        FName /*SpeciesID*/, float /*CurrentPopulationFraction*/);
    FOnSpeciesPopulationCritical OnSpeciesPopulationCritical;

    /** Published by MicroclimateEngine::ULightningSystem when a strike lands.
     *  Subscribed by: EmergentNarrativePlugin (Umkhosi Wemali sacred tree), SibayaEngine. */
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLightningStrike,
        FVector /*WorldLocation*/, bool /*bStruckSacredTree*/);
    FOnLightningStrike OnLightningStrike;

    // ── Convenience broadcast helpers ────────────────────────────────────────

    void BroadcastRainIntensityChanged(float IntensityMmPerHr)
    {
        OnRainIntensityChanged.Broadcast(IntensityMmPerHr);
    }

    void BroadcastTerrainSaturationChanged(float SaturationFraction)
    {
        OnTerrainSaturationChanged.Broadcast(SaturationFraction);
    }

    void BroadcastSettlementDemographicChanged(FName SettlementID, int32 NewWifeCount)
    {
        OnSettlementDemographicChanged.Broadcast(SettlementID, NewWifeCount);
    }

    void BroadcastQuestTriggerConditionMet(FName ConditionTag, float ConditionValue)
    {
        OnQuestTriggerConditionMet.Broadcast(ConditionTag, ConditionValue);
    }

    void BroadcastSpeciesPopulationCritical(FName SpeciesID, float PopulationFraction)
    {
        OnSpeciesPopulationCritical.Broadcast(SpeciesID, PopulationFraction);
    }

    void BroadcastLightningStrike(FVector WorldLocation, bool bStruckSacredTree)
    {
        OnLightningStrike.Broadcast(WorldLocation, bStruckSacredTree);
    }
};
