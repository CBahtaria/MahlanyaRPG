#include "Stealth/StealthVisibilityComponent.h"

// ---------------------------------------------------------------------------
// NOTE: MicroclimateEngine subsystem will be wired here once implemented.
//
// #include "MicroclimateEngine/UMicroclimateSubsystem.h"
//
// Planned integration in TickComponent:
//   if (UMicroclimateSubsystem* MS = UMicroclimateSubsystem::Get(GetWorld()))
//   {
//       FogDensity = MS->SampleFogDensity(OwnerLocation);
//   }
// ---------------------------------------------------------------------------

DEFINE_LOG_CATEGORY_STATIC(LogMahlanyaStealth, Log, All);

// Minimum clamped visibility so actors are never fully invisible from all NPCs.
// Matches the 0.05 floor stated in the specification.
static constexpr float kMinVisibility = 0.05f;
static constexpr float kMaxVisibility = 1.0f;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UStealthVisibilityComponent::UStealthVisibilityComponent()
{
    // Tick required to detect owner movement and trigger conditional recalculation.
    PrimaryComponentTick.bCanEverTick = true;
    // No replication needed — each machine computes its own mask from
    // density values injected by local subsystems.
    SetIsReplicatedByDefault(false);
}

// ---------------------------------------------------------------------------
// Density Injection
// ---------------------------------------------------------------------------

void UStealthVisibilityComponent::SetFogDensityAtLocation(float Density)
{
    const float Clamped = FMath::Clamp(Density, 0.f, 1.f);
    if (!FMath::IsNearlyEqual(FogDensity, Clamped, KINDA_SMALL_NUMBER))
    {
        FogDensity = Clamped;
        // Density changed externally; force an immediate mask recompute
        // regardless of movement threshold so the value stays coherent.
        RecomputeMask();

        UE_LOG(LogMahlanyaStealth, Verbose,
            TEXT("[%s] SetFogDensityAtLocation: %.3f → mask %.3f"),
            *GetNameSafe(GetOwner()), FogDensity, CachedMask);
    }
}

void UStealthVisibilityComponent::SetSmokeDensityAtLocation(float Density)
{
    const float Clamped = FMath::Clamp(Density, 0.f, 1.f);
    if (!FMath::IsNearlyEqual(SmokeDensity, Clamped, KINDA_SMALL_NUMBER))
    {
        SmokeDensity = Clamped;
        RecomputeMask();

        UE_LOG(LogMahlanyaStealth, Verbose,
            TEXT("[%s] SetSmokeDensityAtLocation: %.3f → mask %.3f"),
            *GetNameSafe(GetOwner()), SmokeDensity, CachedMask);
    }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

float UStealthVisibilityComponent::GetVisibilityMask() const
{
    return CachedMask;
}

// ---------------------------------------------------------------------------
// UActorComponent overrides
// ---------------------------------------------------------------------------

void UStealthVisibilityComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    const FVector CurrentLocation = Owner->GetActorLocation();
    const float DistanceMoved     = FVector::Dist(CurrentLocation, LastUpdateLocation);

    // Only recompute when the owner has moved beyond the threshold.
    // This mirrors the godot-4-fog-of-war pattern: updates are event-driven
    // on displacement, not scheduled every frame.
    if (DistanceMoved >= UpdateDistanceThreshold)
    {
        LastUpdateLocation = CurrentLocation;

        // NOTE: MicroclimateEngine fog sampling will replace the manual
        // SetFogDensityAtLocation injection once the subsystem exists.
        // For now, FogDensity is updated externally by MicroclimateEngine.

        RecomputeMask();

        UE_LOG(LogMahlanyaStealth, Verbose,
            TEXT("[%s] TickComponent: moved %.1f cm — recomputed mask to %.3f "
                 "(fog=%.3f, smoke=%.3f)."),
            *GetNameSafe(Owner), DistanceMoved, CachedMask, FogDensity, SmokeDensity);
    }
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void UStealthVisibilityComponent::RecomputeMask()
{
    // Convert densities to transparency fractions:
    //   FogMask   = 1 - FogDensity    (0 fog → fully transparent → visible)
    //   SmokeMask = 1 - SmokeDensity
    // Multiply to combine both occlusion layers, then clamp to [0.05, 1.0].
    // The 0.05 floor ensures NPCs always retain a minimal detection radius
    // so gameplay remains fair even in heavy smoke or fog.
    const float FogMask   = 1.0f - FogDensity;
    const float SmokeMask = 1.0f - SmokeDensity;

    CachedMask = FMath::Clamp(FogMask * SmokeMask, kMinVisibility, kMaxVisibility);
}
