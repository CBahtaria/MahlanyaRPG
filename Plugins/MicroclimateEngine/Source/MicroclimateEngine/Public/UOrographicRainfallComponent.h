#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UOrographicRainfallComponent.generated.h"

/**
 * UOrographicRainfallComponent
 *
 * Computes orographic precipitation probability for the current wind vector
 * against the terrain elevation gradient. Attaches to a Weather Manager actor.
 *
 * When wind exceeds CondensationThreshold_m of uplift over 10km, triggers
 * a rain event in UMicroclimateSubsystem.
 */
UCLASS(ClassGroup=("MicroclimateEngine"), meta=(BlueprintSpawnableComponent))
class MICROCLIMATEENGINE_API UOrographicRainfallComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOrographicRainfallComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                                FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * Compute precipitation probability from wind uplift over terrain.
     *
     * @param WindSpeed_ms       Wind speed [m/s].
     * @param WindBearing_deg    Wind bearing [°, 0=North].
     * @param TerrainDeltaElev_m Elevation gain over 10km upwind [m].
     * @return                   Precipitation probability [0–1].
     */
    UFUNCTION(BlueprintCallable, Category = "Orographic Rainfall")
    float ComputePrecipitationProbability(float WindSpeed_ms,
                                           float WindBearing_deg,
                                           float TerrainDeltaElev_m) const;

    /**
     * Elevation uplift [m] over 10km needed to trigger condensation.
     * Default: 400m (Eswatini Highveld drop over Middleveld escarpment).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orographic Rainfall")
    float CondensationThreshold_m = 400.f;

    /** Probability multiplier at maximum uplift. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orographic Rainfall")
    float MaxProbabilityMultiplier = 3.5f;

private:
    UPROPERTY()
    class UMicroclimateSubsystem* CachedSubsystem = nullptr;
};
