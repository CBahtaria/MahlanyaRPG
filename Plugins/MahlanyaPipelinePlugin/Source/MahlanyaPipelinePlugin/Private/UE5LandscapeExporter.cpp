// Copyright Charles Bartaria. All Rights Reserved.

#include "UE5LandscapeExporter.h"
#include "MahlanyaPipelineTypes.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{
    /** UE5 Landscape import requires one of these exact tile edge lengths. */
    static const int32 ValidTileSizes[] = { 127, 253, 505, 1009, 2017 };

    static bool IsTileSizeValid(int32 TileSize)
    {
        for (int32 S : ValidTileSizes)
        {
            if (S == TileSize) return true;
        }
        return false;
    }
}

// ---------------------------------------------------------------------------
// ElevationToU16
// ---------------------------------------------------------------------------

uint16 FUE5LandscapeExporter::ElevationToU16(float Elevation, float MinE, float MaxE)
{
    if (MaxE <= MinE)
    {
        // Degenerate range — return mid-point
        return 32767u;
    }

    const float Normalised = (Elevation - MinE) / (MaxE - MinE);
    const float Clamped    = FMath::Clamp(Normalised, 0.f, 1.f);
    return static_cast<uint16>(FMath::RoundToInt(Clamped * 65535.f));
}

// ---------------------------------------------------------------------------
// WriteR16Tile
// ---------------------------------------------------------------------------

bool FUE5LandscapeExporter::WriteR16Tile(const FString&        FilePath,
                                          const TArray<uint16>& Pixels,
                                          int32                 Width,
                                          int32                 Height)
{
    if (Pixels.Num() != Width * Height)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("WriteR16Tile: pixel count %d does not match %dx%d=%d"),
               Pixels.Num(), Width, Height, Width * Height);
        return false;
    }

    // FFileHelper works in bytes; cast the uint16 buffer to a byte view.
    const uint8* BytePtr   = reinterpret_cast<const uint8*>(Pixels.GetData());
    const int32  ByteCount = Pixels.Num() * sizeof(uint16);

    // TArrayView<const uint8> so FFileHelper::SaveArrayToFile can accept it
    TArrayView<const uint8> ByteView(BytePtr, ByteCount);

    if (!FFileHelper::SaveArrayToFile(ByteView, *FilePath))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("WriteR16Tile: failed to write '%s'"), *FilePath);
        return false;
    }

    UE_LOG(LogMahlanyaPipeline, Verbose,
           TEXT("WriteR16Tile: wrote %dx%d tile (%d bytes) -> '%s'"),
           Width, Height, ByteCount, *FilePath);
    return true;
}

// ---------------------------------------------------------------------------
// ExportHeightmapTiles
// ---------------------------------------------------------------------------

bool FUE5LandscapeExporter::ExportHeightmapTiles(const FDEMGrid& Grid,
                                                   const FString&  OutputDir,
                                                   int32           TileSize)
{
    // Validate tile size
    if (!IsTileSizeValid(TileSize))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ExportHeightmapTiles: TileSize %d is not a valid UE5 Landscape size."
                    " Valid sizes: 127, 253, 505, 1009, 2017."),
               TileSize);
        return false;
    }

    if (Grid.Width <= 0 || Grid.Height <= 0 || Grid.Data.Num() == 0)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ExportHeightmapTiles: Grid is empty or has invalid dimensions (%dx%d)."),
               Grid.Width, Grid.Height);
        return false;
    }

    // Create output sub-directory
    const FString TileDir = FPaths::Combine(OutputDir, TEXT("heightmap_tiles"));
    if (!IFileManager::Get().MakeDirectory(*TileDir, /*bTree=*/true))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ExportHeightmapTiles: could not create directory '%s'"), *TileDir);
        return false;
    }

    // Tile grid dimensions (ceiling division)
    const int32 RowCount = (Grid.Height + TileSize - 1) / TileSize;
    const int32 ColCount = (Grid.Width  + TileSize - 1) / TileSize;

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("ExportHeightmapTiles: grid=%dx%d, tileSize=%d, tiles=%dx%d, outputDir='%s'"),
           Grid.Width, Grid.Height, TileSize, ColCount, RowCount, *TileDir);

    TArray<uint16> TilePixels;
    TilePixels.SetNumUninitialized(TileSize * TileSize);

    bool bAllOk = true;

    for (int32 TileRow = 0; TileRow < RowCount; ++TileRow)
    {
        for (int32 TileCol = 0; TileCol < ColCount; ++TileCol)
        {
            // Grid origin for this tile
            const int32 OriginRow = TileRow * TileSize;
            const int32 OriginCol = TileCol * TileSize;

            // Fill tile pixels, clamping to the grid edge (edge-padding)
            for (int32 LocalRow = 0; LocalRow < TileSize; ++LocalRow)
            {
                for (int32 LocalCol = 0; LocalCol < TileSize; ++LocalCol)
                {
                    const int32 GridRow = FMath::Min(OriginRow + LocalRow, Grid.Height - 1);
                    const int32 GridCol = FMath::Min(OriginCol + LocalCol, Grid.Width  - 1);

                    const float Elev = Grid.At(GridRow, GridCol);
                    TilePixels[LocalRow * TileSize + LocalCol] =
                        ElevationToU16(Elev, Grid.MinElevation, Grid.MaxElevation);
                }
            }

            const FString FileName  = FString::Printf(TEXT("tile_R%d_C%d.r16"), TileRow, TileCol);
            const FString FilePath  = FPaths::Combine(TileDir, FileName);

            if (!WriteR16Tile(FilePath, TilePixels, TileSize, TileSize))
            {
                bAllOk = false;
                // Continue trying to write the remaining tiles
            }
        }
    }

    if (bAllOk)
    {
        UE_LOG(LogMahlanyaPipeline, Log,
               TEXT("ExportHeightmapTiles: successfully wrote %d tiles to '%s'"),
               RowCount * ColCount, *TileDir);
    }
    else
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ExportHeightmapTiles: one or more tiles failed to write."));
    }

    return bAllOk;
}
