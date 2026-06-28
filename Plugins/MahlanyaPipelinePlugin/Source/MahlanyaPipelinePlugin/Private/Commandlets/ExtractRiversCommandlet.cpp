// Copyright Charles Bartaria. All Rights Reserved.

#include "Commandlets/ExtractRiversCommandlet.h"
#include "MahlanyaPipelineTypes.h"
#include "ZigComputeBridge.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UExtractRiversCommandlet::UExtractRiversCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

// ---------------------------------------------------------------------------
// ResolveLibPath
// ---------------------------------------------------------------------------

FString UExtractRiversCommandlet::ResolveLibPath()
{
    FString EnvPath = FPlatformMisc::GetEnvironmentVariable(TEXT("MAHLANYA_ZIG_LIB_PATH"));
    if (!EnvPath.IsEmpty())
    {
        return EnvPath;
    }
    return FPaths::Combine(FPaths::ProjectPluginsDir(),
                           TEXT("MahlanyaPipelinePlugin"),
                           TEXT("zig-out"),
                           TEXT("lib"),
                           TEXT("libmahlanya_compute.so"));
}

// ---------------------------------------------------------------------------
// LoadRawFloat32
// ---------------------------------------------------------------------------

bool UExtractRiversCommandlet::LoadRawFloat32(const FString& FilePath,
                                               TArray<float>& OutData,
                                               int32& OutWidth,
                                               int32& OutHeight)
{
    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *FilePath))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("LoadRawFloat32: could not read '%s'"), *FilePath);
        return false;
    }

    const int32 FloatCount = RawBytes.Num() / sizeof(float);
    if (FloatCount == 0 || (RawBytes.Num() % sizeof(float)) != 0)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("LoadRawFloat32: file size %d is not a multiple of 4 bytes."),
               RawBytes.Num());
        return false;
    }

    const int32 Side = FMath::RoundToInt(FMath::Sqrt(static_cast<float>(FloatCount)));
    if (Side * Side != FloatCount)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("LoadRawFloat32: %d floats is not a perfect square."), FloatCount);
        return false;
    }

    OutWidth  = Side;
    OutHeight = Side;
    OutData.SetNumUninitialized(FloatCount);
    FMemory::Memcpy(OutData.GetData(), RawBytes.GetData(), RawBytes.Num());
    return true;
}

// ---------------------------------------------------------------------------
// TraceRiverNetwork
//
// Very simplified river extraction:
//   - Build a boolean stream mask from accumulation >= Threshold.
//   - Walk each stream cell along the direction of greatest accumulation
//     to form polylines (one polyline per source cell that has no
//     upstream stream-cell neighbours).
//   - Each UTM coordinate is derived from the cell's (col, row) index
//     scaled to [0, Width) x [0, Height) in world-space units.
//   - Strahler order assignment is deferred to AssignStrahlerOrders().
// ---------------------------------------------------------------------------

FRiverNetwork UExtractRiversCommandlet::TraceRiverNetwork(const TArray<float>& Accum,
                                                           int32 Width,
                                                           int32 Height,
                                                           float Threshold)
{
    FRiverNetwork Network;

    // Build stream mask
    TArray<bool> IsStream;
    IsStream.SetNumZeroed(Width * Height);
    for (int32 i = 0; i < Accum.Num(); ++i)
    {
        IsStream[i] = (Accum[i] >= Threshold);
    }

    // 8-connectivity offsets: N, NE, E, SE, S, SW, W, NW
    const int32 DRow[8] = { -1, -1,  0,  1,  1,  1,  0, -1 };
    const int32 DCol[8] = {  0,  1,  1,  1,  0, -1, -1, -1 };

    TArray<bool> Visited;
    Visited.SetNumZeroed(Width * Height);

    // Find source cells (stream cells with no upstream stream neighbour)
    // and trace downslope to form polylines.
    for (int32 Row = 0; Row < Height; ++Row)
    {
        for (int32 Col = 0; Col < Width; ++Col)
        {
            const int32 Idx = Row * Width + Col;
            if (!IsStream[Idx] || Visited[Idx])
            {
                continue;
            }

            // Check whether any upstream (lower-accumulation) stream neighbour exists.
            bool bHasUpstreamStreamNeighbour = false;
            for (int32 d = 0; d < 8; ++d)
            {
                const int32 NR = Row + DRow[d];
                const int32 NC = Col + DCol[d];
                if (NR < 0 || NR >= Height || NC < 0 || NC >= Width) continue;
                const int32 NIdx = NR * Width + NC;
                if (IsStream[NIdx] && Accum[NIdx] < Accum[Idx])
                {
                    bHasUpstreamStreamNeighbour = true;
                    break;
                }
            }

            if (bHasUpstreamStreamNeighbour)
            {
                // Not a source — will be visited as part of a downstream trace
                continue;
            }

            // Trace downstream from this source cell
            TArray<FVector2D> PolylineVerts;
            int32 CurRow = Row;
            int32 CurCol = Col;

            while (true)
            {
                const int32 CurIdx = CurRow * Width + CurCol;
                if (Visited[CurIdx]) break;

                Visited[CurIdx] = true;
                PolylineVerts.Add(FVector2D(static_cast<float>(CurCol),
                                             static_cast<float>(CurRow)));

                // Move to the unvisited downstream stream neighbour with max accumulation
                int32 BestRow = -1, BestCol = -1;
                float BestAccum = -1.f;
                for (int32 d = 0; d < 8; ++d)
                {
                    const int32 NR = CurRow + DRow[d];
                    const int32 NC = CurCol + DCol[d];
                    if (NR < 0 || NR >= Height || NC < 0 || NC >= Width) continue;
                    const int32 NIdx = NR * Width + NC;
                    if (!IsStream[NIdx] || Visited[NIdx]) continue;
                    if (Accum[NIdx] > BestAccum)
                    {
                        BestAccum = Accum[NIdx];
                        BestRow   = NR;
                        BestCol   = NC;
                    }
                }

                if (BestRow < 0) break; // Reached the outlet
                CurRow = BestRow;
                CurCol = BestCol;
            }

            if (PolylineVerts.Num() < 2)
            {
                continue; // Skip degenerate single-cell segments
            }

            // Add polyline to network (name and Strahler order filled later)
            const int32 PolylineIdx = Network.VertexCounts.Num();
            Network.VertexCounts.Add(PolylineVerts.Num());
            Network.RiverNames.Add(FString::Printf(TEXT("River_%d"), PolylineIdx));
            Network.StrahlerOrders.Add(1); // Placeholder
            Network.Vertices.Append(PolylineVerts);
        }
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("TraceRiverNetwork: extracted %d river polylines (%d total vertices)."),
           Network.VertexCounts.Num(), Network.Vertices.Num());

    return Network;
}

// ---------------------------------------------------------------------------
// AssignStrahlerOrders
//
// Simple heuristic: order = 1 + log2(polyline length / 2), clamped to [1,8].
// A proper implementation would require a full tributary graph, which is
// outside the scope of this commandlet.
// ---------------------------------------------------------------------------

void UExtractRiversCommandlet::AssignStrahlerOrders(FRiverNetwork& Network)
{
    for (int32 i = 0; i < Network.VertexCounts.Num(); ++i)
    {
        const float LogLen = FMath::Log2(static_cast<float>(FMath::Max(2, Network.VertexCounts[i])) / 2.f);
        Network.StrahlerOrders[i] = FMath::Clamp(1 + FMath::FloorToInt(LogLen), 1, 8);
    }
}

// ---------------------------------------------------------------------------
// WriteRiverCSV
// ---------------------------------------------------------------------------

bool UExtractRiversCommandlet::WriteRiverCSV(const FRiverNetwork& Network,
                                              const FString& FilePath)
{
    FString CSV = TEXT("river_name,x,y,strahler_order\n");

    int32 VertexOffset = 0;
    for (int32 PolyIdx = 0; PolyIdx < Network.VertexCounts.Num(); ++PolyIdx)
    {
        const FString&  Name    = Network.RiverNames[PolyIdx];
        const int32     Order   = Network.StrahlerOrders[PolyIdx];
        const int32     VCount  = Network.VertexCounts[PolyIdx];

        for (int32 v = 0; v < VCount; ++v)
        {
            const FVector2D& Vert = Network.Vertices[VertexOffset + v];
            CSV += FString::Printf(TEXT("%s,%.6f,%.6f,%d\n"),
                                   *Name, Vert.X, Vert.Y, Order);
        }
        VertexOffset += VCount;
    }

    if (!FFileHelper::SaveStringToFile(CSV, *FilePath))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("WriteRiverCSV: failed to write '%s'"), *FilePath);
        return false;
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("WriteRiverCSV: wrote %d polylines to '%s'"),
           Network.VertexCounts.Num(), *FilePath);
    return true;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int32 UExtractRiversCommandlet::Main(const FString& Params)
{
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("=== ExtractRivers commandlet started ==="));

    // ------------------------------------------------------------------
    // Parse parameters
    // ------------------------------------------------------------------
    FString DEMPath, OutputDir;
    float Threshold = 1000.f;

    FParse::Value(*Params, TEXT("dem="),       DEMPath);
    FParse::Value(*Params, TEXT("output="),    OutputDir);
    FParse::Value(*Params, TEXT("threshold="), Threshold);

    if (DEMPath.IsEmpty())
    {
        UE_LOG(LogMahlanyaPipeline, Error, TEXT("ExtractRivers: -dem=<path> is required."));
        return 1;
    }
    if (OutputDir.IsEmpty())
    {
        UE_LOG(LogMahlanyaPipeline, Error, TEXT("ExtractRivers: -output=<directory> is required."));
        return 1;
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("ExtractRivers: dem='%s' output='%s' threshold=%.1f"),
           *DEMPath, *OutputDir, Threshold);

    // ------------------------------------------------------------------
    // Load Zig library
    // ------------------------------------------------------------------
    const FString LibPath = ResolveLibPath();
    if (!FZigComputeBridge::Load(LibPath))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ExtractRivers: failed to load Zig compute library from '%s'."), *LibPath);
        return 1;
    }

    // ------------------------------------------------------------------
    // Load DEM
    // ------------------------------------------------------------------
    TArray<float> HeightData;
    int32 Width = 0, Height = 0;
    if (!LoadRawFloat32(DEMPath, HeightData, Width, Height))
    {
        FZigComputeBridge::Unload();
        return 1;
    }

    // ------------------------------------------------------------------
    // D-infinity hydrological processing
    // ------------------------------------------------------------------
    const int32 CellCount = Width * Height;

    // 1. Fill pits (in-place)
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("ExtractRivers: filling pits..."));
    FZigComputeBridge::DInfFillPits(HeightData.GetData(), Width, Height);

    // 2. Flow directions
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("ExtractRivers: computing D-inf flow directions..."));
    TArray<float> FlowDir;
    FlowDir.SetNumUninitialized(CellCount);
    FZigComputeBridge::DInfFlowDirection(HeightData.GetData(), FlowDir.GetData(), Width, Height);

    // 3. Flow accumulation
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("ExtractRivers: computing flow accumulation..."));
    TArray<float> Accum;
    Accum.SetNumUninitialized(CellCount);
    FZigComputeBridge::DInfFlowAccumulation(FlowDir.GetData(), Accum.GetData(), Width, Height);

    // ------------------------------------------------------------------
    // Trace river network
    // ------------------------------------------------------------------
    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("ExtractRivers: tracing river network (threshold=%.1f)..."), Threshold);

    FRiverNetwork Network = TraceRiverNetwork(Accum, Width, Height, Threshold);
    AssignStrahlerOrders(Network);

    // ------------------------------------------------------------------
    // Write output CSV
    // ------------------------------------------------------------------
    if (!IFileManager::Get().MakeDirectory(*OutputDir, /*bTree=*/true))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ExtractRivers: could not create output directory '%s'."), *OutputDir);
        FZigComputeBridge::Unload();
        return 1;
    }

    const FString CSVPath = FPaths::Combine(OutputDir, TEXT("rivers.csv"));
    if (!WriteRiverCSV(Network, CSVPath))
    {
        FZigComputeBridge::Unload();
        return 1;
    }

    FZigComputeBridge::Unload();
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("=== ExtractRivers commandlet finished successfully ==="));
    return 0;
}
