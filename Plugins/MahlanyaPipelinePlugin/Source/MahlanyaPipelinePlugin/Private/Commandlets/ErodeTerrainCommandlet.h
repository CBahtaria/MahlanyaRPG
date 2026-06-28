#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ErodeTerrainCommandlet.generated.h"

/**
 * UErodeTerrainCommandlet
 *
 * Runs the full multi-process erosion pipeline on a DEM heightmap using the
 * Zig SIMD compute kernels in libmahlanya_compute.so, then exports the result
 * as UE5 Landscape .r16 tiles.
 *
 * Usage:
 *   UnrealEditor-Cmd MahlanyaRPG.uproject \
 *       -run=ErodeTerrain \
 *       -dem=<path_to_raw_float32_or_r16> \
 *       -hardness=<path_to_raw_float32> \
 *       -output=<output_directory> \
 *       -iterations=5000
 */
UCLASS()
class UErodeTerrainCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UErodeTerrainCommandlet();

    //~ Begin UCommandlet interface
    virtual int32 Main(const FString& Params) override;
    //~ End UCommandlet interface

private:
    /** Determine the path to libmahlanya_compute.so, preferring the
     *  MAHLANYA_ZIG_LIB_PATH environment variable when set. */
    static FString ResolveLibPath();

    /** Load a raw float32 binary file into an array.
     *  Width * Height floats, row-major, native byte-order. */
    static bool LoadRawFloat32(const FString& FilePath,
                                TArray<float>& OutData,
                                int32& OutWidth,
                                int32& OutHeight);

    /** Load a 16-bit raw .r16 file and convert to float32 [0,1].
     *  Width and Height must be provided via the companion .json sidecar or
     *  command-line flags; here we infer a square grid from the file size. */
    static bool LoadR16AsFloat(const FString& FilePath,
                                TArray<float>& OutData,
                                int32& OutWidth,
                                int32& OutHeight);
};
