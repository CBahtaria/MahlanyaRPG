// Copyright Charles Bartaria. All Rights Reserved.

#include "Commandlets/ExportToUE5Commandlet.h"
#include "UE5LandscapeExporter.h"
#include "MahlanyaPipelineTypes.h"
#include "Algo/MinElement.h"
#include "Algo/MaxElement.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogExportToUE5, Log, All);

UExportToUE5Commandlet::UExportToUE5Commandlet()
{
    IsClient     = false;
    IsEditor     = true;    // This is an Editor-only commandlet
    IsServer     = false;
    LogToConsole = true;
}

bool UExportToUE5Commandlet::LoadRawFloat32(const FString& FilePath,
                                             TArray<float>& OutData,
                                             int32& OutWidth,
                                             int32& OutHeight)
{
    TArray<uint8> Raw;
    if (!FFileHelper::LoadFileToArray(Raw, *FilePath))
    {
        UE_LOG(LogExportToUE5, Error, TEXT("Cannot read: %s"), *FilePath);
        return false;
    }
    const int32 N = Raw.Num() / sizeof(float);
    const int32 Side = FMath::RoundToInt(FMath::Sqrt((float)N));
    if (Side * Side != N)
    {
        UE_LOG(LogExportToUE5, Error, TEXT("File not square float32 grid: %d floats"), N);
        return false;
    }
    OutWidth = OutHeight = Side;
    OutData.SetNumUninitialized(N);
    FMemory::Memcpy(OutData.GetData(), Raw.GetData(), Raw.Num());
    return true;
}

bool UExportToUE5Commandlet::LoadR16AsFloat(const FString& FilePath,
                                             TArray<float>& OutData,
                                             int32& OutWidth,
                                             int32& OutHeight)
{
    TArray<uint8> Raw;
    if (!FFileHelper::LoadFileToArray(Raw, *FilePath))
    {
        UE_LOG(LogExportToUE5, Error, TEXT("Cannot read r16: %s"), *FilePath);
        return false;
    }
    const int32 N = Raw.Num() / sizeof(uint16);
    const int32 Side = FMath::RoundToInt(FMath::Sqrt((float)N));
    if (Side * Side != N)
    {
        UE_LOG(LogExportToUE5, Error, TEXT("r16 not square: %d uint16 values"), N);
        return false;
    }
    OutWidth = OutHeight = Side;
    OutData.SetNumUninitialized(N);
    const uint16* Src = reinterpret_cast<const uint16*>(Raw.GetData());
    for (int32 i = 0; i < N; ++i)
        OutData[i] = (float)Src[i] / 65535.f;
    return true;
}

int32 UExportToUE5Commandlet::Main(const FString& Params)
{
    TArray<FString> Tokens, Switches;
    TMap<FString, FString> ParamMap;
    ParseCommandLine(*Params, Tokens, Switches, ParamMap);

    const FString DEMPath   = ParamMap.FindRef(TEXT("dem"));
    const FString OutputDir = ParamMap.FindRef(TEXT("output"));
    const int32 TileSize    = ParamMap.Contains(TEXT("tile_size"))
                              ? FCString::Atoi(*ParamMap[TEXT("tile_size")])
                              : 1009;

    if (DEMPath.IsEmpty() || OutputDir.IsEmpty())
    {
        UE_LOG(LogExportToUE5, Error,
               TEXT("Usage: -run=ExportToUE5 -dem=<path> -output=<dir> [-tile_size=1009]"));
        return 1;
    }

    TArray<float> HeightData;
    int32 W = 0, H = 0;
    const FString Ext = FPaths::GetExtension(DEMPath).ToLower();
    bool bLoaded = (Ext == TEXT("r16"))
                   ? LoadR16AsFloat(DEMPath, HeightData, W, H)
                   : LoadRawFloat32(DEMPath, HeightData, W, H);
    if (!bLoaded)
        return 1;

    UE_LOG(LogExportToUE5, Log, TEXT("DEM loaded: %d×%d"), W, H);

    FDEMGrid Grid;
    Grid.Data   = MoveTemp(HeightData);
    Grid.Width  = W;
    Grid.Height = H;
    Grid.MinElevation = *Algo::MinElement(Grid.Data);
    Grid.MaxElevation = *Algo::MaxElement(Grid.Data);

    UE_LOG(LogExportToUE5, Log, TEXT("Elevation range: %.1f – %.1f m"),
           Grid.MinElevation, Grid.MaxElevation);

    if (!FUE5LandscapeExporter::ExportHeightmapTiles(Grid, OutputDir, TileSize))
    {
        UE_LOG(LogExportToUE5, Error, TEXT("Tile export failed"));
        return 1;
    }

    UE_LOG(LogExportToUE5, Log, TEXT("UE5 export complete → %s"), *OutputDir);
    return 0;
}
