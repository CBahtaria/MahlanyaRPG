#include "UValleyInversionComponent.h"
#include "UMicroclimateSubsystem.h"
#include "Engine/World.h"

UValleyInversionComponent::UValleyInversionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UValleyInversionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSubsystem && GetWorld())
    {
        CachedSubsystem = GetWorld()->GetSubsystem<UMicroclimateSubsystem>();
    }

    // Determine night phase from world time (simplified: use real seconds mod 86400)
    const float DaySeconds = FMath::Fmod(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f, 86400.f);
    const float HourOfDay = DaySeconds / 3600.f;
    const bool bNight = (HourOfDay >= 20.f || HourOfDay < 6.f);

    const float DeltaHour = DeltaTime / 3600.f;
    if (bNight)
        CurrentFogHeight_m += ColdAirDrainageRate_m_per_hr * DeltaHour;
    else
        CurrentFogHeight_m -= (ColdAirDrainageRate_m_per_hr * 0.5f) * DeltaHour;

    CurrentFogHeight_m = FMath::Clamp(CurrentFogHeight_m, 0.f, 500.f);

    if (CachedSubsystem)
    {
        CachedSubsystem->SetFogBaseAltitude(UsuthuGorgeFloor_m + CurrentFogHeight_m);
    }
}

float UValleyInversionComponent::ComputeFogBaseAltitude(float TimeOfDay_24h,
                                                          float ValleyFloorAlt_m,
                                                          float InversionStrength) const
{
    float ColdAirFraction = 0.f;

    if (TimeOfDay_24h >= 20.f || TimeOfDay_24h < 6.f)
    {
        ColdAirFraction = InversionStrength * 0.8f;
    }
    else if (TimeOfDay_24h < 8.f)
    {
        // Dawn burn-off: lerp from full inversion to 0 over 2 hours
        const float T = (TimeOfDay_24h - 6.f) / 2.f;
        ColdAirFraction = FMath::Lerp(InversionStrength * 0.8f, 0.f, T);
    }

    // Fog base rises as cold air fraction increases (cold air fills valley from floor up)
    return ValleyFloorAlt_m + (1.f - ColdAirFraction) * 500.f;
}
