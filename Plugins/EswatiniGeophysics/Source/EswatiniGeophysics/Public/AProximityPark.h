// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AProximityPark.generated.h"

/**
 * Fog-net deployment site anchored to real GPS coordinates within Eswatini.
 * Used by the UAV integration layer to spawn interactive sites in the world.
 */
UCLASS(BlueprintType)
class ESWATINIGEOPHYSICS_API AProximityPark : public AActor
{
    GENERATED_BODY()

public:
    AProximityPark();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    float LatitudeDeg;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    float LongitudeDeg;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    FString SiteName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Geophysics")
    bool bFogNetDeployed;

    /** Returns "lat=<LatitudeDeg> lon=<LongitudeDeg>" for logging / UI. */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Geophysics")
    FString GetGPSString() const;

protected:
    virtual void BeginPlay() override;

private:
    static constexpr float LAT_MIN = -27.32f;
    static constexpr float LAT_MAX = -25.72f;
    static constexpr float LON_MIN = 30.79f;
    static constexpr float LON_MAX = 32.14f;
};
