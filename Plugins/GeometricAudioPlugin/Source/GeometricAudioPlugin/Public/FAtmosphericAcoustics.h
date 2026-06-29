#pragma once

#include "CoreMinimal.h"
#include "FAtmosphericAcoustics.generated.h"

/**
 * FAtmosphericAcoustics
 *
 * Utility functions for atmospheric acoustic propagation.
 * Used by UGeometricAudioComponent to compute range and absorption corrections.
 *
 * Based on:
 *   - ISO 9613-1 atmospheric absorption
 *   - Salomons 2001 computational atmospheric acoustics (inversion model)
 */
USTRUCT(BlueprintType)
struct GEOMETRICAUDIOPLUGIN_API FAtmosphericAcoustics
{
    GENERATED_BODY()

    /**
     * Speed of sound in air [m/s].
     * Standard formula: 331.3 + 0.606 * T_celsius.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Atmospheric Acoustics")
    float AirTemperature_C = 20.f;

    /** Wind velocity vector [m/s, world-space]. Affects effective propagation speed. */
    UPROPERTY(BlueprintReadOnly, Category = "Atmospheric Acoustics")
    FVector WindVelocity = FVector::ZeroVector;

    /**
     * Temperature gradient [°C/100m].
     * Positive = temperature inversion (warm over cold air) →
     *   sound bends downward → anomalously long-range propagation.
     * Negative = normal lapse rate → no anomalous effect.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Atmospheric Acoustics")
    float TempGradient_C_per_100m = -0.65f;

    /** Relative humidity [0–1]. Affects HF molecular absorption. */
    UPROPERTY(BlueprintReadOnly, Category = "Atmospheric Acoustics")
    float RelativeHumidity = 0.60f;

    // ── Static utilities ──────────────────────────────────────────────────

    /** Speed of sound [m/s] at a given air temperature. */
    static float SpeedOfSound(float TempCelsius)
    {
        return 331.3f + 0.606f * TempCelsius;
    }

    /**
     * Effective sound speed along a propagation direction including wind.
     *
     * @param PropagationDir  Normalised world-space direction from source to listener.
     * @param WindVelocity    Wind velocity vector [m/s].
     * @param TempCelsius     Air temperature.
     * @return                Effective speed [m/s].
     */
    static float EffectiveSoundSpeed(const FVector& PropagationDir,
                                      const FVector& WindVelocity,
                                      float TempCelsius)
    {
        const float C = SpeedOfSound(TempCelsius);
        const float WindComponent = FVector::DotProduct(
            PropagationDir.GetSafeNormal(), WindVelocity);
        return C + WindComponent;
    }

    /**
     * Range multiplier for anomalous propagation under temperature inversion.
     *
     * Positive gradient = warm air over cold → sound refracts toward ground →
     * effectively longer acoustic range.
     *
     * @param TempGradient_C_per_100m  Temperature gradient.
     * @return Range multiplier (≥ 1.0).
     */
    static float InversionRangeMultiplier(float TempGradient_C_per_100m)
    {
        if (TempGradient_C_per_100m > 0.f)
            return 1.f + TempGradient_C_per_100m * 0.15f;
        return 1.f;
    }

    /**
     * ISO 9613-1 high-frequency atmospheric absorption [dB/m].
     *
     * @param FrequencyHz       Octave band centre frequency [Hz].
     * @param RelativeHumidity  [0–1].
     * @return                  Absorption coefficient [dB/m].
     */
    static float AtmosphericAbsorption_dB_per_m(float FrequencyHz,
                                                  float RelativeHumidity)
    {
        // Simplified ISO 9613-1: scales as f^1.5, reduced by humidity
        const float HumidFactor = 1.f - RelativeHumidity * 0.4f;
        const float FreqFactor = FMath::Pow(FrequencyHz / 1000.f, 1.5f);
        return 0.002f * FreqFactor * HumidFactor;
    }
};
