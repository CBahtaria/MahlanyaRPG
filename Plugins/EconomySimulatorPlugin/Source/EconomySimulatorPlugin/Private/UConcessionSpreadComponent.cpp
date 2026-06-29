// Copyright Charles Bartaria. All Rights Reserved.

#include "UConcessionSpreadComponent.h"
#include "Math/UnrealMathUtility.h"

float UConcessionSpreadComponent::ComputeSpreadPressure(
    const FClanEconomicState& TargetClan,
    const TArray<FClanEconomicState>& Neighbours) const
{
    float ExternalPressure = 0.f;

    for (const FClanEconomicState& Neighbour : Neighbours)
    {
        // Any concession accepted by a neighbour creates legal precedent that
        // raises pressure on the target clan regardless of concession count.
        if (Neighbour.AcceptedConcessions.Num() > 0)
        {
            ExternalPressure += SpreadRate;
        }
    }

    return FMath::Clamp(TargetClan.ColonialPressure + ExternalPressure, 0.f, 1.f);
}
