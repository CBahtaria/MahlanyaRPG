// Copyright Charles Bartaria. All Rights Reserved.

#include "MahlanyaScalabilitySubsystem.h"
#include "HAL/IConsoleManager.h"

void UMahlanyaScalabilitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    IConsoleManager& CM = IConsoleManager::Get();

    // Register CVars with default values matching PC Ultra tier.
    // When DefaultScalability.ini applies a scalability group the values are
    // overwritten before any simulation plugin queries them.
    CVar_RuntimeErosion  = CM.RegisterConsoleVariable(
        TEXT("mahlanya.RuntimeErosion.Enabled"), 1,
        TEXT("1 = run GPU erosion CS; 0 = use pre-baked .r16 tiles (mobile)."),
        ECVF_Scalability);

    CVar_RuntimeVoronoi  = CM.RegisterConsoleVariable(
        TEXT("mahlanya.RuntimeVoronoi.Enabled"), 1,
        TEXT("1 = recompute Voronoi on demographic change; 0 = static JSON."),
        ECVF_Scalability);

    CVar_AudioRayCount   = CM.RegisterConsoleVariable(
        TEXT("mahlanya.GeometricAudio.RayCount"), 64,
        TEXT("Rays per audio frame for GeometricAudioPlugin (0 = pre-baked IR)."),
        ECVF_Scalability);

    CVar_AudioMaxBounces = CM.RegisterConsoleVariable(
        TEXT("mahlanya.GeometricAudio.MaxBounces"), 6,
        TEXT("Max acoustic ray bounces (0 on mobile)."),
        ECVF_Scalability);

    CVar_DrawDistance    = CM.RegisterConsoleVariable(
        TEXT("mahlanya.DrawDistance.Km"), 8.0f,
        TEXT("Streaming draw distance in km (8 PC Ultra / 2 Mobile High / 1 Mobile Low)."),
        ECVF_Scalability);
}

bool UMahlanyaScalabilitySubsystem::IsRuntimeErosionEnabled() const
{
    return CVar_RuntimeErosion && CVar_RuntimeErosion->GetInt() != 0;
}

bool UMahlanyaScalabilitySubsystem::IsRuntimeVoronoiEnabled() const
{
    return CVar_RuntimeVoronoi && CVar_RuntimeVoronoi->GetInt() != 0;
}

bool UMahlanyaScalabilitySubsystem::IsGeometricAudioEnabled() const
{
    return CVar_AudioRayCount && CVar_AudioRayCount->GetInt() > 0;
}

int32 UMahlanyaScalabilitySubsystem::GetGeometricAudioRayCount() const
{
    return CVar_AudioRayCount ? CVar_AudioRayCount->GetInt() : 0;
}

int32 UMahlanyaScalabilitySubsystem::GetGeometricAudioMaxBounces() const
{
    return CVar_AudioMaxBounces ? CVar_AudioMaxBounces->GetInt() : 0;
}

float UMahlanyaScalabilitySubsystem::GetDrawDistanceKm() const
{
    return CVar_DrawDistance ? CVar_DrawDistance->GetFloat() : 8.0f;
}

bool UMahlanyaScalabilitySubsystem::IsMobileTier() const
{
    return !IsRuntimeErosionEnabled();
}
