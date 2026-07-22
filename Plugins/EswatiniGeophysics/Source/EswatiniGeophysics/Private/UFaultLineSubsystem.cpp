// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "UFaultLineSubsystem.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UFaultLineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FaultPolylines.Reset();
    bFaultsLoaded = false;
}

void UFaultLineSubsystem::Deinitialize()
{
    FaultPolylines.Reset();
    bFaultsLoaded = false;
    Super::Deinitialize();
}

void UFaultLineSubsystem::LoadFaultLines()
{
    FaultPolylines.Reset();
    bFaultsLoaded = false;

    if (FaultDataPath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("UFaultLineSubsystem: FaultDataPath is empty."));
        return;
    }

    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FaultDataPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("UFaultLineSubsystem: failed to read %s"), *FaultDataPath);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("UFaultLineSubsystem: invalid JSON in %s"), *FaultDataPath);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
    if (!Root->TryGetArrayField(TEXT("features"), Features) || Features == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("UFaultLineSubsystem: no 'features' array."));
        return;
    }

    for (const TSharedPtr<FJsonValue>& FeatureValue : *Features)
    {
        const TSharedPtr<FJsonObject> Feature = FeatureValue->AsObject();
        if (!Feature.IsValid())
        {
            continue;
        }
        const TSharedPtr<FJsonObject> Geometry = Feature->GetObjectField(TEXT("geometry"));
        if (!Geometry.IsValid())
        {
            continue;
        }
        const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
        if (!Geometry->TryGetArrayField(TEXT("coordinates"), Coords) || Coords == nullptr)
        {
            continue;
        }

        TArray<FVector2D> Polyline;
        Polyline.Reserve(Coords->Num());
        for (const TSharedPtr<FJsonValue>& Pair : *Coords)
        {
            const TArray<TSharedPtr<FJsonValue>>* LonLat = nullptr;
            if (!Pair->TryGetArray(LonLat) || LonLat == nullptr || LonLat->Num() < 2)
            {
                continue;
            }
            const float Lon = static_cast<float>((*LonLat)[0]->AsNumber());
            const float Lat = static_cast<float>((*LonLat)[1]->AsNumber());
            Polyline.Emplace(Lon, Lat);
        }

        if (Polyline.Num() >= MIN_FAULT_POINT_COUNT)
        {
            FaultPolylines.Add(MoveTemp(Polyline));
        }
    }

    bFaultsLoaded = true;
    UE_LOG(LogTemp, Log, TEXT("UFaultLineSubsystem: loaded %d fault polylines."), FaultPolylines.Num());
}

void UFaultLineSubsystem::DeformTerrainAlongFaults(float MaxDisplacementCm)
{
    if (!bFaultsLoaded)
    {
        UE_LOG(LogTemp, Warning, TEXT("UFaultLineSubsystem: faults not loaded; call LoadFaultLines first."));
        return;
    }

    const float DisplacementCm = MaxDisplacementCm > 0.f ? MaxDisplacementCm : DEFAULT_MAX_DISPLACEMENT_CM;

    // SimulationBus broadcast stub: iterate polylines and emit a FaultDeformEvent per line.
    // Concrete bus wiring is done through the SimulationBusPlugin dispatcher in a follow-up patch.
    for (int32 Index = 0; Index < FaultPolylines.Num(); ++Index)
    {
        const TArray<FVector2D>& Polyline = FaultPolylines[Index];
        UE_LOG(LogTemp, Verbose,
            TEXT("FaultDeformEvent: polyline=%d points=%d displacement_cm=%.2f"),
            Index, Polyline.Num(), DisplacementCm);
    }
}

TArray<TArray<FVector2D>> UFaultLineSubsystem::GetFaultPolylines() const
{
    return FaultPolylines;
}
