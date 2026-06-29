#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FMicroclimateState.h"
#include "UMicroclimateSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRainIntensityChanged,
                                             float, NewIntensity_mm_per_hr);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTerrainSaturationChanged,
                                             float, SaturationFraction);

/**
 * UMicroclimateSubsystem
 *
 * Server-authoritative world subsystem running a simplified barometric
 * pressure simulation and orographic rainfall model for Eswatini.
 *
 * Publishes:
 *   FOnRainIntensityChanged  → subscribed by ErosionRuntimePlugin, LocomotionPhysicsPlugin
 *   FOnTerrainSaturationChanged → subscribed by ErosionRuntimePlugin
 */
UCLASS()
class MICROCLIMATEENGINE_API UMicroclimateSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    //~ Begin UWorldSubsystem Interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
    //~ End UWorldSubsystem Interface

    virtual void Tick(float DeltaTime);
    virtual bool IsTickable() const { return true; }
    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(UMicroclimateSubsystem, STATGROUP_Tickables);
    }

    /** Current atmospheric snapshot. */
    UFUNCTION(BlueprintCallable, Category = "Microclimate")
    const FMicroclimateState& GetCurrentState() const { return CurrentState; }

    /**
     * Volumetric fog density at a world position.
     * Spikes below FogBaseAltitude_m (valley inversion layer).
     */
    UFUNCTION(BlueprintCallable, Category = "Microclimate")
    float GetVolumetricDensityAt(const FVector& WorldLocation) const;

    /** Smoke density at a world position (from controlled burns). */
    UFUNCTION(BlueprintCallable, Category = "Microclimate")
    float GetSmokeDensityAt(const FVector& WorldLocation) const;

    /** Manually trigger a rain event (e.g., from historical calendar). */
    UFUNCTION(BlueprintCallable, Category = "Microclimate")
    void TriggerRainEvent(float Intensity_mm_per_hr, float Duration_GameMinutes);

    /** Register a smoke source from a controlled burn. */
    UFUNCTION(BlueprintCallable, Category = "Microclimate")
    void RegisterSmokeSource(FVector WorldLocation, float IntensityFraction);

    /** Extinguish all smoke sources (rain). */
    UFUNCTION(BlueprintCallable, Category = "Microclimate")
    void ClearSmokeSources();

    /** Set fog base altitude directly (called by UValleyInversionComponent). */
    void SetFogBaseAltitude(float AltitudeMetres) { CurrentState.FogBaseAltitude_m = AltitudeMetres; }

    /** Broadcast when precipitation changes significantly (>1 mm/hr delta). */
    UPROPERTY(BlueprintAssignable, Category = "Microclimate|Events")
    FOnRainIntensityChanged OnRainIntensityChanged;

    /** Broadcast when terrain saturation crosses 0.25/0.5/0.75 thresholds. */
    UPROPERTY(BlueprintAssignable, Category = "Microclimate|Events")
    FOnTerrainSaturationChanged OnTerrainSaturationChanged;

private:
    FMicroclimateState CurrentState;

    float RainAccumulation_mm = 0.f;
    float LastBroadcastRain = -999.f;
    int32 LastSaturationTier = -1;
    float RainEventRemaining_GameMin = 0.f;

    TArray<TTuple<FVector, float>> SmokeSources;
    TArray<float> PressureHistory;

    void SimulateMinuteTick(float GameMinuteDelta);
    void BroadcastStateChanges(float PrevRainIntensity);
};
