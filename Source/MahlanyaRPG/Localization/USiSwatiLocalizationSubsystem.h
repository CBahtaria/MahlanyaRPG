// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Misc/FileHelper.h"
#include "USiSwatiLocalizationSubsystem.generated.h"

UENUM(BlueprintType)
enum class ELanguageRegister : uint8
{
    Standard,       // Normal speech — any speaker
    Formal,         // Protocol speech — addressing elders/inkosi
    Inhlonipho,     // Avoidance vocabulary — near in-laws/royalty
    Ceremonial,     // Incwala, Umhlanga — ritual context only
};

USTRUCT(BlueprintType)
struct FLocalizedString
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FText English;
    UPROPERTY(BlueprintReadOnly) FText SiSwati;
    UPROPERTY(BlueprintReadOnly) FString Register;
    UPROPERTY(BlueprintReadOnly) FString Context;
};

UCLASS()
class MAHLANYARPG_API USiSwatiLocalizationSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Returns the siSwati string for the given key. Falls back to English if siSwati not loaded.
    UFUNCTION(BlueprintPure, Category="Mahlanya|Localization")
    FText GetSiSwati(const FString& Key) const;

    // Returns the English string for the given key.
    UFUNCTION(BlueprintPure, Category="Mahlanya|Localization")
    FText GetEnglish(const FString& Key) const;

    // Returns both with format: "siSwati (English)" — used in subtitle overlays.
    UFUNCTION(BlueprintPure, Category="Mahlanya|Localization")
    FText GetBilingual(const FString& Key) const;

    // Format with arguments: GetFormatted("ui.hud.cattle_count", {{"0", cattle}})
    UFUNCTION(BlueprintPure, Category="Mahlanya|Localization")
    FText GetFormatted(const FString& Key, const TMap<FString, FString>& Args) const;

private:
    TMap<FString, FLocalizedString> StringTable;
    bool bLoaded = false;

    bool LoadStringTable();
};
