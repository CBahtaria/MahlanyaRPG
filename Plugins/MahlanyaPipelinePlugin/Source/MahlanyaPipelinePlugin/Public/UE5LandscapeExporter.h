#pragma once

#include "CoreMinimal.h"
#include "MahlanyaPipelineTypes.h"

/**
 * FUE5LandscapeExporter
 *
 * Converts an eroded FDEMGrid into UE5 Landscape-compatible .r16 tiles.
 * Tiles are written as raw little-endian uint16 arrays with no file header,
 * matching the format expected by UE5's Landscape import tool.
 */
class MAHLANYAPIPELINEPLUGIN_API FUE5LandscapeExporter
{
public:
    /**
     * Split the heightmap into TileSize x TileSize .r16 tiles.
     *
     * Files are written to: <OutputDir>/heightmap_tiles/tile_R<row>_C<col>.r16
     *
     * Edge tiles are padded with the nearest valid elevation value.
     * TileSize must be one of the UE5 Landscape-valid sizes:
     *   127, 253, 505, 1009, 2017
     *
     * @param Grid      Source heightmap (float32, row-major).
     * @param OutputDir Root output directory. Sub-folder heightmap_tiles/ is created.
     * @param TileSize  Tile dimension in texels (square). Defaults to 1009.
     * @return true if all tiles were written successfully.
     */
    static bool ExportHeightmapTiles(const FDEMGrid& Grid,
                                      const FString&  OutputDir,
                                      int32           TileSize = 1009);

    /**
     * Write a single raw .r16 file (little-endian uint16 array, no header).
     *
     * @param FilePath  Absolute destination path.
     * @param Pixels    Row-major uint16 pixel data.
     * @param Width     Tile width in texels.
     * @param Height    Tile height in texels.
     * @return true on success.
     */
    static bool WriteR16Tile(const FString&         FilePath,
                              const TArray<uint16>&  Pixels,
                              int32                  Width,
                              int32                  Height);

    /**
     * Map a float elevation value to a uint16 in [0, 65535].
     *
     * @param Elevation  The elevation to convert.
     * @param MinE       The minimum elevation in the grid (maps to 0).
     * @param MaxE       The maximum elevation in the grid (maps to 65535).
     * @return Clamped, linearly-mapped uint16 value.
     */
    static uint16 ElevationToU16(float Elevation, float MinE, float MaxE);
};
