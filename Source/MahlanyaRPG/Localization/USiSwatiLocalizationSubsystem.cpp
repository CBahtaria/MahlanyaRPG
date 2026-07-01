// Copyright Charles Bartaria. All Rights Reserved.

#include "USiSwatiLocalizationSubsystem.h"
#include "Core/MahlanyaLogChannels.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void USiSwatiLocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (LoadStringTable())
    {
        UE_LOG(LogMahlanyaSimulation, Log,
            TEXT("SiSwatiLocalizationSubsystem: loaded %d strings from siswati_strings.json"),
            StringTable.Num());
    }
    else
    {
        UE_LOG(LogMahlanyaSimulation, Error,
            TEXT("SiSwatiLocalizationSubsystem: FAILED to load siswati_strings.json — all lookups will return key fallbacks"));
    }
}

bool USiSwatiLocalizationSubsystem::LoadStringTable()
{
    const FString JsonPath = FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("pipeline/localization/siswati_strings.json"));

    FString JsonRaw;
    if (!FFileHelper::LoadFileToString(JsonRaw, *JsonPath))
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
            TEXT("SiSwatiLocalizationSubsystem: could not read file at '%s'"), *JsonPath);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonRaw);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
            TEXT("SiSwatiLocalizationSubsystem: JSON parse failed for '%s'"), *JsonPath);
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* StringsArray;
    if (!RootObject->TryGetArrayField(TEXT("strings"), StringsArray))
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
            TEXT("SiSwatiLocalizationSubsystem: 'strings' array not found in JSON"));
        return false;
    }

    StringTable.Reserve(StringsArray->Num());

    for (const TSharedPtr<FJsonValue>& Entry : *StringsArray)
    {
        const TSharedPtr<FJsonObject>* EntryObj;
        if (!Entry->TryGetObject(EntryObj))
        {
            continue;
        }

        FString Key, EnglishStr, SiSwatiStr, RegisterStr, ContextStr;
        if (!(*EntryObj)->TryGetStringField(TEXT("key"), Key))
        {
            continue;
        }

        (*EntryObj)->TryGetStringField(TEXT("en"), EnglishStr);
        (*EntryObj)->TryGetStringField(TEXT("ss"), SiSwatiStr);
        (*EntryObj)->TryGetStringField(TEXT("register"), RegisterStr);
        (*EntryObj)->TryGetStringField(TEXT("context"), ContextStr);

        FLocalizedString Localized;
        Localized.English  = FText::FromString(EnglishStr);
        Localized.SiSwati  = FText::FromString(SiSwatiStr);
        Localized.Register = RegisterStr;
        Localized.Context  = ContextStr;

        StringTable.Add(Key, MoveTemp(Localized));
    }

    bLoaded = (StringTable.Num() > 0);
    return bLoaded;
}

FText USiSwatiLocalizationSubsystem::GetSiSwati(const FString& Key) const
{
    const FLocalizedString* Found = StringTable.Find(Key);
    if (!Found)
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
            TEXT("SiSwatiLocalizationSubsystem::GetSiSwati — key not found: '%s'"), *Key);
        return FText::FromString(FString::Printf(TEXT("[%s]"), *Key));
    }
    return Found->SiSwati;
}

FText USiSwatiLocalizationSubsystem::GetEnglish(const FString& Key) const
{
    const FLocalizedString* Found = StringTable.Find(Key);
    if (!Found)
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
            TEXT("SiSwatiLocalizationSubsystem::GetEnglish — key not found: '%s'"), *Key);
        return FText::FromString(FString::Printf(TEXT("[%s]"), *Key));
    }
    return Found->English;
}

FText USiSwatiLocalizationSubsystem::GetBilingual(const FString& Key) const
{
    const FLocalizedString* Found = StringTable.Find(Key);
    if (!Found)
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
            TEXT("SiSwatiLocalizationSubsystem::GetBilingual — key not found: '%s'"), *Key);
        return FText::FromString(FString::Printf(TEXT("[%s]"), *Key));
    }

    return FText::Format(
        NSLOCTEXT("SiSwatiLocalization", "BilingualFormat", "{0} ({1})"),
        Found->SiSwati,
        Found->English);
}

FText USiSwatiLocalizationSubsystem::GetFormatted(const FString& Key, const TMap<FString, FString>& Args) const
{
    const FLocalizedString* Found = StringTable.Find(Key);
    if (!Found)
    {
        UE_LOG(LogMahlanyaSimulation, Warning,
            TEXT("SiSwatiLocalizationSubsystem::GetFormatted — key not found: '%s'"), *Key);
        return FText::FromString(FString::Printf(TEXT("[%s]"), *Key));
    }

    FString Result = Found->SiSwati.ToString();
    for (const TPair<FString, FString>& Arg : Args)
    {
        const FString Placeholder = FString::Printf(TEXT("{%s}"), *Arg.Key);
        Result = Result.Replace(*Placeholder, *Arg.Value, ESearchCase::CaseSensitive);
    }

    return FText::FromString(Result);
}
