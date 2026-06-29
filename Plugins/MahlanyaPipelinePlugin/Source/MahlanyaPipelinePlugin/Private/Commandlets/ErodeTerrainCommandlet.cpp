// Copyright Charles Bartaria. All Rights Reserved.

#include "Commandlets/ErodeTerrainCommandlet.h"
#include "MahlanyaPipelineTypes.h"
#include "ZigComputeBridge.h"
#include "UE5LandscapeExporter.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UErodeTerrainCommandlet::UErodeTerrainCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

// ---------------------------------------------------------------------------
// ResolveLibPath
// ---------------------------------------------------------------------------

FString UErodeTerrainCommandlet::ResolveLibPath()
{
    // 1. Check environment variable override
    FString EnvPath = FPlatformMisc::GetEnvironmentVariable(TEXT("MAHLANYA_ZIG_LIB_PATH"));
    if (!EnvPath.IsEmpty())
    {
        UE_LOG(LogMahlanyaPipeline, Log,
               TEXT("ErodeTerrain: using MAHLANYA_ZIG_LIB_PATH env override: '%s'"), *EnvPath);
        return EnvPath;
    }

    // 2. Fall back to path next to the plugin root (built by `zig build`)
    //    PluginsDir/<plugin>/zig-out/lib/libmahlanya_compute.so
    const FString PluginsDir = FPaths::ProjectPluginsDir();
    return FPaths::Combine(PluginsDir,
                           TEXT("MahlanyaPipelinePlugin"),
                           TEXT("zig-out"),
                           TEXT("lib"),
                           TEXT("libmahlanya_compute.so"));
}

// ---------------------------------------------------------------------------
// LoadRawFloat32
// ---------------------------------------------------------------------------

bool UErodeTerrainCommandlet::LoadRawFloat32(const FString& FilePath,
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

    // Infer a square grid from the float count
    const int32 Side = FMath::RoundToInt(FMath::Sqrt(static_cast<float>(FloatCount)));
    if (Side * Side != FloatCount)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("LoadRawFloat32: %d floats is not a perfect square. "
                    "Non-square grids require explicit -width and -height args (not yet supported)."),
               FloatCount);
        return false;
    }

    OutWidth  = Side;
    OutHeight = Side;
    OutData.SetNumUninitialized(FloatCount);
    FMemory::Memcpy(OutData.GetData(), RawBytes.GetData(), RawBytes.Num());

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("LoadRawFloat32: loaded %dx%d grid from '%s'"), OutWidth, OutHeight, *FilePath);
    return true;
}

// ---------------------------------------------------------------------------
// LoadR16AsFloat
// ---------------------------------------------------------------------------

bool UErodeTerrainCommandlet::LoadR16AsFloat(const FString& FilePath,
                                              TArray<float>& OutData,
                                              int32& OutWidth,
                                              int32& OutHeight)
{
    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *FilePath))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("LoadR16AsFloat: could not read '%s'"), *FilePath);
        return false;
    }

    const int32 U16Count = RawBytes.Num() / sizeof(uint16);
    if (U16Count == 0 || (RawBytes.Num() % sizeof(uint16)) != 0)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("LoadR16AsFloat: file size %d is not a multiple of 2 bytes."),
               RawBytes.Num());
        return false;
    }

    // Infer a square grid
    const int32 Side = FMath::RoundToInt(FMath::Sqrt(static_cast<float>(U16Count)));
    if (Side * Side != U16Count)
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("LoadR16AsFloat: %d uint16s is not a perfect square. "
                    "Provide explicit -width and -height if the grid is non-square."),
               U16Count);
        return false;
    }

    OutWidth  = Side;
    OutHeight = Side;
    OutData.SetNumUninitialized(U16Count);

    const uint16* Src = reinterpret_cast<const uint16*>(RawBytes.GetData());
    for (int32 i = 0; i < U16Count; ++i)
    {
        OutData[i] = static_cast<float>(Src[i]) / 65535.f;
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("LoadR16AsFloat: loaded %dx%d grid from '%s'"), OutWidth, OutHeight, *FilePath);
    return true;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int32 UErodeTerrainCommandlet::Main(const FString& Params)
{
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("=== ErodeTerrain commandlet started ==="));
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("Params: %s"), *Params);

    // ------------------------------------------------------------------
    // Parse command-line parameters
    // ------------------------------------------------------------------
    FString DEMPath, HardnessPath, OutputDir;
    int32 Iterations = 1000;

    FParse::Value(*Params, TEXT("dem="),       DEMPath);
    FParse::Value(*Params, TEXT("hardness="),  HardnessPath);
    FParse::Value(*Params, TEXT("output="),    OutputDir);
    FParse::Value(*Params, TEXT("iterations="), Iterations);

    if (DEMPath.IsEmpty())
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ErodeTerrain: -dem=<path> is required."));
        return 1;
    }
    if (HardnessPath.IsEmpty())
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ErodeTerrain: -hardness=<path> is required."));
        return 1;
    }
    if (OutputDir.IsEmpty())
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ErodeTerrain: -output=<directory> is required."));
        return 1;
    }

    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("ErodeTerrain: dem='%s' hardness='%s' output='%s' iterations=%d"),
           *DEMPath, *HardnessPath, *OutputDir, Iterations);

    // ------------------------------------------------------------------
    // Load Zig compute library
    // ------------------------------------------------------------------
    const FString LibPath = ResolveLibPath();
    if (!FZigComputeBridge::Load(LibPath))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ErodeTerrain: failed to load Zig compute library from '%s'."), *LibPath);
        return 1;
    }

    // ------------------------------------------------------------------
    // Load DEM
    // ------------------------------------------------------------------
    FDEMGrid DEM;
    {
        const FString Ext = FPaths::GetExtension(DEMPath).ToLower();
        bool bLoaded = false;
        if (Ext == TEXT("r16"))
        {
            bLoaded = LoadR16AsFloat(DEMPath, DEM.Data, DEM.Width, DEM.Height);
        }
        else
        {
            bLoaded = LoadRawFloat32(DEMPath, DEM.Data, DEM.Width, DEM.Height);
        }

        if (!bLoaded)
        {
            FZigComputeBridge::Unload();
            return 1;
        }

        // Compute elevation range
        DEM.MinElevation =  TNumericLimits<float>::Max();
        DEM.MaxElevation = -TNumericLimits<float>::Max();
        for (float V : DEM.Data)
        {
            DEM.MinElevation = FMath::Min(DEM.MinElevation, V);
            DEM.MaxElevation = FMath::Max(DEM.MaxElevation, V);
        }
        UE_LOG(LogMahlanyaPipeline, Log,
               TEXT("ErodeTerrain: DEM loaded. Elevation range [%.2f, %.2f]"),
               DEM.MinElevation, DEM.MaxElevation);
    }

    // ------------------------------------------------------------------
    // Load Hardness grid
    // ------------------------------------------------------------------
    FHardnessGrid Hardness;
    {
        bool bLoaded = LoadRawFloat32(HardnessPath, Hardness.Data, Hardness.Width, Hardness.Height);
        if (!bLoaded)
        {
            FZigComputeBridge::Unload();
            return 1;
        }

        if (Hardness.Width != DEM.Width || Hardness.Height != DEM.Height)
        {
            UE_LOG(LogMahlanyaPipeline, Error,
                   TEXT("ErodeTerrain: Hardness grid (%dx%d) does not match DEM (%dx%d)."),
                   Hardness.Width, Hardness.Height, DEM.Width, DEM.Height);
            FZigComputeBridge::Unload();
            return 1;
        }
    }

    // ------------------------------------------------------------------
    // Allocate erosion work buffers
    // ------------------------------------------------------------------
    const int32 CellCount = DEM.Width * DEM.Height;
    TArray<float> Water, Sediment;
    Water.SetNumZeroed(CellCount);
    Sediment.SetNumZeroed(CellCount);

    // Erosion parameters (reasonable defaults — expose as CLI args if needed)
    const float Rain         = 0.01f;
    const float Ks           = 0.3f;   // Solubility / erosion rate
    const float Kd           = 0.01f;  // Deposition rate
    const float TalusAngle   = 0.57f;  // ~33 degrees in radians
    const float Dt           = 0.05f;
    const float WindStrength = 0.2f;
    const float SlopeFactor  = 0.8f;

    // ------------------------------------------------------------------
    // Erosion loop
    // ------------------------------------------------------------------
    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("ErodeTerrain: starting erosion loop (%d iterations)..."), Iterations);

    for (int32 Iter = 0; Iter < Iterations; ++Iter)
    {
        // Fluvial every iteration
        FZigComputeBridge::FluvialErosionPass(
            DEM.RawPtr(), Water.GetData(), Sediment.GetData(),
            Hardness.RawPtr(), DEM.Width, DEM.Height,
            Rain, Ks, Kd);

        // Thermal every 5th iteration
        if (Iter % 5 == 0)
        {
            FZigComputeBridge::ThermalErosionPass(
                DEM.RawPtr(), Hardness.RawPtr(),
                DEM.Width, DEM.Height, TalusAngle, Dt);
        }

        // Aeolian every 20th iteration
        if (Iter % 20 == 0)
        {
            FZigComputeBridge::AeolianErosionPass(
                DEM.RawPtr(), Hardness.RawPtr(),
                DEM.Width, DEM.Height, WindStrength, Dt);
        }

        // Mass wasting every 50th iteration
        if (Iter % 50 == 0)
        {
            const uint64 Seed = static_cast<uint64>(Iter) * 6364136223846793005ULL + 1442695040888963407ULL;
            FZigComputeBridge::MassWastingPass(
                DEM.RawPtr(), Hardness.RawPtr(),
                DEM.Width, DEM.Height, SlopeFactor, Seed);
        }

        // Log progress every 10%
        if (Iterations > 0 && (Iter + 1) % FMath::Max(1, Iterations / 10) == 0)
        {
            UE_LOG(LogMahlanyaPipeline, Log,
                   TEXT("ErodeTerrain: %d/%d iterations complete (%.0f%%)"),
                   Iter + 1, Iterations,
                   100.f * static_cast<float>(Iter + 1) / static_cast<float>(Iterations));
        }
    }

    UE_LOG(LogMahlanyaPipeline, Log, TEXT("ErodeTerrain: erosion complete."));

    // Recompute elevation range after erosion
    DEM.MinElevation =  TNumericLimits<float>::Max();
    DEM.MaxElevation = -TNumericLimits<float>::Max();
    for (float V : DEM.Data)
    {
        DEM.MinElevation = FMath::Min(DEM.MinElevation, V);
        DEM.MaxElevation = FMath::Max(DEM.MaxElevation, V);
    }
    UE_LOG(LogMahlanyaPipeline, Log,
           TEXT("ErodeTerrain: post-erosion elevation range [%.2f, %.2f]"),
           DEM.MinElevation, DEM.MaxElevation);

    // ------------------------------------------------------------------
    // Export heightmap tiles
    // ------------------------------------------------------------------
    if (!FUE5LandscapeExporter::ExportHeightmapTiles(DEM, OutputDir))
    {
        UE_LOG(LogMahlanyaPipeline, Error,
               TEXT("ErodeTerrain: tile export failed."));
        FZigComputeBridge::Unload();
        return 1;
    }

    FZigComputeBridge::Unload();
    UE_LOG(LogMahlanyaPipeline, Log, TEXT("=== ErodeTerrain commandlet finished successfully ==="));
    return 0;
}
