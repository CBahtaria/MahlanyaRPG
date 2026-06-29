#pragma once

#include "CoreMinimal.h"
#include "FMicroclimateState.generated.h"

/**
 * FMicroclimateState
 *
 * Complete snapshot of the atmospheric state at a given simulation tick.
 * Published by UMicroclimateSubsystem and consumed by:
 *   - ErosionRuntimePlugin (PrecipitationIntensity)
 *   - LocomotionPhysicsPlugin (wet friction when PrecipitationIntensity > 0)
 *   - UStealthManagerComponent (fog, smoke, rain visibility reduction)
 */
USTRUCT(BlueprintType)
struct MICROCLIMATEENGINE_API FMicroclimateState
{
    GENERATED_BODY()

    /** Barometric pressure at surface level [hPa]. Typical range: 870–1020. */
    UPROPERTY(BlueprintReadOnly, Category = "Microclimate")
    float PressureHPa = 1013.25f;

    /**
     * Pressure tendency over the last 3 simulated hours [hPa/3h].
     * < -3.0 = rapidly falling (storm approaching).
     * > +3.0 = rapidly rising (clearing).
     */
    UPROPERTY(BlueprintReadOnly, Category = "Microclimate")
    float PressureTendencyHPa = 0.f;

    /** Volumetric cloud density fraction [0–1]. Drives UE5 Volumetric Cloud coverage. */
    UPROPERTY(BlueprintReadOnly, Category = "Microclimate")
    float CloudDensityFraction = 0.2f;

    /**
     * Fog base altitude [m ASL].
     * Valley inversion: cold air drains down channels, fog base rises from floor.
     * 0 = ground fog, 2000 = clear.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Microclimate")
    float FogBaseAltitude_m = 2000.f;

    /** Precipitation intensity [mm/hr]. 0 = dry, > 10 = heavy rain. */
    UPROPERTY(BlueprintReadOnly, Category = "Microclimate")
    float PrecipitationIntensity = 0.f;

    /** Wind speed at surface level [m/s]. */
    UPROPERTY(BlueprintReadOnly, Category = "Microclimate")
    float WindSpeed_ms = 3.5f;

    /** Wind bearing [degrees, 0=North, 90=East]. */
    UPROPERTY(BlueprintReadOnly, Category = "Microclimate")
    float WindBearing_deg = 200.f;

    /**
     * Mie turbidity parameter [1.5–6.0].
     * Fed to the SwaziSkyAtmosphere shader.
     * 1.5 = clean Highveld (crisp blue), 6.0 = dusty Lowveld (amber haze).
     */
    UPROPERTY(BlueprintReadOnly, Category = "Microclimate")
    float TurbidityParam = 2.0f;

    /** True if a thunderstorm cell is active (drives ULightningSystem). */
    UPROPERTY(BlueprintReadOnly, Category = "Microclimate")
    bool bThunderstormActive = false;
};
