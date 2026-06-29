#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UFloraManagerComponent.generated.h"

UENUM(BlueprintType)
enum class EBiomeZoneType : uint8
{
    AfromontaneForest     UMETA(DisplayName="Afromontane Forest (Highveld ravines >1400m)"),
    SwaziThornveld        UMETA(DisplayName="Swazi Thornveld (Middleveld 600-1200m)"),
    AcaciaSavanna         UMETA(DisplayName="Acacia Savanna (Lowveld <600m)"),
    RiparianGallery       UMETA(DisplayName="Riparian Gallery Forest (river corridors)"),
    HighAltitudeGrassland UMETA(DisplayName="High-Altitude Grassland (Highveld plateau >1500m)"),
};

UCLASS(ClassGroup=("Ecology"), meta=(BlueprintSpawnableComponent))
class ECOLOGYSIMULATORPLUGIN_API UFloraManagerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBiomeZoneType BiomeZone = EBiomeZoneType::SwaziThornveld;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float VegetationDensity = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BurnCoverage = 0.f;

    UFUNCTION(BlueprintCallable, Category="Flora")
    float GetFireSpreadProbability(float WindSpeed_ms) const;

    UFUNCTION(BlueprintCallable, Category="Flora")
    static EBiomeZoneType ClassifyBiomeZone(float Altitude_m, float RiverDistance_m);
};
