#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "USiSwatiLanguageSubsystem.generated.h"

UCLASS()
class CULTURALPROTOCOLPLUGIN_API USiSwatiLanguageSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category="siSwati Language")
    bool LoadMorphology(const FString& JsonPath);

    UFUNCTION(BlueprintCallable, Category="siSwati Language")
    FString GenerateName(const FString& NounClass = TEXT("1a"), bool bFemale = false) const;

    UFUNCTION(BlueprintCallable, Category="siSwati Language")
    FString ToPossessive(const FString& Name, const FString& NounClass) const;

    UFUNCTION(BlueprintCallable, Category="siSwati Language")
    FString ApplyInhlonipho(const FString& Word, const FString& TabooRoot,
                             const FString& Substitute) const;

    UFUNCTION(BlueprintCallable, Category="siSwati Language")
    FString GetGreeting(const FString& Context) const;

    UFUNCTION(BlueprintCallable, Category="siSwati Language")
    FString GetPraiseword(const FString& Role) const;

private:
    TMap<FString, FString> Greetings;
    TMap<FString, FString> Praisewords;
    TArray<FString> WarriorRoots;
    TMap<FString, FString> NounPrefixes;

    static FString DefaultMorphologyPath();
};
