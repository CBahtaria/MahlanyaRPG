#include "UMicroclimateSubsystem.h"
#include "Math/UnrealMathUtility.h"

void UMicroclimateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    PressureHistory = { 1013.25f, 1013.25f, 1013.25f };
    UE_LOG(LogTemp, Log, TEXT("UMicroclimateSubsystem initialized"));
}

void UMicroclimateSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

void UMicroclimateSubsystem::Tick(float DeltaTime)
{
    // 1 real second = 1 game minute (configurable in future)
    SimulateMinuteTick(DeltaTime / 60.f);
}

void UMicroclimateSubsystem::SimulateMinuteTick(float GameMinuteDelta)
{
    const float PrevRain = CurrentState.PrecipitationIntensity;

    // Pressure random walk (±0.5 hPa/min scaled by delta)
    CurrentState.PressureHPa += FMath::RandRange(-0.5f, 0.5f) * GameMinuteDelta;
    CurrentState.PressureHPa = FMath::Clamp(CurrentState.PressureHPa, 870.f, 1020.f);

    // Pressure tendency (difference from oldest history entry)
    CurrentState.PressureTendencyHPa = CurrentState.PressureHPa - PressureHistory[0];
    if (PressureHistory.Num() >= 3) PressureHistory.RemoveAt(0);
    PressureHistory.Add(CurrentState.PressureHPa);

    // Cloud density tracks low pressure
    const float CloudTarget = FMath::Clamp((1013.25f - CurrentState.PressureHPa) / 20.f, 0.f, 1.f);
    CurrentState.CloudDensityFraction += (CloudTarget - CurrentState.CloudDensityFraction) * 0.02f * GameMinuteDelta;

    // Rain event timer
    if (RainEventRemaining_GameMin > 0.f)
    {
        RainEventRemaining_GameMin -= GameMinuteDelta;
        if (RainEventRemaining_GameMin <= 0.f)
            CurrentState.PrecipitationIntensity = 0.f;
    }

    // Thunderstorm detection
    CurrentState.bThunderstormActive =
        (CurrentState.CloudDensityFraction > 0.7f && CurrentState.PrecipitationIntensity > 5.f);

    // Turbidity driven by cloud cover
    CurrentState.TurbidityParam = 1.5f + CurrentState.CloudDensityFraction * 4.5f;

    // Rain accumulation (drains at 2 mm/game-hour)
    RainAccumulation_mm += CurrentState.PrecipitationIntensity * GameMinuteDelta / 60.f;
    RainAccumulation_mm -= 2.f * GameMinuteDelta / 60.f;
    RainAccumulation_mm = FMath::Clamp(RainAccumulation_mm, 0.f, 200.f);

    BroadcastStateChanges(PrevRain);
}

void UMicroclimateSubsystem::BroadcastStateChanges(float PrevRain)
{
    if (FMath::Abs(CurrentState.PrecipitationIntensity - LastBroadcastRain) > 1.0f)
    {
        OnRainIntensityChanged.Broadcast(CurrentState.PrecipitationIntensity);
        LastBroadcastRain = CurrentState.PrecipitationIntensity;
    }

    const int32 Tier = FMath::Clamp((int32)(RainAccumulation_mm / 50.f), 0, 3);
    if (Tier != LastSaturationTier)
    {
        OnTerrainSaturationChanged.Broadcast(Tier / 3.f);
        LastSaturationTier = Tier;
    }
}

void UMicroclimateSubsystem::TriggerRainEvent(float Intensity_mm_per_hr,
                                               float Duration_GameMinutes)
{
    CurrentState.PrecipitationIntensity = Intensity_mm_per_hr;
    RainEventRemaining_GameMin = Duration_GameMinutes;
    CurrentState.CloudDensityFraction = FMath::Max(0.5f, CurrentState.CloudDensityFraction);
}

float UMicroclimateSubsystem::GetVolumetricDensityAt(const FVector& WorldLocation) const
{
    const float AltitudeMetres = WorldLocation.Z / 100.f;  // UE5 cm → m
    if (AltitudeMetres < CurrentState.FogBaseAltitude_m)
    {
        return 0.8f * (1.f - AltitudeMetres / FMath::Max(1.f, CurrentState.FogBaseAltitude_m));
    }
    return 0.f;
}

float UMicroclimateSubsystem::GetSmokeDensityAt(const FVector& WorldLocation) const
{
    float Total = 0.f;
    for (const TTuple<FVector, float>& Source : SmokeSources)
    {
        const float Dist = FVector::Dist(WorldLocation, Source.Key);
        Total += Source.Value * FMath::Max(0.f, 1.f - Dist / 5000.f);
    }
    return FMath::Clamp(Total, 0.f, 1.f);
}

void UMicroclimateSubsystem::RegisterSmokeSource(FVector WorldLocation,
                                                   float IntensityFraction)
{
    SmokeSources.Emplace(WorldLocation, FMath::Clamp(IntensityFraction, 0.f, 1.f));
}

void UMicroclimateSubsystem::ClearSmokeSources()
{
    SmokeSources.Empty();
}
