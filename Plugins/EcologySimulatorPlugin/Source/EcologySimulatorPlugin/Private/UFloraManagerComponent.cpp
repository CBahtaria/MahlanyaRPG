#include "UFloraManagerComponent.h"

float UFloraManagerComponent::GetFireSpreadProbability(float WindSpeed_ms) const
{
    float Base = 0.f;
    switch (BiomeZone)
    {
        case EBiomeZoneType::HighAltitudeGrassland: Base = 0.7f; break;
        case EBiomeZoneType::AcaciaSavanna:         Base = 0.5f; break;
        case EBiomeZoneType::SwaziThornveld:        Base = 0.3f; break;
        case EBiomeZoneType::AfromontaneForest:     Base = 0.05f; break;
        case EBiomeZoneType::RiparianGallery:       Base = 0.02f; break;
    }
    return FMath::Clamp(Base * (1.f - BurnCoverage) * (1.f + WindSpeed_ms * 0.1f), 0.f, 1.f);
}

EBiomeZoneType UFloraManagerComponent::ClassifyBiomeZone(float Altitude_m, float RiverDistance_m)
{
    if (RiverDistance_m < 200.f)          return EBiomeZoneType::RiparianGallery;
    if (Altitude_m > 1500.f)              return EBiomeZoneType::HighAltitudeGrassland;
    if (Altitude_m > 1400.f)              return EBiomeZoneType::AfromontaneForest;
    if (Altitude_m > 600.f)               return EBiomeZoneType::SwaziThornveld;
    return EBiomeZoneType::AcaciaSavanna;
}
