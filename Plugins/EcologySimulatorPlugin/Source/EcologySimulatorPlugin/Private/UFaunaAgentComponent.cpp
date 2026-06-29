#include "UFaunaAgentComponent.h"

void UFaunaAgentComponent::UpdateMigration(float GameDayOfYear, const FVector& WaterSourceLocation)
{
    if (!bIsMigratory || !GetOwner()) return;

    // Dry season: days 120–240 (southern hemisphere winter, May–August)
    const bool bDrySeason = (GameDayOfYear >= 120.f && GameDayOfYear <= 240.f);
    if (!bDrySeason) return;

    // Interpolate toward water source over 30 days from day 120
    const float t = FMath::Clamp((GameDayOfYear - 120.f) / 30.f, 0.f, 1.f);
    const FVector CurrentLoc = GetOwner()->GetActorLocation();
    const FVector NewLoc = FMath::Lerp(CurrentLoc, WaterSourceLocation, t * 0.01f);
    GetOwner()->SetActorLocation(NewLoc);
}
