#include "UOrographicRainfallComponent.h"
#include "UMicroclimateSubsystem.h"
#include "Engine/World.h"

UOrographicRainfallComponent::UOrographicRainfallComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UOrographicRainfallComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                   FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSubsystem && GetWorld())
    {
        CachedSubsystem = GetWorld()->GetSubsystem<UMicroclimateSubsystem>();
    }

    if (!CachedSubsystem) return;

    const FMicroclimateState& State = CachedSubsystem->GetCurrentState();
    // Trigger rain when upslope wind exceeds 3 m/s and delta elevation is large
    // (terrain delta queried by caller; here we check wind speed threshold)
    if (State.WindSpeed_ms > 3.f)
    {
        // In full implementation, terrain delta queried via height sample along wind direction.
        // Deferred: orographic trigger fired externally by Blueprint when terrain delta known.
    }
}

float UOrographicRainfallComponent::ComputePrecipitationProbability(
    float WindSpeed_ms, float WindBearing_deg, float TerrainDeltaElev_m) const
{
    if (TerrainDeltaElev_m < CondensationThreshold_m)
        return 0.05f;  // background probability

    const float UpliftFraction = FMath::Clamp(
        (TerrainDeltaElev_m - CondensationThreshold_m) / 1000.f, 0.f, 1.f);
    const float WindContribution = FMath::Clamp(WindSpeed_ms / 10.f, 0.f, 1.f);

    return FMath::Clamp(
        0.05f + UpliftFraction * WindContribution * MaxProbabilityMultiplier,
        0.f, 1.f);
}
