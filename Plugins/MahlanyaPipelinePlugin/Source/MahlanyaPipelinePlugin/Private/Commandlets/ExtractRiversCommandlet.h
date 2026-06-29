#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "MahlanyaPipelineTypes.h"
#include "ExtractRiversCommandlet.generated.h"

/**
 * UExtractRiversCommandlet
 *
 * Extracts river networks from an eroded DEM using D-infinity hydrology
 * kernels from libmahlanya_compute.so.
 *
 * Pipeline:
 *   1. Fill topographic pits (Wang & Liu)
 *   2. Compute D-infinity flow directions (Tarboton 1997)
 *   3. Compute upstream contributing-area accumulation
 *   4. Threshold accumulation to identify stream cells
 *   5. Trace polylines and assign Strahler stream orders
 *   6. Write a CSV of river vertices
 *
 * Usage:
 *   UnrealEditor-Cmd MahlanyaRPG.uproject \
 *       -run=ExtractRivers \
 *       -dem=<path_to_raw_float32> \
 *       -output=<output_directory> \
 *       -threshold=1000
 *
 * Output CSV format (one row per vertex):
 *   river_name,x,y,strahler_order
 */
UCLASS()
class UExtractRiversCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UExtractRiversCommandlet();

    //~ Begin UCommandlet interface
    virtual int32 Main(const FString& Params) override;
    //~ End UCommandlet interface

private:
    /** Load a raw float32 binary file (square grid inferred from file size). */
    static bool LoadRawFloat32(const FString& FilePath,
                                TArray<float>& OutData,
                                int32& OutWidth,
                                int32& OutHeight);

    /**
     * Threshold the accumulation grid and trace river polylines.
     * Cells with accumulation >= Threshold are stream cells.
     * Simple 8-connectivity tracing; Strahler orders computed bottom-up.
     */
    static FRiverNetwork TraceRiverNetwork(const TArray<float>& Accum,
                                            int32 Width,
                                            int32 Height,
                                            float Threshold);

    /** Determine Strahler order for each river polyline by traversal. */
    static void AssignStrahlerOrders(FRiverNetwork& Network);

    /** Write the FRiverNetwork to a CSV file. */
    static bool WriteRiverCSV(const FRiverNetwork& Network, const FString& FilePath);

    /** Resolve path to libmahlanya_compute.so. */
    static FString ResolveLibPath();
};
