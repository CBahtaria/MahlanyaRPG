#include "SwaziKineticLocomotion.h"
#include "SimulationBusSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

USwaziKineticLocomotion::USwaziKineticLocomotion()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USwaziKineticLocomotion::BeginPlay()
{
    Super::BeginPlay();

    OwningCharacter = Cast<ACharacter>(GetOwner());
    if (!OwningCharacter)
    {
        UE_LOG(LogTemp, Warning, TEXT("USwaziKineticLocomotion: Owner is not an ACharacter. Disabling tick."));
        PrimaryComponentTick.bCanEverTick = false;
        return;
    }

    // Subscribe to rain intensity changes from the SimulationBus
    if (USimulationBusSubsystem* Bus = GetWorld()->GetSubsystem<USimulationBusSubsystem>())
    {
        Bus->OnRainIntensityChanged.AddUObject(this, &USwaziKineticLocomotion::OnRainIntensityChanged);
    }
}

void USwaziKineticLocomotion::OnRainIntensityChanged(float IntensityMmPerHr)
{
    CurrentRainIntensityMmPerHr = IntensityMmPerHr;
}

FVector USwaziKineticLocomotion::ComputeWeightedSurfaceNormal() const
{
    // 5-point multi-trace for a stable weighted surface normal:
    // centre + four cardinal offsets at 30cm radius from capsule base.
    const float CapsuleHalfHeight = OwningCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector ActorLoc        = OwningCharacter->GetActorLocation();
    const float TraceLen          = CapsuleHalfHeight + 40.0f;
    const float OffsetRadius      = 30.0f;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwningCharacter);

    const TArray<FVector> Offsets = {
        FVector(0,0,0),
        FVector(OffsetRadius, 0, 0), FVector(-OffsetRadius, 0, 0),
        FVector(0, OffsetRadius, 0), FVector(0, -OffsetRadius, 0),
    };

    FVector AccumulatedNormal = FVector::ZeroVector;
    int32   HitCount = 0;

    for (const FVector& Offset : Offsets)
    {
        FHitResult Hit;
        FVector Start = ActorLoc + Offset;
        FVector End   = Start - FVector(0, 0, TraceLen);
        if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
        {
            AccumulatedNormal += Hit.Normal;
            ++HitCount;
        }
    }

    return HitCount > 0 ? (AccumulatedNormal / HitCount).GetSafeNormal() : FVector::UpVector;
}

float USwaziKineticLocomotion::ComputeEffectiveFriction(
    float BaseFriction, bool bIsWet, float SlopeDot) const
{
    float Friction = BaseFriction;
    if (bIsWet)
    {
        Friction *= WetClayFrictionScalar;
    }
    // Reduce friction slightly on steeper slopes (less contact normal force)
    Friction *= FMath::Lerp(0.7f, 1.0f, FMath::Clamp(SlopeDot, 0.f, 1.f));
    return Friction;
}

void USwaziKineticLocomotion::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!OwningCharacter) return;

    UCharacterMovementComponent* MoveComp = OwningCharacter->GetCharacterMovement();
    if (!MoveComp || !MoveComp->IsMovingOnGround()) return;

    // 1. Compute weighted surface normal from 5-point ground trace
    FVector SurfaceNormal  = ComputeWeightedSurfaceNormal();
    FVector VelocityDir    = OwningCharacter->GetVelocity().GetSafeNormal();

    // 2. Slope severity: dot product of world-up and surface normal
    //    SlopeDot = 1.0 on flat ground, decreasing toward 0.0 on vertical wall
    float SlopeDot = FVector::DotProduct(FVector::UpVector, SurfaceNormal);

    // 3. Wetness from rain intensity state (set via OnRainIntensityChanged)
    bool bIsWet = CurrentRainIntensityMmPerHr > WetRainThresholdMmPerHr;

    // 4. Compute effective friction for this material + wetness + slope
    float EffectiveFriction = ComputeEffectiveFriction(BaseFrictionCoefficient, bIsWet, SlopeDot);

    if (SlopeDot < 0.98f)  // On an incline (not flat)
    {
        // Direction of climbing: negative dot = moving against the surface normal (uphill)
        float ClimbDot = FVector::DotProduct(VelocityDir, SurfaceNormal);

        if (ClimbDot < 0.0f)  // Ascending
        {
            MoveComp->MaxWalkSpeed  = FMath::Lerp(120.0f, 450.0f, SlopeDot);
            MoveComp->GroundFriction = EffectiveFriction * 1.5f;
        }
        else  // Descending
        {
            MoveComp->MaxWalkSpeed  = FMath::Lerp(850.0f, 450.0f, SlopeDot);
            MoveComp->GroundFriction = EffectiveFriction * 0.4f;

            // Trigger kinetic slide if friction + slope thresholds both breached
            if (EffectiveFriction < SlipFrictionThreshold && SlopeDot < SlipSlopeThreshold)
            {
                if (!bIsSliding)
                {
                    bIsSliding = true;
                    TriggerKineticSlideState();
                }
            }
            else
            {
                bIsSliding = false;
            }
        }
    }
    else  // Flat terrain — restore standard values
    {
        MoveComp->MaxWalkSpeed   = 600.0f;
        MoveComp->GroundFriction = EffectiveFriction;
        bIsSliding = false;
    }
}

void USwaziKineticLocomotion::TriggerKineticSlideState_Implementation()
{
    // Blueprint implementable — dispatches GameplayCue or animation state switch
    // to engage the Umshiza fighting-stick anchor slide pose in ABP_Mahlanya.
    UE_LOG(LogTemp, Log, TEXT("SwaziKineticLocomotion: Kinetic slide triggered — Umshiza anchor pose"));
}
