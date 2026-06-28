#include "UmshizaStickProjectile.h"

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

AUmshizaStickProjectile::AUmshizaStickProjectile()
{
    PrimaryActorTick.bCanEverTick = true;

    // Root: static mesh (the visual stick)
    StickMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StickMesh"));
    SetRootComponent(StickMesh);

    // Spline component owned by this actor; rebuilt per-throw
    TrajectorySpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrajectorySpline"));
    TrajectorySpline->SetupAttachment(RootComponent);
    // The spline is used purely as a path container; it should not be visible in game
    TrajectorySpline->SetVisibility(false, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginPlay
// ─────────────────────────────────────────────────────────────────────────────

void AUmshizaStickProjectile::BeginPlay()
{
    Super::BeginPlay();

    // Begin with physics disabled; movement is driven by TickFlight
    if (StickMesh)
    {
        StickMesh->SetSimulatePhysics(false);
        StickMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────

void AUmshizaStickProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Advance flight when airborne (outbound or returning)
    if (bLaunched && !bEmbedded)
    {
        TickFlight(DeltaTime);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Launch
// ─────────────────────────────────────────────────────────────────────────────

void AUmshizaStickProjectile::Launch(FVector AimPoint, float StrikeForce)
{
    // Reset any prior flight state before starting a new throw
    ResetFlightState();

    const FVector Start = GetActorLocation();
    const float   ArcHeight = FVector::Dist(Start, AimPoint) * 0.25f;

    BuildTrajectorySpline(Start, AimPoint, ArcHeight);

    TravelSpeed = 600.f * FMath::Max(StrikeForce, 0.01f);
    bLaunched   = true;
    bReturning  = false;
    bEmbedded   = false;
    TravelAlpha = 0.f;

    UE_LOG(LogTemp, Verbose, TEXT("AUmshizaStickProjectile: Launched toward %s (ArcHeight=%.1f, Speed=%.1f)"),
        *AimPoint.ToString(), ArcHeight, TravelSpeed);
}

// ─────────────────────────────────────────────────────────────────────────────
// BeginReturn
// ─────────────────────────────────────────────────────────────────────────────

void AUmshizaStickProjectile::BeginReturn()
{
    if (!bLaunched)
    {
        UE_LOG(LogTemp, Warning, TEXT("AUmshizaStickProjectile: BeginReturn called but stick was never launched."));
        return;
    }

    // Reverse the spline by flipping its control points so TickFlight can
    // continue using the same [0→1] traversal direction toward the owner.
    const int32 NumPoints = TrajectorySpline->GetNumberOfSplinePoints();
    if (NumPoints >= 2)
    {
        // Collect current positions and tangents in world space
        TArray<FVector> Positions;
        TArray<FVector> Tangents;
        Positions.Reserve(NumPoints);
        Tangents.Reserve(NumPoints);

        for (int32 i = 0; i < NumPoints; ++i)
        {
            Positions.Add(TrajectorySpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World));
            Tangents.Add(TrajectorySpline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World));
        }

        // Rebuild spline with reversed point order (and negated tangents)
        TrajectorySpline->ClearSplinePoints(false);
        for (int32 i = NumPoints - 1; i >= 0; --i)
        {
            TrajectorySpline->AddSplineWorldPoint(Positions[i]);
        }
        for (int32 i = 0; i < NumPoints; ++i)
        {
            // Negate tangents to preserve the curve shape in reverse
            TrajectorySpline->SetTangentAtSplinePoint(
                i,
                -Tangents[(NumPoints - 1) - i],
                ESplineCoordinateSpace::World,
                false);
        }
        TrajectorySpline->UpdateSpline();
    }

    // Reset alpha so we travel from stick's current position back to the owner
    TravelAlpha = 0.f;
    bReturning  = true;
    bEmbedded   = false;
    bLaunched   = true;  // keep ticking

    UE_LOG(LogTemp, Verbose, TEXT("AUmshizaStickProjectile: BeginReturn — reversing trajectory."));
}

// ─────────────────────────────────────────────────────────────────────────────
// NotifyHit
// ─────────────────────────────────────────────────────────────────────────────

void AUmshizaStickProjectile::NotifyHit(
    UPrimitiveComponent* MyComp,
    AActor*              Other,
    UPrimitiveComponent* OtherComp,
    bool                 bSelfMoved,
    FVector              HitLocation,
    FVector              HitNormal,
    FVector              NormalImpulse,
    const FHitResult&    Hit)
{
    Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

    // Ignore hits during the return flight — the stick should pass through on its way back
    if (bReturning)
    {
        return;
    }

    // Embed the stick into the surface
    bEmbedded   = true;
    EmbedNormal = HitNormal;

    // Snap to the exact hit point and align with the surface normal
    SetActorLocation(HitLocation);
    SetActorRotation(HitNormal.Rotation());

    UE_LOG(LogTemp, Verbose, TEXT("AUmshizaStickProjectile: Embedded at %s (normal=%s)"),
        *HitLocation.ToString(), *HitNormal.ToString());
}

// ─────────────────────────────────────────────────────────────────────────────
// BuildTrajectorySpline (private)
// ─────────────────────────────────────────────────────────────────────────────

void AUmshizaStickProjectile::BuildTrajectorySpline(FVector Start, FVector End, float ArcHeight)
{
    TrajectorySpline->ClearSplinePoints(false);

    // Three-point parabola: Start → Apex → End
    const FVector Midpoint = (Start + End) * 0.5f;
    const FVector Apex     = Midpoint + FVector(0.f, 0.f, ArcHeight);

    TrajectorySpline->AddSplineWorldPoint(Start);
    TrajectorySpline->AddSplineWorldPoint(Apex);
    TrajectorySpline->AddSplineWorldPoint(End);

    // Use Curve type for smooth arc; set tangents to approximate a ballistic arc
    for (int32 i = 0; i < TrajectorySpline->GetNumberOfSplinePoints(); ++i)
    {
        TrajectorySpline->SetSplinePointType(i, ESplinePointType::Curve, false);
    }

    TrajectorySpline->UpdateSpline();
}

// ─────────────────────────────────────────────────────────────────────────────
// TickFlight (private)
// ─────────────────────────────────────────────────────────────────────────────

void AUmshizaStickProjectile::TickFlight(float DeltaTime)
{
    const float SplineLength = TrajectorySpline->GetSplineLength();
    if (SplineLength < KINDA_SMALL_NUMBER)
    {
        return;
    }

    // Advance normalised alpha proportional to distance travelled this frame
    const float DistanceDelta = TravelSpeed * DeltaTime;
    TravelAlpha += DistanceDelta / SplineLength;
    TravelAlpha  = FMath::Min(TravelAlpha, 1.f);

    // Sample position and tangent from the spline
    const float    SplineDist     = TravelAlpha * SplineLength;
    const FVector  NewLocation    = TrajectorySpline->GetLocationAtDistanceAlongSpline(SplineDist, ESplineCoordinateSpace::World);
    const FVector  TangentDir     = TrajectorySpline->GetTangentAtDistanceAlongSpline(SplineDist, ESplineCoordinateSpace::World).GetSafeNormal();

    SetActorLocation(NewLocation, true);  // sweep=true for hit detection in flight

    // Orient the stick along its direction of travel
    if (!TangentDir.IsNearlyZero())
    {
        SetActorRotation(TangentDir.Rotation());
    }

    // Check for arrival at the end of the spline
    if (TravelAlpha >= 1.f)
    {
        if (bReturning)
        {
            // Snap to owner hand and clean up
            AttachToOwnerHand();
        }
        else
        {
            // Outbound arc complete without collision — embed at endpoint
            bEmbedded = true;
            EmbedNormal = FVector::UpVector;

            UE_LOG(LogTemp, Verbose, TEXT("AUmshizaStickProjectile: Reached arc end without collision — embedded."));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AttachToOwnerHand (private)
// ─────────────────────────────────────────────────────────────────────────────

void AUmshizaStickProjectile::AttachToOwnerHand()
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("AUmshizaStickProjectile: AttachToOwnerHand — no owner actor set."));
        ResetFlightState();
        return;
    }

    // Locate the skeletal mesh component on the owner
    USkeletalMeshComponent* OwnerMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
    if (!OwnerMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("AUmshizaStickProjectile: AttachToOwnerHand — owner has no USkeletalMeshComponent."));
        ResetFlightState();
        return;
    }

    // Snap and attach to the specified hand socket
    FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);
    AttachToComponent(OwnerMesh, Rules, OwnerHandSocket);

    UE_LOG(LogTemp, Verbose, TEXT("AUmshizaStickProjectile: Attached to owner socket '%s'."), *OwnerHandSocket.ToString());

    ResetFlightState();
}

// ─────────────────────────────────────────────────────────────────────────────
// ResetFlightState (private)
// ─────────────────────────────────────────────────────────────────────────────

void AUmshizaStickProjectile::ResetFlightState()
{
    bLaunched   = false;
    bReturning  = false;
    bEmbedded   = false;
    TravelAlpha = 0.f;
    EmbedNormal = FVector::UpVector;
}
