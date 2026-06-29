#include "UAcousticZoneComponent.h"

UAcousticZoneComponent::UAcousticZoneComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetBoxExtent(FVector(500.f, 500.f, 300.f));
    SetCollisionResponseToAllChannels(ECR_Ignore);
    SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

float UAcousticZoneComponent::GetBlendWeight(const FVector& ListenerWorldPos) const
{
    const FBox Box = GetComponentTransform().TransformBox(
        FBox(-GetScaledBoxExtent(), GetScaledBoxExtent()));

    const FVector Closest = Box.GetClosestPointTo(ListenerWorldPos);
    const float DistToEdge = (ListenerWorldPos - Closest).Size();

    if (Box.IsInsideOrOn(ListenerWorldPos))
    {
        // Fully inside: blend based on distance from wall
        const FVector LocalPos = GetComponentTransform().InverseTransformPosition(ListenerWorldPos);
        const FVector HalfSize = GetScaledBoxExtent();
        const float MinDistToWall = FMath::Min3(
            HalfSize.X - FMath::Abs(LocalPos.X),
            HalfSize.Y - FMath::Abs(LocalPos.Y),
            HalfSize.Z - FMath::Abs(LocalPos.Z));
        return FMath::Clamp(MinDistToWall / FMath::Max(BlendRadius_cm, 1.f), 0.f, 1.f);
    }

    // Outside zone
    return 0.f;
}

FString UAcousticZoneComponent::GetArchetypeName() const
{
    static const TArray<FString> Names = {
        TEXT("GraniteCave"), TEXT("ThatchedHutInterior"), TEXT("LubomboCanyon"),
        TEXT("OpenHighveld"), TEXT("UsuthuGorge"), TEXT("RiverbedFloodplain"),
    };
    const int32 Idx = (int32)Archetype;
    return Names.IsValidIndex(Idx) ? Names[Idx] : TEXT("Unknown");
}
