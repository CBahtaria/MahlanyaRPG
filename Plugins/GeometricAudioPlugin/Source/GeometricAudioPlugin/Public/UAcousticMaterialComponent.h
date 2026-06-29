#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UAcousticMaterialComponent.generated.h"

/**
 * UAcousticMaterialComponent
 *
 * Attach to any static mesh actor to assign per-octave-band acoustic absorption
 * coefficients. Queried by UGeometricAudioComponent during ray-casting.
 *
 * 8 octave bands: 125Hz, 250Hz, 500Hz, 1kHz, 2kHz, 4kHz, 8kHz, 16kHz.
 *
 * Preset names match the pipeline's MATERIAL_ABSORPTION dict keys.
 */
UENUM(BlueprintType)
enum class EAcousticMaterialPreset : uint8
{
    Granite         UMETA(DisplayName = "Granite"),
    ThatchGrass     UMETA(DisplayName = "Thatch Grass"),
    ClayEarth       UMETA(DisplayName = "Clay Earth"),
    DryGrass        UMETA(DisplayName = "Dry Grass"),
    WaterSurface    UMETA(DisplayName = "Water Surface"),
    OpenSky         UMETA(DisplayName = "Open Sky"),
    Custom          UMETA(DisplayName = "Custom"),
};

UCLASS(ClassGroup=("GeometricAudio"), meta=(BlueprintSpawnableComponent))
class GEOMETRICAUDIOPLUGIN_API UAcousticMaterialComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAcousticMaterialComponent();

    /**
     * Select a material preset to auto-populate absorption coefficients.
     * Set to Custom to manually specify AbsorptionCoefficients.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustic Material")
    EAcousticMaterialPreset Preset = EAcousticMaterialPreset::Granite;

    /**
     * Per-octave-band absorption coefficients [0–1].
     * Index: 0=125Hz, 1=250Hz, 2=500Hz, 3=1kHz, 4=2kHz, 5=4kHz, 6=8kHz, 7=16kHz.
     * Auto-populated by ApplyPreset(); editable when Preset == Custom.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustic Material",
              meta=(ArraySizeEnum="8"))
    float AbsorptionCoefficients[8] = { 0.02f, 0.02f, 0.03f, 0.03f, 0.04f, 0.05f, 0.05f, 0.06f };

    /** Apply the current Preset to AbsorptionCoefficients. */
    UFUNCTION(BlueprintCallable, Category = "Acoustic Material")
    void ApplyPreset();

    /** Return the absorption coefficient for a given octave band index [0–7]. */
    UFUNCTION(BlueprintCallable, Category = "Acoustic Material")
    float GetAbsorption(int32 BandIndex) const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    static constexpr float PRESETS[6][8] = {
        { 0.02f, 0.02f, 0.03f, 0.03f, 0.04f, 0.05f, 0.05f, 0.06f }, // Granite
        { 0.15f, 0.25f, 0.40f, 0.55f, 0.65f, 0.70f, 0.72f, 0.70f }, // ThatchGrass
        { 0.35f, 0.40f, 0.45f, 0.50f, 0.55f, 0.55f, 0.60f, 0.60f }, // ClayEarth
        { 0.25f, 0.35f, 0.45f, 0.55f, 0.60f, 0.65f, 0.65f, 0.65f }, // DryGrass
        { 0.01f, 0.01f, 0.02f, 0.02f, 0.03f, 0.03f, 0.05f, 0.05f }, // WaterSurface
        { 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f }, // OpenSky
    };
};
