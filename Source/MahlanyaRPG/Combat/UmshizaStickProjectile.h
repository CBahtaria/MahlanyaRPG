#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "UmshizaStickProjectile.generated.h"

/**
 * AUmshizaStickProjectile
 *
 * The Umshiza is a traditional Swazi fighting stick thrown in a parabolic arc
 * and recalled to the owner's hand on command.
 *
 * Pattern: GodofWar-AxeThrow — spline-based projectile arc with return-to-hand tracking.
 *
 * Usage:
 *   1. Call Launch(AimPoint) to send the stick along a parabolic spline arc.
 *   2. Call BeginReturn() at any time to reverse the spline and fly back.
 *   3. On arrival, the stick attaches itself to OwnerHandSocket on the owner's mesh.
 */
UCLASS()
class AUmshizaStickProjectile : public AActor
{
    GENERATED_BODY()

public:
    AUmshizaStickProjectile();

    // ── Primary interface ─────────────────────────────────────────────────────

    /**
     * Launch the stick along a parabolic spline arc toward AimPoint.
     *
     * @param AimPoint     World-space target location.
     * @param StrikeForce  Scalar applied to TravelSpeed (1.0 = default 600 cm/s).
     */
    UFUNCTION(BlueprintCallable, Category = "Umshiza|Combat")
    void Launch(FVector AimPoint, float StrikeForce = 1.0f);

    /**
     * Begin the return flight: reverses the trajectory spline so the stick
     * flies back to OwnerHandSocket.
     */
    UFUNCTION(BlueprintCallable, Category = "Umshiza|Combat")
    void BeginReturn();

    // ── Replicated state ──────────────────────────────────────────────────────

    /** True while the stick is flying back to the owner's hand. */
    UPROPERTY(BlueprintReadOnly, Category = "Umshiza|State")
    bool bReturning = false;

    /** Surface normal at the point where the stick embedded (set on collision). */
    UPROPERTY(BlueprintReadOnly, Category = "Umshiza|State")
    FVector EmbedNormal = FVector::UpVector;

    /** Socket name on the owner's skeletal mesh where the stick snaps on return. */
    UPROPERTY(EditAnywhere, Category = "Umshiza|Config")
    FName OwnerHandSocket = FName("hand_r_socket");

protected:
    // ── Actor overrides ───────────────────────────────────────────────────────

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /**
     * Collision entry point: embeds stick and records hit normal when not returning.
     */
    virtual void NotifyHit(
        UPrimitiveComponent* MyComp,
        AActor*              Other,
        UPrimitiveComponent* OtherComp,
        bool                 bSelfMoved,
        FVector              HitLocation,
        FVector              HitNormal,
        FVector              NormalImpulse,
        const FHitResult&    Hit) override;

    // ── Components ────────────────────────────────────────────────────────────

    /** Spline that defines the parabolic flight path (rebuilt on each Launch). */
    UPROPERTY(VisibleAnywhere, Category = "Umshiza|Components")
    USplineComponent* TrajectorySpline;

    /** Visual representation of the fighting stick. */
    UPROPERTY(VisibleAnywhere, Category = "Umshiza|Components")
    UStaticMeshComponent* StickMesh;

private:
    // ── Internal flight state ─────────────────────────────────────────────────

    /** Normalised progress [0, 1] along TrajectorySpline. */
    float TravelAlpha = 0.f;

    /** Movement speed in cm/s along the spline arc. */
    float TravelSpeed = 600.f;

    /** True after the stick has hit a surface and stopped mid-air. */
    bool bEmbedded = false;

    /** True after Launch() has been called; gates TickFlight. */
    bool bLaunched = false;

    // ── Internal helpers ──────────────────────────────────────────────────────

    /**
     * Construct a three-point parabolic spline from Start to End with the
     * apex offset upward by ArcHeight at the midpoint.
     */
    void BuildTrajectorySpline(FVector Start, FVector End, float ArcHeight);

    /**
     * Advance TravelAlpha along the spline by DeltaTime * TravelSpeed and
     * move the actor to the resulting world position.
     * Handles arrival logic (attachment on return, reset on embed).
     */
    void TickFlight(float DeltaTime);

    /**
     * Attach this actor to the owner's OwnerHandSocket and reset all flight
     * state so the owner can throw again.
     */
    void AttachToOwnerHand();

    /** Full reset of flight state variables. */
    void ResetFlightState();
};
