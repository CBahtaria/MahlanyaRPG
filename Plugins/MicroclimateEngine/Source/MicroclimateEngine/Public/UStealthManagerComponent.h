#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UStealthManagerComponent.generated.h"

/**
 * UStealthManagerComponent
 *
 * Computes effective NPC detection range accounting for:
 *   - Volumetric fog density (UMicroclimateSubsystem::GetVolumetricDensityAt)
 *   - Smoke density (controlled burns)
 *   - Precipitation intensity (rain reduces visibility)
 *   - Time of day (night multiplier)
 *
 * Cross-Repo Upgrade 2 pattern (godot-4-fog-of-war): updates only when
 * the player moves > 50 cm (not per-frame) to match the texture-multiply
 * "lazy update" pattern.
 */
UCLASS(ClassGroup=("MicroclimateEngine"), meta=(BlueprintSpawnableComponent))
class MICROCLIMATEENGINE_API UStealthManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStealthManagerComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                                FActorComponentTickFunction* ThisTickFunction) override;

    /** Effective NPC detection range [cm] at the player's current location. */
    UFUNCTION(BlueprintCallable, Category = "Stealth")
    float ComputeDetectionRange() const;

    /** Base detection range [cm] in clear conditions (default 50m). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stealth")
    float BaseDetectionRange = 5000.f;

    /** Fractional range multiplier at night [0–1]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stealth")
    float NightDetectionMultiplier = 0.4f;

    /** Cached result of last ComputeDetectionRange call. */
    UPROPERTY(BlueprintReadOnly, Category = "Stealth")
    float CachedDetectionRange = 5000.f;

private:
    FVector LastUpdateLocation = FVector(FLT_MAX, FLT_MAX, FLT_MAX);

    static constexpr float UPDATE_DISTANCE_CM = 50.f;

    bool IsNightTime() const;

    UPROPERTY()
    class UMicroclimateSubsystem* CachedSubsystem = nullptr;
};
