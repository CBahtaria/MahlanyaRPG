#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ComputeVoronoiCommandlet.generated.h"

/**
 * UComputeVoronoiCommandlet
 *
 * Generates Centroidal Voronoi Tessellation for Umuti settlement layout
 * using the Lloyd relaxation kernel from libmahlanya_compute.so.
 * Enforces taboo arc exclusion (240°–300° = luhlanga zone).
 *
 * Usage:
 *   UnrealEditor-Cmd MahlanyaRPG.uproject \
 *       -run=ComputeVoronoi \
 *       -n_points=24 \
 *       -iters=50 \
 *       -output=<output_json_path> \
 *       [-taboo_start=240] \
 *       [-taboo_end=300]
 *
 * Output JSON:
 *   {"points": [[x,y], ...], "areas": [f, ...]}
 */
UCLASS()
class UComputeVoronoiCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UComputeVoronoiCommandlet();

    //~ Begin UCommandlet interface
    virtual int32 Main(const FString& Params) override;
    //~ End UCommandlet interface

private:
    /** Resolve path to libmahlanya_compute.so. */
    static FString ResolveLibPath();

    /** Generate N uniformly random points in [0,1]². */
    static TArray<double> RandomPoints(int32 N, int32 Seed = 42);
};
