#pragma once
#include "CoreMinimal.h"
#include "FTerrainFrictionTensor.generated.h"

/**
 * Anisotropic friction tensor for a terrain physical material.
 *
 * AnisotropyAxis:  bearing (degrees) of MAX-grip direction.
 *   e.g. shale 235° = perpendicular to bedding planes = highest grip.
 *   Moving 90° away from that axis = lowest grip = StaticWet / AnisotropyRatio.
 *
 * AnisotropyRatio: max_grip / min_grip.
 *   1.0 = fully isotropic (granite, clay, grass).
 *   3.2 = shale (3× more grip perpendicular to bedding than along it).
 */
USTRUCT(BlueprintType)
struct LOCOMOTIONPHYSICSPLUGIN_API FTerrainFrictionTensor
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaticDry    = 0.60f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float StaticWet    = 0.40f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float DynamicDry   = 0.50f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float DynamicWet   = 0.30f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float AnisotropyAxis  = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float AnisotropyRatio = 1.0f;
};

/** Global registry keyed by UPhysicalMaterial name (FName). */
struct LOCOMOTIONPHYSICSPLUGIN_API FTerrainFrictionRegistry
{
    static const TMap<FName, FTerrainFrictionTensor>& Get();
    static FTerrainFrictionTensor Lookup(FName MaterialName);
};
