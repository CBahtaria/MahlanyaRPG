#pragma once
#include "CoreMinimal.h"
#include "SibayaEngineTypes.h"

/**
 * FVoronoiSolver
 *
 * C++ Lloyd centroidal Voronoi relaxation using the Zig native compute library
 * (libmahlanya_compute.so) via FPlatformProcess::GetDllHandle.
 *
 * Falls back to a pure C++ Lloyd implementation when the Zig library is absent
 * (ensures the plugin works in editor without the native library built).
 */
class SIBAYAENGINE_API FVoronoiSolver
{
public:
    /**
     * Run Lloyd relaxation on the given set of 2D points in [0,1]².
     *
     * Taboo arc [240°, 300°] relative to Sibaya at (0.5, 0.5) is always
     * excluded; points that drift into it are reflected back.
     *
     * @param InOutPoints  Points in normalised [0,1]² space, modified in place.
     * @param Iterations   Lloyd iteration count (default 50).
     */
    static void LloydRelax(TArray<FVector2D>& InOutPoints, int32 Iterations = 50);

    /**
     * Compute the full Voronoi layout for a settlement.
     *
     * @param NumWives       Number of wives (each gets a hut cell).
     * @param NumSons        Number of adult sons (Lilawu / bachelor quarters).
     * @param NumDependents  Number of dependents (Guma enclosures).
     * @param CattleCount    Cattle count; modulates Sibaya cell area.
     * @param TerrainGradient  Magnitude of terrain slope at settlement [0,1].
     * @return               Resolved layout with all hut positions in [0,1]².
     */
    static FVoronoiLayout ComputeLayout(int32 NumWives, int32 NumSons,
                                         int32 NumDependents, int32 CattleCount,
                                         float TerrainGradient);

private:
    /** Attempt to load libmahlanya_compute.so. Returns nullptr if unavailable. */
    static void* TryLoadZigLib();

    /** Pure C++ fallback Lloyd: iterate centroid of Voronoi cells. */
    static void LloydRelaxCPP(TArray<FVector2D>& Points, int32 Iterations);

    /** Returns true if the bearing from (0.5,0.5) to Point is in [240°,300°]. */
    static bool IsInTabooArc(const FVector2D& Point);

    /** Reflects a point out of the taboo arc by mirroring its bearing. */
    static FVector2D ReflectFromTabooArc(const FVector2D& Point);
};
