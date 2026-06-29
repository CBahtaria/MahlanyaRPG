#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UHerdingController.generated.h"

UENUM(BlueprintType)
enum class EHerdState : uint8 {
    Calm,        // Normal grazing/following
    Agitated,    // Responding to steering
    Stampeding,  // Collective momentum exceeded threshold
};

USTRUCT(BlueprintType)
struct FCattleAgent {
    GENERATED_BODY()
    UPROPERTY() AActor* Actor = nullptr;
    UPROPERTY() FVector Velocity = FVector::ZeroVector;
    UPROPERTY() float Mass_kg = 450.f;  // Average Nguni cattle mass
};

UCLASS(ClassGroup=(Mahlanya), meta=(BlueprintSpawnableComponent))
class LOCOMOTIONPHYSICSPLUGIN_API UHerdingController : public UActorComponent
{
    GENERATED_BODY()
public:
    UHerdingController();

    /** Register a cattle actor with the herd. */
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Herding")
    void AddCattleAgent(AActor* CattleActor, float Mass_kg = 450.f);

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Herding")
    void RemoveCattleAgent(AActor* CattleActor);

    /** Apply a steering force from the player (shout direction, Umshiza gesture).
     *  Direction: normalised world-space vector. Magnitude: 0-1 force strength.
     *  Applied as a repulsion vector to all cattle within RepulsionRadius_cm. */
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Herding")
    void ApplySteeringForce(FVector Origin, FVector Direction, float Magnitude = 1.0f);

    /** Current aggregate herd state. */
    UPROPERTY(BlueprintReadOnly, Category="Mahlanya|Herding")
    EHerdState HerdState = EHerdState::Calm;

    /** Aggregate momentum magnitude (kg·cm/s). Stampede fires above StampedeThreshold. */
    UPROPERTY(BlueprintReadOnly, Category="Mahlanya|Herding")
    float AggregateMomentum = 0.f;

    /** Momentum threshold above which stampede begins (default 50,000 kg·cm/s). */
    UPROPERTY(EditAnywhere, Category="Mahlanya|Herding")
    float StampedeThreshold = 50000.f;

    /** Radius within which a steering force affects cattle (cm). */
    UPROPERTY(EditAnywhere, Category="Mahlanya|Herding")
    float RepulsionRadius_cm = 500.f;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStampede);
    /** Fired when stampede begins. */
    UPROPERTY(BlueprintAssignable, Category="Mahlanya|Herding")
    FOnStampede OnStampede;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHerdCalmed);
    /** Fired when stampede ends and herd returns to Calm. */
    UPROPERTY(BlueprintAssignable, Category="Mahlanya|Herding")
    FOnHerdCalmed OnHerdCalmed;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

protected:
    UPROPERTY() TArray<FCattleAgent> CattleAgents;

private:
    float StampedeTimer = 0.f;  // Seconds in stampede state
    float CalmTimer     = 0.f;  // Seconds calming down after stampede

    // Cached DeltaTime for use inside CheckStampedeTransitions
    float LastDeltaTime = 0.f;

    void UpdateAggregateMomentum();
    void CheckStampedeTransitions();
    /** Apply velocity dampening each tick (drag model). */
    void DampenVelocities(float DeltaTime);
};
