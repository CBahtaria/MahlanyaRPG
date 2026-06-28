#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMahlanyaPipeline, Log, All);

// ---------------------------------------------------------------------------
// FDEMGrid
// Float32 heightmap stored in row-major order.
// ---------------------------------------------------------------------------
struct FDEMGrid
{
    TArray<float> Data;
    int32 Width        = 0;
    int32 Height       = 0;
    float MinElevation = 0.f;
    float MaxElevation = 0.f;

    float& At(int32 Row, int32 Col)
    {
        return Data[Row * Width + Col];
    }

    const float& At(int32 Row, int32 Col) const
    {
        return Data[Row * Width + Col];
    }

    float* RawPtr() { return Data.GetData(); }
};

// ---------------------------------------------------------------------------
// FHardnessGrid
// Float32 rock hardness values in the range [0, 1], row-major order.
// ---------------------------------------------------------------------------
struct FHardnessGrid
{
    TArray<float> Data;
    int32 Width  = 0;
    int32 Height = 0;

    float* RawPtr() { return Data.GetData(); }
};

// ---------------------------------------------------------------------------
// FRiverNetwork
// River polylines extracted from a D-infinity flow-accumulation raster.
// ---------------------------------------------------------------------------
struct FRiverNetwork
{
    /** UTM coordinates for every vertex across all polylines. */
    TArray<FVector2D> Vertices;

    /** Number of vertices belonging to each polyline (parallel with RiverNames). */
    TArray<int32> VertexCounts;

    /** Human-readable name per polyline; empty string if unnamed. */
    TArray<FString> RiverNames;

    /** Strahler stream order per polyline. */
    TArray<int32> StrahlerOrders;
};
