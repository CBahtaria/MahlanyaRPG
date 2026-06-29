#include "UAcousticMaterialComponent.h"

UAcousticMaterialComponent::UAcousticMaterialComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ApplyPreset();
}

void UAcousticMaterialComponent::ApplyPreset()
{
    if (Preset == EAcousticMaterialPreset::Custom) return;
    const int32 Idx = (int32)Preset;
    if (Idx >= 0 && Idx < 6)
    {
        for (int32 b = 0; b < 8; ++b)
            AbsorptionCoefficients[b] = PRESETS[Idx][b];
    }
}

float UAcousticMaterialComponent::GetAbsorption(int32 BandIndex) const
{
    if (BandIndex < 0 || BandIndex >= 8) return 0.f;
    return AbsorptionCoefficients[BandIndex];
}

#if WITH_EDITOR
void UAcousticMaterialComponent::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    ApplyPreset();
}
#endif

// Static member definition
constexpr float UAcousticMaterialComponent::PRESETS[6][8];
