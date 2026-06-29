#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "UAcousticZoneComponent.generated.h"

/**
 * UAcousticZoneComponent
 *
 * A box volume that defines an acoustic archetype zone.
 * When the player enters, UGeometricAudioComponent selects the pre-baked
 * impulse response corresponding to this archetype for convolution reverb.
 *
 * Archetypes map to IR_<Archetype>.wav files in Game/Audio/ImpulseResponses/.
 */
UENUM(BlueprintType)
enum class EAcousticArchetype : uint8
{
    GraniteCave          UMETA(DisplayName = "Granite Cave (RT60 ~4.2s)"),
    ThatchedHutInterior  UMETA(DisplayName = "Thatched Hut Interior (RT60 ~0.15s)"),
    LubomboCanyon        UMETA(DisplayName = "Lubombo Canyon (multi-echo)"),
    OpenHighveld         UMETA(DisplayName = "Open Highveld (near-anechoic)"),
    UsuthuGorge          UMETA(DisplayName = "Usuthu Gorge (flutter echo)"),
    RiverbedFloodplain   UMETA(DisplayName = "Riverbed Floodplain (RT60 ~0.25s)"),
};

UCLASS(ClassGroup=("GeometricAudio"), meta=(BlueprintSpawnableComponent))
class GEOMETRICAUDIOPLUGIN_API UAcousticZoneComponent : public UBoxComponent
{
    GENERATED_BODY()

public:
    UAcousticZoneComponent();

    /** Acoustic archetype to use when the listener is inside this volume. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustic Zone")
    EAcousticArchetype Archetype = EAcousticArchetype::OpenHighveld;

    /**
     * Blend radius [cm]: range over which this zone fades in/out.
     * Prevents hard IR switching at zone boundaries.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Acoustic Zone",
              meta=(ClampMin="0.0"))
    float BlendRadius_cm = 500.f;

    /**
     * Blend weight at the listener's position.
     * 0 = outside zone, 1 = fully inside.
     */
    UFUNCTION(BlueprintCallable, Category = "Acoustic Zone")
    float GetBlendWeight(const FVector& ListenerWorldPos) const;

    /** Display name of the current archetype. */
    UFUNCTION(BlueprintCallable, Category = "Acoustic Zone")
    FString GetArchetypeName() const;
};
