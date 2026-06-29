#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UValleyInversionComponent.generated.h"

/**
 * UValleyInversionComponent
 *
 * Simulates cold-air drainage into valley channels at night, producing
 * temperature inversion fog. The Great Usuthu gorge is the primary target.
 *
 * Updates UMicroclimateSubsystem::FogBaseAltitude_m each tick.
 */
UCLASS(ClassGroup=("MicroclimateEngine"), meta=(BlueprintSpawnableComponent))
class MICROCLIMATEENGINE_API UValleyInversionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UValleyInversionComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                                FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * Compute fog base altitude for given conditions.
     *
     * @param TimeOfDay_24h      Current time [0–24h].
     * @param ValleyFloorAlt_m   Valley floor elevation [m ASL].
     * @param InversionStrength  0 = no inversion, 1 = strong.
     * @return Fog base altitude [m ASL].
     */
    UFUNCTION(BlueprintCallable, Category = "Valley Inversion")
    float ComputeFogBaseAltitude(float TimeOfDay_24h, float ValleyFloorAlt_m,
                                  float InversionStrength) const;

    /** Cold-air drainage rate [m/game-hour]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valley Inversion")
    float ColdAirDrainageRate_m_per_hr = 80.f;

    /** Altitude of the Usuthu gorge valley floor [m ASL]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Valley Inversion")
    float UsuthuGorgeFloor_m = 280.f;

private:
    float CurrentFogHeight_m = 0.f;

    UPROPERTY()
    class UMicroclimateSubsystem* CachedSubsystem = nullptr;
};
