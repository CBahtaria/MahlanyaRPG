#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FTerrainFrictionTensor.h"
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

    // ── Friction system ──────────────────────────────────────────────────────

    /** Detected physical material name from ground hit. Updated each tick. */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Locomotion")
    FName CurrentMaterialName;

    /** Resolved friction tensor for CurrentMaterialName. */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Locomotion")
    FTerrainFrictionTensor CurrentFrictionTensor;

    /** Instability margin: (RequiredFriction - EffectiveFriction). >0 = slip risk. */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Locomotion")
    float InstabilityMargin = 0.f;

    /** Threshold above which Chaos slip is triggered. Default 0.15. */
    UPROPERTY(EditAnywhere, Category = "Swazi Locomotion")
    float SlipInstabilityThreshold = 0.15f;

    // ── Biomechanical fatigue ─────────────────────────────────────────────────

    /** Current stamina [0,1]. Drains with altitude and movement speed. */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Locomotion|Stamina")
    float Stamina = 1.0f;

    /** Stamina drain per second at walk speed, sea level. */
    UPROPERTY(EditAnywhere, Category = "Swazi Locomotion|Stamina")
    float BaseStaminaDrainRate = 0.05f;

    /** Stamina recovery rate scalar (logarithmic: RecoveryRate * ln(1 + RestDuration)). */
    UPROPERTY(EditAnywhere, Category = "Swazi Locomotion|Stamina")
    float StaminaRecoveryRate = 0.08f;

    /** Altitude (m ASL) above which fatigue penalty begins. */
    UPROPERTY(EditAnywhere, Category = "Swazi Locomotion|Stamina")
    float FatigueAltitudeBaseline_m = 800.f;

    /** Altitude atop which full +40% drain is reached.
     *  At 1800m (1000m above baseline): multiplier = 1.40. */
    UPROPERTY(EditAnywhere, Category = "Swazi Locomotion|Stamina")
    float FatigueAltitudeScale_m = 1000.f;

    /** Current player altitude in metres ASL (set by landscape query or direct assignment). */
    UPROPERTY(BlueprintReadWrite, Category = "Swazi Locomotion|Stamina")
    float CurrentAltitude_m = 0.f;

    /** Seconds the character has been at rest (velocity ≈ 0). Used for log-recovery. */
    UPROPERTY(BlueprintReadOnly, Category = "Swazi Locomotion|Stamina")
    float RestDuration_s = 0.f;

    // ── Delegates ────────────────────────────────────────────────────────────

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlipStateChanged, bool, bSlipping);
    /** Fired when slip state begins (true) or ends (false). */
    UPROPERTY(BlueprintAssignable, Category = "Swazi Locomotion")
    FOnSlipStateChanged OnSlipStateChanged;

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

    /** Compute anisotropic effective friction from terrain tensor, move direction, and wetness. */
    float ComputeAnisotropicFriction(const FTerrainFrictionTensor& T, FVector MoveDir,
                                     bool bIsWet, bool bIsMoving) const;

    /** Multi-trace 5-point weighted surface normal for foot IK stability. */
    FVector ComputeWeightedSurfaceNormal() const;
};
