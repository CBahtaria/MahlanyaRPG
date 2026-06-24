#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SwaziKineticLocomotion.generated.h"

class ACharacter;

/**
 * USwaziKineticLocomotion
 *
 * Physics-accurate locomotion component for Mahlanya.
 * Reads terrain material friction tensors and surface normals each tick
 * to dynamically adjust walk speed, ground friction, and slip state.
 *
 * Attach to the Mahlanya character BP alongside CharacterMovementComponent.
 * Reads wet/dry state from USimulationBusSubsystem::OnRainIntensityChanged.
 */
UCLASS(ClassGroup = (MahlanyaRPG), meta = (BlueprintSpawnableComponent))
class LOCOMOTIONPHYSICSPLUGIN_API USwaziKineticLocomotion : public UActorComponent
{
    GENERATED_BODY()

public:
    USwaziKineticLocomotion();

    // ── Config ───────────────────────────────────────────────────────────────

    /** Base ground friction coefficient on dry level terrain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swazi Locomotion")
    float BaseFrictionCoefficient = 0.8f;

    /** Friction scalar applied when terrain is wet (rain intensity > threshold). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swazi Locomotion")
    float WetClayFrictionScalar = 0.22f;

    /**
     * Rain intensity threshold (mm/hr) above which terrain is considered wet.
     * Set by MicroclimateEngine via OnRainIntensityChanged delegate.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swazi Locomotion")
    float WetRainThresholdMmPerHr = 2.0f;

    /** Friction margin below which a kinetic slide is triggered. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swazi Locomotion")
    float SlipFrictionThreshold = 0.3f;

    /** Slope dot product below which slip is possible (≈ sin(38°) ≈ 0.62 → SlopeDot < 0.78). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swazi Locomotion")
    float SlipSlopeThreshold = 0.78f;

    // ── Runtime state ────────────────────────────────────────────────────────

    /** Current rain intensity (mm/hr), updated by SimulationBus. */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Locomotion")
    float CurrentRainIntensityMmPerHr = 0.0f;

    /** True when Mahlanya is in a kinetic slide state. */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Locomotion")
    bool bIsSliding = false;

    // ── Interface ─────────────────────────────────────────────────────────────

    /** Called by SimulationBus delegate when rain intensity changes. */
    UFUNCTION()
    void OnRainIntensityChanged(float IntensityMmPerHr);

    /** Dispatches gameplay cue or animation state to trigger the slide recovery pose. */
    UFUNCTION(BlueprintNativeEvent, Category = "Swazi Locomotion")
    void TriggerKineticSlideState();
    virtual void TriggerKineticSlideState_Implementation();

    // UActorComponent interface
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                                FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY()
    ACharacter* OwningCharacter = nullptr;

    /** Compute the effective friction from anisotropic terrain material + wetness. */
    float ComputeEffectiveFriction(float BaseFriction, bool bIsWet, float SlopeDot) const;

    /** Multi-trace 5-point weighted surface normal for foot IK stability. */
    FVector ComputeWeightedSurfaceNormal() const;
};
