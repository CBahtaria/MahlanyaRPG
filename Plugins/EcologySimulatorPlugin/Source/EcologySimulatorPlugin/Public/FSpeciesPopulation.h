#pragma once
#include "CoreMinimal.h"
#include "FSpeciesPopulation.generated.h"

USTRUCT(BlueprintType)
struct ECOLOGYSIMULATORPLUGIN_API FSpeciesPopulation
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName SpeciesID;
    UPROPERTY(BlueprintReadOnly) FString DisplayName;
    UPROPERTY(BlueprintReadOnly) float Population = 0.f;
    UPROPERTY(BlueprintReadOnly) float InitialPopulation = 0.f;
    UPROPERTY(BlueprintReadOnly) float BirthRate = 0.f;        // r
    UPROPERTY(BlueprintReadOnly) float DeathRate = 0.f;        // delta (predator)
    UPROPERTY(BlueprintReadOnly) float LV_Alpha = 0.f;         // predation rate
    UPROPERTY(BlueprintReadOnly) float LV_Beta = 0.f;          // predator efficiency
    UPROPERTY(BlueprintReadOnly) float CarryingCapacity = 0.f; // K (prey only)
    UPROPERTY(BlueprintReadOnly) bool bIsPredator = false;
    UPROPERTY(BlueprintReadOnly) FName PreySpeciesID;
    UPROPERTY(BlueprintReadOnly) TArray<FName> BiomeZones;
};
