#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UHistoricalCalendarSubsystem.h"
#include "UImbongiGenerator.h"
#include "UEmergentNarrativeSubsystem.generated.h"

USTRUCT(BlueprintType)
struct EMERGENTNARRATIVEPLUGIN_API FQuestTriggerRule
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName QuestTemplateID;
    UPROPERTY(BlueprintReadOnly) FName ConditionID;     // matches FOnQuestTriggerConditionMet
    UPROPERTY(BlueprintReadOnly) float CooldownDays = 30.f;
    UPROPERTY(BlueprintReadOnly) bool bFired = false;
    UPROPERTY(BlueprintReadOnly) float LastFiredDay = -999.f;
};

UCLASS()
class EMERGENTNARRATIVEPLUGIN_API UEmergentNarrativeSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="Narrative")
    void AdvanceNarrativeTime(float GameDayDelta);

    UFUNCTION(BlueprintCallable, Category="Narrative")
    FString GenerateImbongiVerse(const FString& PlayerNameRoot,
                                  int32 CattleCount,
                                  int32 HuntsCompleted,
                                  const FString& EnemyName,
                                  const FString& LocationName) const;

    UFUNCTION(BlueprintCallable, Category="Narrative")
    bool HasQuestFired(FName QuestTemplateID) const;

    UFUNCTION(BlueprintCallable, Category="Narrative")
    TArray<FName> GetActiveQuests() const;

private:
    TArray<FQuestTriggerRule> TriggerRules;
    TSet<FName> ActiveQuests;
    float TotalDaysElapsed = 0.f;

    UPROPERTY()
    UImbongiGenerator* ImbongiGen = nullptr;

    void RegisterTriggerRules();

    UFUNCTION()
    void OnQuestConditionMet(FName ConditionID, float ConditionValue);
};
