#include "USiSwatiLanguageSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void USiSwatiLanguageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadMorphology(DefaultMorphologyPath());
}

FString USiSwatiLanguageSubsystem::DefaultMorphologyPath()
{
    return FPaths::ProjectPluginsDir()
        / TEXT("CulturalProtocolPlugin/Source/CulturalProtocolPlugin/Data/siswati_morphology.json");
}

bool USiSwatiLanguageSubsystem::LoadMorphology(const FString& JsonPath)
{
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *JsonPath)) return false;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;

    // Greeting phrases
    Greetings.Empty();
    const TSharedPtr<FJsonObject>* GreetObj;
    if (Root->TryGetObjectField(TEXT("greeting_phrases"), GreetObj))
        for (const auto& KV : (*GreetObj)->Values)
        {
            const TSharedPtr<FJsonObject>& Entry = KV.Value->AsObject();
            if (Entry.IsValid())
                Greetings.Add(KV.Key, Entry->GetStringField(TEXT("gloss")));
        }

    // Praisewords
    Praisewords.Empty();
    const TSharedPtr<FJsonObject>* PraisObj;
    if (Root->TryGetObjectField(TEXT("praisewords"), PraisObj))
        for (const auto& KV : (*PraisObj)->Values)
            Praisewords.Add(KV.Key, KV.Value->AsString());

    // Warrior roots
    WarriorRoots.Empty();
    const TSharedPtr<FJsonObject>* NameRoots;
    if (Root->TryGetObjectField(TEXT("name_roots"), NameRoots))
    {
        const TArray<TSharedPtr<FJsonValue>>* Roots;
        if ((*NameRoots)->TryGetArrayField(TEXT("warrior_roots"), Roots))
            for (const auto& R : *Roots) WarriorRoots.Add(R->AsString());
    }

    // Noun class prefixes
    NounPrefixes.Empty();
    const TArray<TSharedPtr<FJsonValue>>* NounClasses;
    if (Root->TryGetArrayField(TEXT("noun_classes"), NounClasses))
        for (const auto& NC : *NounClasses)
        {
            const TSharedPtr<FJsonObject>& NCObj = NC->AsObject();
            if (NCObj.IsValid())
                NounPrefixes.Add(
                    NCObj->GetStringField(TEXT("class")),
                    NCObj->GetStringField(TEXT("prefix_singular")));
        }

    return WarriorRoots.Num() > 0;
}

FString USiSwatiLanguageSubsystem::GenerateName(const FString& NounClass, bool bFemale) const
{
    if (WarriorRoots.IsEmpty()) return TEXT("Mahlanya");
    const int32 Idx = FMath::RandRange(0, WarriorRoots.Num() - 1);
    const FString* Prefix = NounPrefixes.Find(NounClass);
    const FString Pre = Prefix ? *Prefix : TEXT("um");
    return Pre + WarriorRoots[Idx];
}

FString USiSwatiLanguageSubsystem::ToPossessive(const FString& Name, const FString& NounClass) const
{
    // Simplified: siSwati possessive uses concord prefix + 'a' + stem.
    // Return "wa{Name}" for Class 1a as a common form.
    return TEXT("wa") + Name;
}

FString USiSwatiLanguageSubsystem::ApplyInhlonipho(
    const FString& Word, const FString& TabooRoot, const FString& Substitute) const
{
    if (TabooRoot.IsEmpty() || !Word.Contains(TabooRoot)) return Word;
    FString Result = Word;
    Result.ReplaceInline(*TabooRoot, *Substitute, ESearchCase::IgnoreCase);
    return Result;
}

FString USiSwatiLanguageSubsystem::GetGreeting(const FString& Context) const
{
    const FString* Found = Greetings.Find(Context);
    if (Found) return *Found;
    const FString* Default = Greetings.Find(TEXT("sawubona"));
    return Default ? *Default : TEXT("Sawubona");
}

FString USiSwatiLanguageSubsystem::GetPraiseword(const FString& Role) const
{
    const FString* Found = Praisewords.Find(Role);
    if (Found) return *Found;
    const FString* Default = Praisewords.Find(TEXT("Mahlanya"));
    return Default ? *Default : TEXT("Mahlanya");
}
