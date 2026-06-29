#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SibayaEngineTypes.h"
#include "USibayaEngineSubsystem.generated.h"

/**
 * USibayaEngineSubsystem
 *
 * World subsystem managing all Umuti settlements. Handles demographic events
 * and triggers Voronoi recomputation when the settlement layout must change.
 * Publishes FOnSettlementDemographicChanged to SimulationBusPlugin.
 */
UCLASS()
class SIBAYAENGINE_API USibayaEngineSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    //~ Begin UWorldSubsystem Interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    //~ End UWorldSubsystem Interface

    /** Called when a new wife joins the settlement; triggers Voronoi recompute. */
    UFUNCTION(BlueprintCallable, Category = "Sibaya|Demographics")
    void OnWifeMarried(FSettlementID SettlementID, FWifeData NewWife);

    /** Called when a hut is destroyed; triggers ruin overlay + recompute. */
    UFUNCTION(BlueprintCallable, Category = "Sibaya|Demographics")
    void OnHutDestroyed(FSettlementID SettlementID, FHutID HutID);

    /** Called when cattle are lost to a raid; updates economy state. */
    UFUNCTION(BlueprintCallable, Category = "Sibaya|Demographics")
    void OnCattleRaid(FSettlementID SettlementID, int32 CattleLost);

    /** Merges two settlements into one; recomputes unified layout. */
    UFUNCTION(BlueprintCallable, Category = "Sibaya|Demographics")
    void OnFamilyMerge(FSettlementID SettlementA, FSettlementID SettlementB);

    /** Returns the current Voronoi layout for a settlement. */
    UFUNCTION(BlueprintCallable, Category = "Sibaya|Query")
    FVoronoiLayout GetLayout(FSettlementID SettlementID) const;

    /** Register a new settlement with initial demographic data. */
    UFUNCTION(BlueprintCallable, Category = "Sibaya|Management")
    void RegisterSettlement(FSettlementID SettlementID,
                             int32 NumWives, int32 NumSons,
                             int32 NumDependents, int32 CattleCount,
                             float TerrainGradient);

private:
    struct FSettlementData
    {
        int32 NumWives        = 0;
        int32 NumSons         = 0;
        int32 NumDependents   = 0;
        int32 CattleCount     = 0;
        float TerrainGradient = 0.f;
        FVoronoiLayout Layout;
        TArray<FHutID> DestroyedHuts;
    };

    TMap<FSettlementID, FSettlementData> Settlements;

    /** Recompute Voronoi layout for a single settlement. */
    void RecomputeLayout(FSettlementID SettlementID);

    /** Publish demographic change to SimulationBusPlugin. */
    void PublishDemographicChange(FSettlementID SettlementID);
};
