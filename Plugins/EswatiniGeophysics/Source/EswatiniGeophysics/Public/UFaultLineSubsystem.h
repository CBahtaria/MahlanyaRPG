// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UFaultLineSubsystem.generated.h"

/**
 * World subsystem that loads Eswatini fault-line GeoJSON polylines and
 * broadcasts terrain deformation events along them via SimulationBus.
 */
UCLASS(BlueprintType)
class ESWATINIGEOPHYSICS_API UFaultLineSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Absolute or project-relative path to eswatini_faults.geojson. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    FString FaultDataPath;

    /** Parses the GeoJSON file into FaultPolylines. */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Geophysics")
    void LoadFaultLines();

    /** Broadcasts a fault-deform event per polyline through SimulationBus. */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Geophysics")
    void DeformTerrainAlongFaults(float MaxDisplacementCm);

    /** Read-only accessor for parsed fault polylines (lon/lat pairs). */
    TArray<TArray<FVector2D>> GetFaultPolylines() const;

private:
    TArray<TArray<FVector2D>> FaultPolylines;
    bool bFaultsLoaded = false;

    static constexpr float DEFAULT_MAX_DISPLACEMENT_CM = 500.f;
    static constexpr int32 MIN_FAULT_POINT_COUNT = 2;
};
