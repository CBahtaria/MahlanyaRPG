#include "UGeometricAudioComponent.h"
#include "UAcousticMaterialComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

static constexpr int32 NUM_OCTAVE_BANDS = 8;
// Centre frequencies: 125, 250, 500, 1k, 2k, 4k, 8k, 16kHz
static constexpr float OCTAVE_FREQS[NUM_OCTAVE_BANDS] = {
    125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f
};

UGeometricAudioComponent::UGeometricAudioComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

TArray<FRayAcousticHit> UGeometricAudioComponent::ComputeRuntimeIR(
    const FVector& SourceLocation, const FVector& ListenerLocation)
{
    return TraceAcousticRays(SourceLocation, ListenerLocation);
}

TArray<FRayAcousticHit> UGeometricAudioComponent::TraceAcousticRays(
    const FVector& Origin, const FVector& Target)
{
    TArray<FRayAcousticHit> Results;
    if (!GetWorld()) return Results;

    const float EffectiveSpeed_cm_s = FAtmosphericAcoustics::EffectiveSoundSpeed(
        (Target - Origin).GetSafeNormal(),
        AtmosphericState.WindVelocity * 100.f,   // m/s → cm/s
        AtmosphericState.AirTemperature_C) * 100.f;

    // Hemisphere of rays (Fibonacci lattice for uniform distribution)
    const float GoldenAngle = 2.399963f;  // radians

    for (int32 RayIdx = 0; RayIdx < NumRays; ++RayIdx)
    {
        const float t = (float)RayIdx / (float)NumRays;
        const float Inc = FMath::Acos(1.f - 2.f * t);
        const float Azimuth = GoldenAngle * RayIdx;
        const FVector RayDir(
            FMath::Sin(Inc) * FMath::Cos(Azimuth),
            FMath::Sin(Inc) * FMath::Sin(Azimuth),
            FMath::Cos(Inc));

        FVector RayOrigin = Origin;
        FVector RayDirection = RayDir;
        float TotalDist_cm = 0.f;
        TArray<TArray<float>> AbsorptionSequence;
        bool bHitListener = false;

        for (int32 Bounce = 0; Bounce < MaxBounces; ++Bounce)
        {
            const FVector TraceEnd = RayOrigin + RayDirection * 100000.f;  // 1km
            FHitResult Hit;

            if (!GetWorld()->LineTraceSingleByChannel(Hit, RayOrigin, TraceEnd,
                                                      ECC_WorldStatic))
                break;

            const float SegmentDist = Hit.Distance;
            TotalDist_cm += SegmentDist;

            // Check if ray passed near listener during this segment
            const FVector ClosestPoint = FMath::ClosestPointOnSegment(
                Target, RayOrigin, Hit.Location);
            if (FVector::Dist(ClosestPoint, Target) < CaptureRadius_cm)
            {
                // Build hit record from accumulated absorptions
                FRayAcousticHit Record = BuildHitRecord(
                    TotalDist_cm / 100.f,   // cm → m
                    AbsorptionSequence,
                    EffectiveSpeed_cm_s / 100.f);  // cm/s → m/s
                Results.Add(Record);
                bHitListener = true;
                break;
            }

            // Accumulate surface absorption
            if (Hit.GetActor())
            {
                AbsorptionSequence.Add(GetHitAbsorption(Hit.GetActor()));
            }

            // Reflect ray direction off surface normal
            RayDirection = FMath::GetReflectionVector(RayDirection, Hit.Normal).GetSafeNormal();
            RayOrigin = Hit.Location + RayDirection * 0.5f;  // offset to avoid self-intersection
        }
    }

    // Sort by delay
    Results.Sort([](const FRayAcousticHit& A, const FRayAcousticHit& B)
    {
        return A.DelaySeconds < B.DelaySeconds;
    });

    return Results;
}

TArray<float> UGeometricAudioComponent::GetHitAbsorption(const AActor* HitActor)
{
    TArray<float> Absorption;
    Absorption.SetNumZeroed(NUM_OCTAVE_BANDS);

    if (!HitActor) return Absorption;

    const UAcousticMaterialComponent* AcousticMat =
        HitActor->FindComponentByClass<UAcousticMaterialComponent>();

    if (AcousticMat)
    {
        for (int32 b = 0; b < NUM_OCTAVE_BANDS; ++b)
            Absorption[b] = AcousticMat->GetAbsorption(b);
    }
    else
    {
        // Default: granite-like reflective surface
        for (int32 b = 0; b < NUM_OCTAVE_BANDS; ++b)
            Absorption[b] = 0.03f;
    }

    return Absorption;
}

FRayAcousticHit UGeometricAudioComponent::BuildHitRecord(
    float PathLength_m,
    const TArray<TArray<float>>& AbsorptionSequence,
    float EffectiveSpeedMs)
{
    FRayAcousticHit Record;
    Record.Distance_m = PathLength_m;
    Record.DelaySeconds = PathLength_m / FMath::Max(EffectiveSpeedMs, 1.f);
    Record.EnergyPerBand.SetNum(NUM_OCTAVE_BANDS);

    for (int32 b = 0; b < NUM_OCTAVE_BANDS; ++b)
    {
        float Energy = 1.f / FMath::Max(PathLength_m * PathLength_m, 0.01f);

        // Apply surface absorption at each bounce
        for (const TArray<float>& BandAbsorption : AbsorptionSequence)
        {
            if (BandAbsorption.IsValidIndex(b))
                Energy *= (1.f - BandAbsorption[b]);
        }

        // Atmospheric HF absorption
        const float AtmAbsorption_dB_per_m = FAtmosphericAcoustics::AtmosphericAbsorption_dB_per_m(
            OCTAVE_FREQS[b], AtmosphericState.RelativeHumidity);
        const float AtmLinear = FMath::Pow(10.f, -AtmAbsorption_dB_per_m * PathLength_m / 20.f);
        Energy *= AtmLinear;

        Record.EnergyPerBand[b] = FMath::Max(Energy, 0.f);
    }

    return Record;
}
