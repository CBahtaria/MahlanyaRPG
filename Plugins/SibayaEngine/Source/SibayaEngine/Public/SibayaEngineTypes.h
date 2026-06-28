#pragma once
#include "CoreMinimal.h"
#include "SibayaEngineTypes.generated.h"

/** Opaque settlement identifier */
USTRUCT(BlueprintType)
struct SIBAYAENGINE_API FSettlementID
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FName ID;
    bool operator==(const FSettlementID& O) const { return ID == O.ID; }
};
FORCEINLINE uint32 GetTypeHash(const FSettlementID& S) { return GetTypeHash(S.ID); }

/** Opaque hut identifier */
USTRUCT(BlueprintType)
struct SIBAYAENGINE_API FHutID
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FName ID;
};

/** Demographic data for a newly married wife */
USTRUCT(BlueprintType)
struct SIBAYAENGINE_API FWifeData
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FName WifeID;
    UPROPERTY(BlueprintReadWrite) int32 SeniorityRank = 1;
    UPROPERTY(BlueprintReadWrite) int32 CattleLobola = 11;
};

/** Resolved 2D Voronoi layout for a settlement */
USTRUCT(BlueprintType)
struct SIBAYAENGINE_API FVoronoiLayout
{
    GENERATED_BODY()
    /** Hut centre positions in normalised [0,1]² space */
    UPROPERTY(BlueprintReadOnly) TArray<FVector2D> HutPositions;
    /** Social role weight per hut, parallel to HutPositions */
    UPROPERTY(BlueprintReadOnly) TArray<float> HierarchicalWeights;
};

/** Directed political relation between two settlements */
USTRUCT(BlueprintType)
struct SIBAYAENGINE_API FSettlementRelation
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FSettlementID From;
    UPROPERTY(BlueprintReadOnly) FSettlementID To;
    UPROPERTY(BlueprintReadOnly) FName RelationType;     // "ally", "vassal", "feud"
    UPROPERTY(BlueprintReadOnly) int32 CattleTribute = 0;
    UPROPERTY(BlueprintReadOnly) float DistanceKm = 0.f;
};
