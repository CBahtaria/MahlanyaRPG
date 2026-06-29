// Copyright Charles Bartaria. All Rights Reserved.

#include "Commandlets/ComputeVoronoiCommandlet.h"
#include "MahlanyaPipelineTypes.h"
#include "ZigComputeBridge.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UComputeVoronoiCommandlet::UComputeVoronoiCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

// ---------------------------------------------------------------------------
// ResolveLibPath
// ---------------------------------------------------------------------------

FString UComputeVoronoiCommandlet::ResolveLibPath()
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
// GenerateRandomPoints
//
// A minimal linear-congruential PRNG (Knuth MMIX) is used so we avoid
// pulling in <random> or std::. The taboo zone is re-rolled per-point.
// ---------------------------------------------------------------------------

void UComputeVoronoiCommandlet::GenerateRandomPoints(TArray<double>& OutPoints,
                                                      int32 NPoints,
                                                      double TabooStart,
                                                      double TabooEnd,
                                                      int32 Seed)
{
    OutPoints.SetNumUninitialized(NPoints * 2);

    // LCG constants (Knuth MMIX)
    const uint64 LCG_A = 6364136223846793005ULL;
    const uint64 LCG_C = 1442695040888963407ULL;
    uint64 State       = static_cast<uint64>(Seed);

    const bool bHasTaboo = (TabooEnd > TabooStart);
    const double TabooWidth = TabooEnd - TabooStart;

    auto NextDouble = [&]() -> double
    {
        State = State * LCG_A + LCG_C;
        // Map high 32 bits to [0,1)
        return static_cast<double>(static_cast<uint32>(State >> 32)) / 4294967296.0;
    };

    for (int32 i = 0; i < NPoints; ++i)
    {
        double X, Y;
        int32 MaxRetries = 1000;
        do
        {
            X = NextDouble();
            Y = NextDouble();
            --MaxRetries;
        }
        while (bHasTaboo
               && X >= TabooStart
               && X <= TabooEnd
               && MaxRetries > 0);

        OutPoints[i * 2 + 0] = X;
        OutPoints[i * 2 + 1] = Y;
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("GenerateRandomPoints: generated %d points (taboo x=[%.3f,%.3f])."),
           NPoints, TabooStart, TabooEnd);
    (void)TabooWidth;
}

// ---------------------------------------------------------------------------
// BuildJSON
//
// Produces compact but readable JSON:
//   {"points":[[x,y],...],"areas":[...]}
// We build this manually to avoid a JSON library dependency.
// ---------------------------------------------------------------------------

FString UComputeVoronoiCommandlet::BuildJSON(const TArray<double>& Points,
                                              const TArray<double>& Areas,
                                              int32 NPoints)
{
    FString Out;
    Out.Reserve(NPoints * 40);  // rough estimate

    Out += TEXT("{\"points\":[");
    for (int32 i = 0; i < NPoints; ++i)
    {
        if (i > 0) Out += TEXT(",");
        Out += FString::Printf(TEXT("[%.8f,%.8f]"),
                               Points[i * 2 + 0],
                               Points[i * 2 + 1]);
    }

    Out += TEXT("],\"areas\":[");
    for (int32 i = 0; i < NPoints; ++i)
    {
        if (i > 0) Out += TEXT(",");
        Out += FString::Printf(TEXT("%.10f"), Areas[i]);
    }

    Out += TEXT("]}");
    return Out;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int32 UComputeVoronoiCommandlet::Main(const FString& Params)
{
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("=== ComputeVoronoi commandlet started ==="));
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("Params: %s"), *Params);

    // ------------------------------------------------------------------
    // Parse parameters
    // ------------------------------------------------------------------
    int32 NPoints      = 512;
    int32 Iters        = 50;
    FString OutputPath;
    float TabooStart   = 0.f;
    float TabooEnd     = 0.f;

    FParse::Value(*Params, TEXT("n_points="),    NPoints);
    FParse::Value(*Params, TEXT("iters="),       Iters);
    FParse::Value(*Params, TEXT("output="),      OutputPath);
    FParse::Value(*Params, TEXT("taboo_start="), TabooStart);
    FParse::Value(*Params, TEXT("taboo_end="),   TabooEnd);

    if (OutputPath.IsEmpty())
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ComputeVoronoi: -output=<json_path> is required."));
        return 1;
    }
    if (NPoints < 1)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ComputeVoronoi: -n_points must be >= 1."));
        return 1;
    }
    if (Iters < 0)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ComputeVoronoi: -iters must be >= 0."));
        return 1;
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("ComputeVoronoi: n_points=%d iters=%d output='%s' taboo=[%.3f,%.3f]"),
           NPoints, Iters, *OutputPath,
           static_cast<double>(TabooStart), static_cast<double>(TabooEnd));

    // ------------------------------------------------------------------
    // Load Zig library
    // ------------------------------------------------------------------
    const FString LibPath = ResolveLibPath();
    if (!FZigComputeBridge::Load(LibPath))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ComputeVoronoi: failed to load Zig compute library from '%s'."), *LibPath);
        return 1;
    }

    // ------------------------------------------------------------------
    // Generate initial random points
    // ------------------------------------------------------------------
    TArray<double> Points;
    GenerateRandomPoints(Points, NPoints,
                         static_cast<double>(TabooStart),
                         static_cast<double>(TabooEnd),
                         /*Seed=*/12345);

    // ------------------------------------------------------------------
    // Lloyd relaxation (domain [0,1] x [0,1])
    // ------------------------------------------------------------------
    if (Iters > 0)
    {
        UE_LOG(LogMahlanyaPipeline, Log,
               TEXT("ComputeVoronoi: running Lloyd relaxation (%d iterations)..."), Iters);
        FZigComputeBridge::LloydRelax(Points.GetData(), NPoints, Iters, 1.0, 1.0);
        UE_LOG(LogMahlanyaPipeline, Log, TEXT("ComputeVoronoi: Lloyd relaxation complete."));
    }

    // ------------------------------------------------------------------
    // Compute Voronoi cell areas
    // ------------------------------------------------------------------
    TArray<double> Areas;
    Areas.SetNumZeroed(NPoints);

    UE_LOG(LogMahlanyaPipeline, Log, TEXT("ComputeVoronoi: computing Voronoi cell areas..."));
    FZigComputeBridge::VoronoiCellAreas(Points.GetData(), NPoints, Areas.GetData());

    // ------------------------------------------------------------------
    // Write JSON output
    // ------------------------------------------------------------------
    const FString OutputDir = FPaths::GetPath(OutputPath);
    if (!OutputDir.IsEmpty() && !IFileManager::Get().DirectoryExists(*OutputDir))
    {
        if (!IFileManager::Get().MakeDirectory(*OutputDir, /*bTree=*/true))
        {
            UE_LOG(LogMahlanyaPipeline, Error,
                   TEXT("ComputeVoronoi: could not create output directory '%s'."), *OutputDir);
            FZigComputeBridge::Unload();
            return 1;
        }
    }

    const FString JSON = BuildJSON(Points, Areas, NPoints);
    if (!FFileHelper::SaveStringToFile(JSON, *OutputPath))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ComputeVoronoi: failed to write JSON to '%s'."), *OutputPath);
        FZigComputeBridge::Unload();
        return 1;
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("ComputeVoronoi: wrote %d points + areas to '%s'."), NPoints, *OutputPath);

    FZigComputeBridge::Unload();
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("=== ComputeVoronoi commandlet finished successfully ==="));
    return 0;
}
