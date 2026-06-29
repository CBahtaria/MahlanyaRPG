#include "UImbongiGenerator.h"

FString UImbongiGenerator::SelectPraiseword(int32 CattleCount) const
{
    if (CattleCount >= 200)
    {
        return TEXT("Ndlovu");
    }
    else if (CattleCount >= 100)
    {
        return TEXT("Ingwe");
    }
    else if (CattleCount >= 50)
    {
        return TEXT("Wethu");
    }
    return TEXT("Mahlanya");
}

FString UImbongiGenerator::SelectHonorificSuffix(int32 HuntsCompleted) const
{
    if (HuntsCompleted >= 10)
    {
        return TEXT("-weNkosi");
    }
    else if (HuntsCompleted >= 5)
    {
        return TEXT("-waMasoja");
    }
    return TEXT("");
}

FString UImbongiGenerator::BuildGloss(const FString& Verse, const FString& PlayerNameRoot,
                                       int32 CattleCount, const FString& EnemyName,
                                       const FString& LocationName) const
{
    const FString Praiseword = SelectPraiseword(CattleCount);
    int32 HuntsCompleted = 0;

    // Derive HuntsCompleted tier from the suffix present in the verse to reconstruct
    // achievement english without passing HuntsCompleted again.
    FString AchievementEnglish;
    if (Verse.Contains(TEXT("-weNkosi")))
    {
        HuntsCompleted = 10;
        AchievementEnglish = TEXT("slew the beasts of the King");
    }
    else if (Verse.Contains(TEXT("-waMasoja")))
    {
        HuntsCompleted = 5;
        AchievementEnglish = TEXT("stood as a warrior of the household");
    }
    else
    {
        AchievementEnglish = TEXT("paid tribute in cattle");
    }

    FString PraisewordMeaning;
    if (Praiseword == TEXT("Ndlovu"))
    {
        PraisewordMeaning = TEXT("Great Elephant, lord of strength");
    }
    else if (Praiseword == TEXT("Ingwe"))
    {
        PraisewordMeaning = TEXT("Leopard, swift and fearless");
    }
    else if (Praiseword == TEXT("Wethu"))
    {
        PraisewordMeaning = TEXT("Ours, beloved of the people");
    }
    else
    {
        PraisewordMeaning = TEXT("Bold One, daring beyond measure");
    }

    return FString::Printf(
        TEXT("%s means %s. You entered %s, struck down %s, and %s."),
        *Praiseword,
        *PraisewordMeaning,
        *LocationName,
        *EnemyName,
        *AchievementEnglish
    );
}

FString UImbongiGenerator::GenerateVerse(
    const FString& PlayerNameRoot,
    int32 CattleCount,
    int32 HuntsCompleted,
    const FString& HistoricalEnemyName,
    const FString& LocationName) const
{
    const FString Praiseword = SelectPraiseword(CattleCount);
    const FString Suffix = SelectHonorificSuffix(HuntsCompleted);

    FString AchievementPhrase;
    if (HuntsCompleted >= 10)
    {
        AchievementPhrase = TEXT("Wabulala izilwane zeNkosi");
    }
    else if (HuntsCompleted >= 5)
    {
        AchievementPhrase = TEXT("Wena weMasoja ekhaya");
    }
    else
    {
        AchievementPhrase = TEXT("Wena okhokha izinkomo");
    }

    const FString Verse = FString::Printf(
        TEXT("%s %s%s!\nNgena %s wakushaya %s,\n%s."),
        *Praiseword,
        *PlayerNameRoot,
        *Suffix,
        *LocationName,
        *HistoricalEnemyName,
        *AchievementPhrase
    );

    LastGloss = BuildGloss(Verse, PlayerNameRoot, CattleCount, HistoricalEnemyName, LocationName);

    return Verse;
}
