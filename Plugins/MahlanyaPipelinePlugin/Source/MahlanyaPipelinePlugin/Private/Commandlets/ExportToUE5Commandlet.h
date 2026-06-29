#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ExportToUE5Commandlet.generated.h"

/**
 * UExportToUE5Commandlet
 *
 * Converts an eroded DEM (raw float32 binary or .r16) to UE5 Landscape
 * .r16 tiles using FUE5LandscapeExporter.
 *
 * Usage:
 *   UnrealEditor-Cmd MahlanyaRPG.uproject \
 *       -run=ExportToUE5 \
 *       -dem=<path_to_raw_float32_or_r16> \
 *       -output=<output_directory> \
 *       -tile_size=1009
 *
 * Valid tile sizes: 127, 253, 505, 1009, 2017
 */
UCLASS()
class UExportToUE5Commandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UExportToUE5Commandlet();

    //~ Begin UCommandlet interface
    virtual int32 Main(const FString& Params) override;
    //~ End UCommandlet interface

private:
    /** Load a raw float32 binary grid (square, inferred from file size). */
    static bool LoadRawFloat32(const FString& FilePath,
                                TArray<float>& OutData,
                                int32& OutWidth,
                                int32& OutHeight);

    /** Load a .r16 (raw uint16, no header) grid and return values in [0,1]. */
    static bool LoadR16AsFloat(const FString& FilePath,
                                TArray<float>& OutData,
                                int32& OutWidth,
                                int32& OutHeight);
};
