#include "SwaziKineticLocomotion.h"
#include "FTerrainFrictionTensor.h"
#include "SimulationBusSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

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

float USwaziKineticLocomotion::ComputeAnisotropicFriction(
    const FTerrainFrictionTensor& T, FVector MoveDir, bool bIsWet, bool bIsMoving) const
{
    // Step 1: Select base friction value from wetness and motion state
    float Base;
    if (bIsWet && bIsMoving)
    {
        Base = T.DynamicWet;
    }
    else if (bIsWet && !bIsMoving)
    {
        Base = T.StaticWet;
    }
    else if (!bIsWet && bIsMoving)
    {
        Base = T.DynamicDry;
    }
    else
    {
        Base = T.StaticDry;
    }

    // Step 2: Isotropic fast path
    if (T.AnisotropyRatio <= 1.01f)
    {
        return Base;
    }

    // Step 3: Compute bearing of movement direction in degrees [0, 360)
    float Bearing = FMath::RadiansToDegrees(FMath::Atan2(MoveDir.Y, MoveDir.X));
    if (Bearing < 0.f)
    {
        Bearing += 360.f;
    }

    // Step 4: Angular difference, clamped to [0, 90] for symmetrical response
    float AngularDiff = FMath::Abs(FMath::FindDeltaAngleDegrees(Bearing, T.AnisotropyAxis));
    AngularDiff = FMath::Min(AngularDiff, 180.f - AngularDiff);

    // Step 5: Cosine factor from angular difference
    const float CosFactor = FMath::Cos(FMath::DegreesToRadians(AngularDiff));

    // Step 6: Friction multiplier blends between min-grip (1/ratio) and max-grip (1.0)
    const float FrictionMultiplier =
        (1.f / T.AnisotropyRatio) + (1.f - 1.f / T.AnisotropyRatio) * (CosFactor * CosFactor);

    // Step 7: Return scaled friction
    return Base * FrictionMultiplier;
}

void USwaziKineticLocomotion::TickComponent(
    float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!OwningCharacter) return;

    UCharacterMovementComponent* MoveComp = OwningCharacter->GetCharacterMovement();
    if (!MoveComp || !MoveComp->IsMovingOnGround()) return;

    // ── Step 1: Compute weighted surface normal from 5-point ground trace ──────
    FVector SurfaceNormal = ComputeWeightedSurfaceNormal();

    // ── Step 2: Sample physical material from centre foot trace hit ───────────
    {
        const float CapsuleHalfHeight = OwningCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector ActorLoc        = OwningCharacter->GetActorLocation();
        const float TraceLen          = CapsuleHalfHeight + 40.0f;

        FCollisionQueryParams Params;
        Params.AddIgnoredActor(OwningCharacter);
        Params.bReturnPhysicalMaterial = true;

        FHitResult CentreHit;
        FVector Start = ActorLoc;
        FVector End   = Start - FVector(0, 0, TraceLen);

        if (GetWorld()->LineTraceSingleByChannel(CentreHit, Start, End, ECC_WorldStatic, Params)
            && CentreHit.PhysMaterial.IsValid())
        {
            CurrentMaterialName = CentreHit.PhysMaterial->GetFName();
        }
        else
        {
            CurrentMaterialName = FName(TEXT("PM_GrasslandDry"));
        }

        CurrentFrictionTensor = FTerrainFrictionRegistry::Lookup(CurrentMaterialName);
    }

    // ── Step 3: Determine wetness ─────────────────────────────────────────────
    const bool bIsWet = CurrentRainIntensityMmPerHr > WetRainThresholdMmPerHr;

    // ── Step 4: Slope dot product (1.0 = flat, 0.0 = vertical) ───────────────
    const float SlopeDot = FVector::DotProduct(FVector::UpVector, SurfaceNormal);

    // ── Step 5-6: Movement direction and speed ────────────────────────────────
    const FVector Velocity  = OwningCharacter->GetVelocity();
    const FVector MoveDir   = Velocity.GetSafeNormal();
    const bool    bIsMoving = Velocity.Size() > 10.f;

    // ── Step 7: Anisotropic effective friction ────────────────────────────────
    const float EffectiveFriction =
        ComputeAnisotropicFriction(CurrentFrictionTensor, MoveDir, bIsWet, bIsMoving);

    // ── Step 8: Required friction from slope angle ────────────────────────────
    // SlopeAngle = acos(SlopeDot); RequiredFriction = sin(SlopeAngle)
    // sin(acos(x)) = sqrt(1 - x^2), avoids trig overhead
    const float SlopeAngle       = FMath::Acos(FMath::Clamp(SlopeDot, 0.f, 1.f));
    const float RequiredFriction = FMath::Sin(SlopeAngle);
    InstabilityMargin = RequiredFriction - EffectiveFriction;

    // ── Step 9: Chaos slip state transitions ──────────────────────────────────
    if (InstabilityMargin > SlipInstabilityThreshold && !bIsSliding)
    {
        bIsSliding = true;
        OnSlipStateChanged.Broadcast(true);
        TriggerKineticSlideState();
    }
    else if (InstabilityMargin <= 0.f && bIsSliding)
    {
        bIsSliding = false;
        OnSlipStateChanged.Broadcast(false);
    }

    // ── Step 10: Apply walk speed and ground friction ─────────────────────────
    MoveComp->MaxWalkSpeed  = FMath::Lerp(200.f, 600.f, SlopeDot) * (bIsWet ? 0.85f : 1.f);
    MoveComp->GroundFriction = EffectiveFriction * 8.f;  // UE5 ground friction is ~0-8 scale

    // ── Step 11: Stamina update ───────────────────────────────────────────────
    const float AltitudeMultiplier =
        1.f + FMath::Max(0.f, (CurrentAltitude_m - FatigueAltitudeBaseline_m)
                              / FatigueAltitudeScale_m) * 0.40f;

    const float SpeedFactor = FMath::Clamp(Velocity.Size() / 600.f, 0.f, 2.f);

    if (SpeedFactor > 0.05f)
    {
        Stamina       -= BaseStaminaDrainRate * SpeedFactor * AltitudeMultiplier * DeltaTime;
        RestDuration_s = 0.f;
    }
    else
    {
        RestDuration_s += DeltaTime;
        Stamina        += StaminaRecoveryRate * FMath::Loge(1.f + RestDuration_s) * DeltaTime;
    }

    Stamina = FMath::Clamp(Stamina, 0.f, 1.f);
}

void USwaziKineticLocomotion::TriggerKineticSlideState_Implementation()
{
    // Blueprint implementable — dispatches GameplayCue or animation state switch
    // to engage the Umshiza fighting-stick anchor slide pose in ABP_Mahlanya.
    UE_LOG(LogTemp, Log, TEXT("SwaziKineticLocomotion: Kinetic slide triggered — Umshiza anchor pose"));
}
