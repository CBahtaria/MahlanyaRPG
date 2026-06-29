#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "FAtmosphericAcoustics.h"
#include "UGeometricAudioComponent.generated.h"

/**
 * FRayAcousticHit
 *
 * A single acoustic ray reflection hit, accumulated during
 * UGeometricAudioComponent::TraceAcousticRays.
 */
USTRUCT(BlueprintType)
struct GEOMETRICAUDIOPLUGIN_API FRayAcousticHit
{
    GENERATED_BODY()

    /** Path length from source to this hit to receiver [m]. */
    UPROPERTY(BlueprintReadOnly) float Distance_m = 0.f;

    /** Propagation delay [s] = Distance_m / EffectiveSpeedOfSound. */
    UPROPERTY(BlueprintReadOnly) float DelaySeconds = 0.f;

    /** Per-octave-band energy after absorption at each surface hit. */
    UPROPERTY(BlueprintReadOnly) TArray<float> EnergyPerBand;
};

/**
 * UGeometricAudioComponent
 *
 * Extends UAudioComponent with runtime geometric ray-casting for open-air sources.
 * Fires NUM_RAYS rays from the source; each ray bounces up to MAX_BOUNCES times,
 * querying UAcousticMaterialComponent on hit geometry for per-band absorption.
 *
 * Integrates with UMicroclimateSubsystem for atmospheric corrections.
 *
 * For stationary or pre-baked environments, use UAcousticZoneComponent + convolution
 * reverb instead — this component is for dynamic open-air situations (distant drums,
 * shouting across a gorge, stampede rumble).
 */
UCLASS(ClassGroup=("GeometricAudio"), meta=(BlueprintSpawnableComponent))
class GEOMETRICAUDIOPLUGIN_API UGeometricAudioComponent : public UAudioComponent
{
    GENERATED_BODY()

public:
    UGeometricAudioComponent();

    /**
     * Compute the runtime IR for a source at SourceLocation heard at ListenerLocation.
     * Ray-casts in world space, queries UAcousticMaterialComponent on hit actors.
     *
     * @param SourceLocation    World position of the sound source.
     * @param ListenerLocation  World position of the listener (player ears).
     * @return                  Array of ray hits sorted by arrival delay.
     */
    UFUNCTION(BlueprintCallable, Category = "Geometric Audio")
    TArray<FRayAcousticHit> ComputeRuntimeIR(const FVector& SourceLocation,
                                              const FVector& ListenerLocation);

    /**
     * Update the atmospheric state used for propagation corrections.
     * Call when UMicroclimateSubsystem broadcasts a state change.
     */
    UFUNCTION(BlueprintCallable, Category = "Geometric Audio")
    void SetAtmosphericState(const FAtmosphericAcoustics& NewState)
    { AtmosphericState = NewState; }

    /** Number of rays fired per IR computation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometric Audio|Ray Tracing",
              meta=(ClampMin="1", ClampMax="512"))
    int32 NumRays = 64;

    /** Maximum ray bounces per ray path. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometric Audio|Ray Tracing",
              meta=(ClampMin="1", ClampMax="32"))
    int32 MaxBounces = 6;

    /**
     * Capture sphere radius [cm].
     * Ray paths passing within this distance of the listener are counted.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometric Audio|Ray Tracing")
    float CaptureRadius_cm = 100.f;

private:
    FAtmosphericAcoustics AtmosphericState;

    /** Fire a single ray from Origin toward Target direction; collect hits. */
    TArray<FRayAcousticHit> TraceAcousticRays(const FVector& Origin,
                                                const FVector& Target);

    /** Read absorption coefficients from a hit actor's UAcousticMaterialComponent. */
    static TArray<float> GetHitAbsorption(const AActor* HitActor);

    /** Accumulate energy through a sequence of ray hits. */
    static FRayAcousticHit BuildHitRecord(float PathLength_m,
                                           const TArray<TArray<float>>& AbsorptionSequence,
                                           float EffectiveSpeedMs);
};
