#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UmutiSettlementEngine.generated.h"

/**
 * ESwaziHutType
 * Encodes the social role and hierarchical function of each structure
 * within a Swazi Umuti (homestead), ordered by decreasing authority.
 */
UENUM(BlueprintType)
enum class ESwaziHutType : uint8
{
    Sibaya           UMETA(DisplayName = "Cattle Kraal (Origin Point)"),
    IndluLeyinkhulu  UMETA(DisplayName = "Great House (Queen Mother Apex)"),
    Inkhundla        UMETA(DisplayName = "Public Meeting Ground"),
    Inthwasa         UMETA(DisplayName = "Healer Devotional Sanctuary"),
    Lilawu           UMETA(DisplayName = "Bachelor Quarters (East Gate Defence)"),
    Guma             UMETA(DisplayName = "Standard Domestic Enclosure"),
};

/**
 * FSwaziHutNode
 * A single resolved hut position in the Voronoi tessellation.
 * Consumed by SibayaEngine subsystem and PCG Framework for asset instancing.
 */
USTRUCT(BlueprintType)
struct SIBAYAENGINE_API FSwaziHutNode
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Swazi Geometry")
    ESwaziHutType HutType = ESwaziHutType::Guma;

    /** World-space position of the hut (Landscape Z = terrain height at this XY). */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Geometry")
    FVector WorldLocation = FVector::ZeroVector;

    /** Hut faces inward toward Sibaya unless otherwise constrained. */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Geometry")
    FRotator WorldRotation = FRotator::ZeroRotator;

    /**
     * Social importance weight [0–1].
     * IndluLeyinkhulu = 0.95, senior wife = 0.75, junior wife descends by 0.02/rank.
     * Used by PCG to select mesh variant (large/medium/small) and LOD priority.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Geometry")
    float HierarchicalWeight = 0.5f;

    /** Seniority rank (0 = chief/queen, 1 = first wife, …). */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Geometry")
    int32 SeniorityRank = 0;
};

/**
 * AUmutiSettlementEngine
 *
 * Generates a culturally accurate Swazi homestead layout using constrained
 * Voronoi tessellation. Consumed by USibayaEngineSubsystem.
 *
 * Social geometry enforced:
 *   - Sibaya (cattle kraal) at the geometric origin.
 *   - IndluLeyinkhulu (queen mother apex) due West of Sibaya, facing East.
 *   - Lilawu (bachelor quarters) at the East gate, facing inward.
 *   - Wives' huts arranged in alternating arcs, distance = d_base / rank^0.6.
 *   - Taboo arc: 240°–300° (due West) is spiritually forbidden (luhlanga zone).
 *   - Gate: single entrance at due South (between bachelor quarters and Guma flanks).
 */
UCLASS(BlueprintType)
class SIBAYAENGINE_API AUmutiSettlementEngine : public AActor
{
    GENERATED_BODY()

public:
    AUmutiSettlementEngine();

    /**
     * Generate the full homestead matrix for a settlement.
     *
     * @param SettlementOrigin  World-space location of the Sibaya centre.
     * @param Radius            Settlement radius in cm (UE5 units).
     * @param TotalWives        Number of wives (drives arc tessellation count).
     * @return                  Array of FSwaziHutNode, ordered by HierarchicalWeight descending.
     */
    UFUNCTION(BlueprintCallable, Category = "Swazi Geometry Engine")
    TArray<FSwaziHutNode> GenerateHomesteadMatrix(
        FVector SettlementOrigin,
        float   Radius,
        int32   TotalWives);

    /**
     * Run Lloyd's centroidal Voronoi relaxation on existing hut positions.
     * Called after GenerateHomesteadMatrix to resolve spatial equilibrium
     * and ensure no two huts overlap on the terrain gradient field.
     *
     * @param Iterations  Number of Lloyd relaxation steps (default 50).
     */
    UFUNCTION(BlueprintCallable, Category = "Swazi Geometry Engine")
    void RunLloydRelaxation(int32 Iterations = 50);

private:
    /** Cached hut nodes from the last GenerateHomesteadMatrix call. */
    TArray<FSwaziHutNode> CachedHutNodes;

    static constexpr float TABOO_ARC_START_DEG = 240.0f;
    static constexpr float TABOO_ARC_END_DEG   = 300.0f;

    /** True if the given bearing (degrees) falls inside the luhlanga taboo arc. */
    static bool IsInTabooArc(float BearingDeg);
};
