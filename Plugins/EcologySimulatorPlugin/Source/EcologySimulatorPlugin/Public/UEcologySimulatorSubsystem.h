#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FSpeciesPopulation.h"
#include "UEcologySimulatorSubsystem.generated.h"

UCLASS()
class ECOLOGYSIMULATORPLUGIN_API UEcologySimulatorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category="Ecology")
    void SimulateEcologyTick(float GameDayDelta);

    UFUNCTION(BlueprintCallable, Category="Ecology")
    void RegisterSpecies(const FSpeciesPopulation& Species);

    UFUNCTION(BlueprintCallable, Category="Ecology")
    FSpeciesPopulation GetSpeciesState(FName SpeciesID) const;

    UFUNCTION(BlueprintCallable, Category="Ecology")
    TArray<FName> GetAllSpeciesIDs() const;

    UFUNCTION(BlueprintCallable, Category="Ecology")
    float GetPopulationFraction(FName SpeciesID) const;

    UFUNCTION(BlueprintCallable, Category="Ecology")
    void ApplyDroughtStress(float DroughtSeverity);

private:
    TMap<FName, FSpeciesPopulation> SpeciesMap;
    float AccumulatedDays = 0.f;
    static constexpr float WEEK_DAYS = 7.f;
    static constexpr float CRITICAL_FRACTION = 0.15f;

    void SeedDefaultSpecies();
    void ProcessWeeklyEcology();
    void StepLotkaVolterra(FSpeciesPopulation& Prey, FSpeciesPopulation& Predator, float DeltaYears);
    void CheckPopulationTriggers();

    void AddPair(FName PredID, FString PredName, float PR, float PD, float PA, float PB, float PPop,
                 FName PreyID, FString PreyName, float YR, float YA, float YK, float YPop);
};
