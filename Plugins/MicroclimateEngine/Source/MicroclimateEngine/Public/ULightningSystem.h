#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ULightningSystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLightningStrike,
                                              FVector, StrikeLocation,
                                              bool, bStruckTree);

/**
 * ELightningTreeState
 *
 * Trees struck by lightning acquire ritual significance in Swazi cosmology
 * (Umkhosi Wemali). NPC dialogue and world flags respond to Sacred state.
 */
UENUM(BlueprintType)
enum class ELightningTreeState : uint8
{
    Normal  UMETA(DisplayName = "Normal"),
    Struck  UMETA(DisplayName = "Struck (burning)"),
    Sacred  UMETA(DisplayName = "Sacred (Umkhosi Wemali)"),
    Ash     UMETA(DisplayName = "Ash (consumed)"),
};

/**
 * ULightningSystem
 *
 * Cloud charge accumulation and strike simulation.
 * When charge exceeds StrikeThreshold, fires a strike at a terrain location
 * near the storm cell centre. Trees in the strike radius transition to Sacred.
 */
UCLASS(ClassGroup=("MicroclimateEngine"), meta=(BlueprintSpawnableComponent))
class MICROCLIMATEENGINE_API ULightningSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    ULightningSystem();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                                FActorComponentTickFunction* ThisTickFunction) override;

    /** Fires when a lightning strike occurs. */
    UPROPERTY(BlueprintAssignable, Category = "Lightning")
    FOnLightningStrike OnLightningStrike;

    /** Charge threshold [0–1] at which a strike fires. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning")
    float StrikeThreshold = 0.85f;

    /** Charge accumulation rate per game-minute when storm is active. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lightning")
    float ChargeAccumulationRate = 0.08f;

    /** Current cloud charge [0–1]. */
    UPROPERTY(BlueprintReadOnly, Category = "Lightning")
    float CurrentCharge = 0.f;

    /** Update the state of a struck tree. */
    UFUNCTION(BlueprintCallable, Category = "Lightning")
    void SetTreeState(AActor* Tree, ELightningTreeState State);

    /** Query a tree's current lightning state. */
    UFUNCTION(BlueprintCallable, Category = "Lightning")
    ELightningTreeState GetTreeState(AActor* Tree) const;

private:
    void AccumulateCharge(float GameMinuteDelta, bool bStormActive);
    void FireStrike();

    TMap<AActor*, ELightningTreeState> TreeStates;

    UPROPERTY()
    class UMicroclimateSubsystem* CachedSubsystem = nullptr;
};
