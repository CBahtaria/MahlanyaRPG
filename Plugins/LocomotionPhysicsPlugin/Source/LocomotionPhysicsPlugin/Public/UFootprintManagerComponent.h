#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UFootprintManagerComponent.generated.h"

/** One footprint recorded in the world */
USTRUCT(BlueprintType)
struct FFootprint {
    GENERATED_BODY()
    UPROPERTY() FVector WorldLocation;
    UPROPERTY() float DepthMetres = 0.f;        // Depression depth
    UPROPERTY() float AgeSeconds  = 0.f;        // Time since stamped
    UPROPERTY() bool  bBaked      = false;       // True after sun-hardened
    UPROPERTY() float HardnessPa  = 100000.f;   // Terrain hardness at stamp time
};

/** Terrain hardness values (Pascals) by physical material name */
struct FTerrainHardnessTable {
    static float Lookup(FName MaterialName);
};

UCLASS(ClassGroup=(Mahlanya), meta=(BlueprintSpawnableComponent))
class LOCOMOTIONPHYSICSPLUGIN_API UFootprintManagerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UFootprintManagerComponent();

    /** Stamp a footprint at the given world location.
     *  MaterialName: physical material FName (e.g. PM_ClayWet).
     *  CharacterMass_kg: owner's mass; defaults to 75 kg.
     *  FootContactArea_m2: sole contact area; defaults to 0.025 m² (250 cm²). */
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Footprints")
    void StampFootprint(FVector WorldLocation, FName MaterialName,
                        float CharacterMass_kg = 75.f,
                        float FootContactArea_m2 = 0.025f);

    /** Age all active footprints — call from OwningCharacter tick or a timer.
     *  RainIntensity_mmhr: current rain (mm/hr) fills prints upward.
     *  SunIntensity: 0-1 scalar (0=night, 1=midday); bakes prints that have been dry >300s. */
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Footprints")
    void AgePrints(float DeltaTime, float RainIntensity_mmhr, float SunIntensity);

    /** Returns freshness score [0,1] of the most recently stamped print.
     *  1.0 = fresh (just stamped), 0.0 = fully erased or baked-flat. */
    UFUNCTION(BlueprintPure, Category="Mahlanya|Footprints")
    float GetMostRecentPrintFreshness() const;

    /** Read-only snapshot of active footprints for debug rendering. */
    UFUNCTION(BlueprintPure, Category="Mahlanya|Footprints")
    const TArray<FFootprint>& GetActiveFootprints() const { return ActiveFootprints; }

    /** Maximum footprints to keep in memory before oldest are discarded. */
    UPROPERTY(EditAnywhere, Category="Mahlanya|Footprints")
    int32 MaxFootprints = 200;

    /** Rain fill rate: metres of depth recovered per mm/hr per second. */
    UPROPERTY(EditAnywhere, Category="Mahlanya|Footprints")
    float RainFillRate = 0.0001f;

    /** Dry seconds before sun begins baking a print (locking its depth). */
    UPROPERTY(EditAnywhere, Category="Mahlanya|Footprints")
    float SunBakeThreshold_s = 300.f;

protected:
    UPROPERTY() TArray<FFootprint> ActiveFootprints;

private:
    /** FootprintDepth_m = CharacterMass_kg / (FootContactArea_m2 * TerrainHardness_Pa) */
    static float ComputeDepth(float Mass_kg, float Area_m2, float Hardness_Pa);
};
