#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UImbongiGenerator.generated.h"

// Procedural Imbongi (praise poet) verse generator
UCLASS(BlueprintType, Blueprintable)
class EMERGENTNARRATIVEPLUGIN_API UImbongiGenerator : public UObject
{
    GENERATED_BODY()

public:
    // Generate a praise verse for the player.
    // PlayerNameRoot: siSwati root of player's name (e.g., "Hlanya" from "Mahlanya")
    // CattleCount: player's current cattle wealth
    // HuntsCompleted: count of hunts
    // HistoricalEnemyName: name of recent historical adversary (from knowledge graph)
    // LocationName: name of the place the deed was done
    UFUNCTION(BlueprintCallable, Category="Imbongi")
    FString GenerateVerse(
        const FString& PlayerNameRoot,
        int32 CattleCount,
        int32 HuntsCompleted,
        const FString& HistoricalEnemyName,
        const FString& LocationName) const;

    // Returns an English gloss of the most recently generated verse.
    UFUNCTION(BlueprintCallable, Category="Imbongi")
    FString GetLastVerseGloss() const { return LastGloss; }

private:
    mutable FString LastGloss;

    FString SelectPraiseword(int32 CattleCount) const;
    FString SelectHonorificSuffix(int32 HuntsCompleted) const;
    FString BuildGloss(const FString& Verse, const FString& PlayerNameRoot,
                        int32 CattleCount, const FString& EnemyName,
                        const FString& LocationName) const;
};
