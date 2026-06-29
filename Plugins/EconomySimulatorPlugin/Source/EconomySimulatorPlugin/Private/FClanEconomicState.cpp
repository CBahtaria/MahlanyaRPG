// Copyright Charles Bartaria. All Rights Reserved.

#include "FClanEconomicState.h"
#include "Math/UnrealMathUtility.h"

float FClanEconomicState::ComputeRaidProbability() const
{
    // P(raid) = DroughtStress * (1 - PoliticalStrength) * 0.3
    // Uses the cached PoliticalStrength; callers should ensure it is fresh.
    return DroughtStress * (1.f - FMath::Clamp(PoliticalStrength, 0.f, 1.f)) * 0.3f;
}

float FClanEconomicState::ComputePoliticalStrength() const
{
    // Cattle contribution on a log scale: log(count+1) / 10, clamped 0–1.
    // log(100+1) ≈ 4.6 → 0.46, log(1000+1) ≈ 6.9 → 0.69.
    const float CattleStrength = FMath::Clamp(
        FMath::Loge(static_cast<float>(FMath::Max(CattleCount, 1)) + 1.f) / 10.f,
        0.f, 1.f);

    // Each accepted concession cedes 8% of political legitimacy.
    const float ConcessionPenalty = static_cast<float>(AcceptedConcessions.Num()) * 0.08f;

    // Colonial pressure also suppresses strength (30% weight).
    return FMath::Clamp(
        CattleStrength - ConcessionPenalty - ColonialPressure * 0.3f,
        0.f, 1.f);
}
