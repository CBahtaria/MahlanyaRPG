// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "URuntimeHeightmapComponent.generated.h"

class UTextureRenderTarget2D;

/**
 * URuntimeHeightmapComponent
 *
 * Session-local GPU displacement overlay for Mahlanya terrain.
 * Attach to any actor that should drive or read runtime terrain displacement
 * (typically placed on the player pawn or a dedicated landscape manager actor).
 *
 * Rain events:
 *   MicroclimateEngine → SimulationBus::OnRainIntensityChanged
 *   → URuntimeHeightmapComponent::OnRainIntensityChanged
 *   → FTimerHandle fires DispatchErosionIteration() at 50 Hz while raining
 *   → Compute shader (ErosionCS.usf) writes displacement to DisplacementTarget
 *
 * Footprints / wheel ruts:
 *   Any caller → ApplyFootprint(WorldLocation, Depth)
 *   → Updates DisplacementTarget via RHIUpdateTexture2D on the render thread
 *
 * Session-local contract:
 *   ResetToBaseline() clears DisplacementTarget to zero.
 *   Called automatically from EndPlay so multiplayer determinism is preserved.
 *
 * DisplacementTarget is a UTextureRenderTarget2D exposed as a material parameter
 * so the Nanite landscape material can apply the overlay without runtime Nanite.
 */
UCLASS(ClassGroup=(Mahlanya), meta=(BlueprintSpawnableComponent))
class EROSIONRUNTIMEPLUGIN_API URuntimeHeightmapComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    URuntimeHeightmapComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ── Displacement API ──────────────────────────────────────────────────────

    // Apply a single footprint or surface depression at a world XY location.
    // Depth is the displacement in cm (positive = downward into terrain).
    // Radius is the blur radius of the footprint kernel in texture texels.
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Terrain")
    void ApplyFootprint(FVector WorldLocation, float Depth, int32 RadiusTexels = 3);

    // Clear all session-local displacement and return to the baked heightmap.
    // Automatically called from EndPlay; can also be called manually.
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Terrain")
    void ResetToBaseline();

    // ── Configuration ─────────────────────────────────────────────────────────

    // World-space XY origin of the displacement patch (lower-left corner, cm).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mahlanya|Terrain")
    FVector2D PatchOriginXY = FVector2D::ZeroVector;

    // Half-extent of the patch in cm. Default 20,000 cm = 200 m radius.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mahlanya|Terrain",
              meta=(ClampMin="100.0"))
    float PatchRadiusCm = 20000.f;

    // Resolution of the displacement texture (both axes). Must be power-of-two.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mahlanya|Terrain",
              meta=(ClampMin="64", ClampMax="1024"))
    int32 PatchResolution = 256;

    // Erosion dispatch rate while it is raining (iterations per second).
    // Each iteration runs one pass of the GPU erosion kernel.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mahlanya|Terrain",
              meta=(ClampMin="1.0", ClampMax="120.0"))
    float ErosionIterationsPerSecond = 50.f;

    // ── Output ────────────────────────────────────────────────────────────────

    // Output render target. Bind this as a material parameter on the landscape
    // material (e.g. "DisplacementRT") so the erosion overlay is applied.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mahlanya|Terrain")
    TObjectPtr<UTextureRenderTarget2D> DisplacementTarget;

    // Current cumulative saturation of the displacement buffer [0, 1].
    // Broadcast back to SimulationBus as FOnTerrainSaturationChanged.
    UFUNCTION(BlueprintPure, Category="Mahlanya|Terrain")
    float GetTerrainSaturation() const { return AccumulatedSaturation; }

private:
    void OnRainIntensityChanged(float IntensityMmPerHr);
    void DispatchErosionIteration();

    float CurrentRainIntensity  = 0.f;
    float AccumulatedSaturation = 0.f;

    FTimerHandle  ErosionTimerHandle;
    FDelegateHandle RainDelegateHandle;
};
