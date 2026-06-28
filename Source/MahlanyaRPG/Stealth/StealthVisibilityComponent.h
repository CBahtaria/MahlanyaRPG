#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StealthVisibilityComponent.generated.h"

// ---------------------------------------------------------------------------
// NOTE: MicroclimateEngine subsystem header will be included here once the
// plugin is implemented.
//
// #include "MicroclimateEngine/UMicroclimateSubsystem.h"
//
// When wiring:
//   1. Add MicroclimateEngine to MahlanyaRPG.Build.cs PublicDependencyModuleNames.
//   2. Replace SetFogDensityAtLocation injection calls with a direct subsystem
//      query: UMicroclimateSubsystem::Get()->SampleFogDensity(WorldLocation).
//   3. Wire the Niagara smoke event to SetSmokeDensityAtLocation via a
//      UNiagaraDataInterfaceExport listener on each fire/smoke system.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// UStealthVisibilityComponent
//
// Computes a [0.05, 1.0] visibility fraction for the owning actor by combining
// two environmental occlusion layers:
//
//   1. FogDensity   — volumetric fog density at the actor's world location,
//                     injected by MicroclimateEngine (future subsystem).
//   2. SmokeDensity — smoke density from active fire/smoke Niagara systems.
//
// Combined mask formula:
//   VisibilityMask = Clamp(FogMask * SmokeMask, 0.05, 1.0)
//   where FogMask   = 1.0 - FogDensity
//         SmokeMask = 1.0 - SmokeDensity
//
// Performance:
//   Inspired by the godot-4-fog-of-war texture-multiply pattern.
//   The mask is recomputed only when the owner has moved more than
//   UpdateDistanceThreshold (default 50 cm) since the last computation,
//   avoiding a full recalculation every frame.
//
// Usage:
//   float DetectionRange = Component->BaseDetectionRange
//                        * Component->GetVisibilityMask();
// ---------------------------------------------------------------------------
UCLASS(ClassGroup=(Mahlanya), meta=(BlueprintSpawnableComponent))
class MAHLANYARPG_API UStealthVisibilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStealthVisibilityComponent();

    // -----------------------------------------------------------------------
    // Density Injection (called by external systems)
    // -----------------------------------------------------------------------

    /**
     * Set the volumetric fog density [0, 1] at the owner's current location.
     * Called by MicroclimateEngine when the fog field is updated.
     * 0 = no fog (fully visible); 1 = maximum fog (minimum visibility).
     */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Stealth")
    void SetFogDensityAtLocation(float Density);

    /**
     * Set the smoke density [0, 1] at the owner's current location.
     * Called by active fire/smoke Niagara systems via data interface export.
     * 0 = no smoke; 1 = fully smoke-occluded.
     */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Stealth")
    void SetSmokeDensityAtLocation(float Density);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /**
     * Returns the current visibility mask in [0.05, 1.0].
     * Multiply by BaseDetectionRange to get the effective detection radius.
     *   1.0 = owner is fully visible at maximum range.
     *   0.05 = owner is nearly invisible (5% of normal detection range).
     */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Stealth")
    float GetVisibilityMask() const;

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------

    /**
     * Base detection range in UE5 centimetres.
     * NPCs sample this to build their perception radius:
     *   float PerceptionRadius = BaseDetectionRange * GetVisibilityMask();
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mahlanya|Stealth",
        meta=(ClampMin="0.0", UIMin="0.0"))
    float BaseDetectionRange = 3000.f;

    /**
     * Minimum owner displacement (cm) required before the mask is recomputed.
     * Keeps the update budget low when the owner is stationary or slow-moving.
     * Default 50 cm matches the godot-4-fog-of-war 0.5-unit threshold scaled
     * to UE5's centimetre coordinate system.
     */
    UPROPERTY(EditAnywhere, Category = "Mahlanya|Stealth",
        meta=(ClampMin="1.0", UIMin="1.0"))
    float UpdateDistanceThreshold = 50.f;

    // -----------------------------------------------------------------------
    // UActorComponent overrides
    // -----------------------------------------------------------------------

    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

private:
    /** Fog density [0,1] sampled from MicroclimateEngine at the last update. */
    float FogDensity = 0.f;

    /** Smoke density [0,1] sampled from Niagara fire/smoke at the last update. */
    float SmokeDensity = 0.f;

    /** Last computed visibility mask; returned by GetVisibilityMask(). */
    float CachedMask = 1.0f;

    /** Owner world location at the time of the last mask recomputation. */
    FVector LastUpdateLocation = FVector::ZeroVector;

    /**
     * Recompute CachedMask from current FogDensity and SmokeDensity.
     * Only called when the owner has moved beyond UpdateDistanceThreshold.
     */
    void RecomputeMask();
};
