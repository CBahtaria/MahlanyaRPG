#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SibayaEngineTypes.h"
#include "PoliticalGraphComponent.generated.h"

/**
 * UPoliticalGraphComponent
 *
 * Attaches to the GameMode or a manager Actor. Stores the directed inter-settlement
 * political graph loaded from political_graph.ndjson (output of build_political_graph.py).
 * Provides Blueprint-callable query interface.
 */
UCLASS(ClassGroup=("SibayaEngine"), meta=(BlueprintSpawnableComponent))
class SIBAYAENGINE_API UPoliticalGraphComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPoliticalGraphComponent();

    /** Load graph from NDJSON file at runtime. */
    UFUNCTION(BlueprintCallable, Category = "Political Graph")
    bool LoadFromFile(const FString& FilePath);

    /** All settlements that are allied with the given settlement. */
    UFUNCTION(BlueprintCallable, Category = "Political Graph")
    TArray<FSettlementRelation> GetAlliesOf(FSettlementID SettlementID) const;

    /** Cattle tribute from one settlement to another (0 if no tribute relation). */
    UFUNCTION(BlueprintCallable, Category = "Political Graph")
    int32 GetCattleTribute(FSettlementID From, FSettlementID To) const;

    /**
     * Approximate trade route path between settlements.
     * Returns a list of world-space waypoints (simplified — straight line if
     * no pathfinding data loaded; full A* when DEM is loaded).
     */
    UFUNCTION(BlueprintCallable, Category = "Political Graph")
    TArray<FVector> GetTradeRoutePath(FSettlementID From, FSettlementID To) const;

    /** True if the two settlements are in active feud. */
    UFUNCTION(BlueprintCallable, Category = "Political Graph")
    bool IsInFeud(FSettlementID A, FSettlementID B) const;

    /** Add or update a relation. Used by EconomySimulatorSubsystem at runtime. */
    UFUNCTION(BlueprintCallable, Category = "Political Graph")
    void UpsertRelation(const FSettlementRelation& Relation);

private:
    /** All directed edges keyed by FromSettlement */
    TMap<FSettlementID, TArray<FSettlementRelation>> AdjacencyList;

    /** Stored world-space positions for each settlement (loaded from JSON) */
    TMap<FSettlementID, FVector> SettlementPositions;
};
