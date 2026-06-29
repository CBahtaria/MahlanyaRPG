// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UHardwareAdaptiveScaler.h"
#include "USimulationDeviceSettings.generated.h"

USTRUCT(BlueprintType)
struct FDeviceSimulationProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Config, Category="Device Profile")
    FString DeviceProfileName;

    UPROPERTY(EditAnywhere, Config, Category="Device Profile")
    EHardwareTier OverrideTier = EHardwareTier::MidRange;

    UPROPERTY(EditAnywhere, Config, Category="Device Profile")
    float ReplicationFrequency = 30.f;

    UPROPERTY(EditAnywhere, Config, Category="Device Profile")
    int32 MaxProperties = 100;

    UPROPERTY(EditAnywhere, Config, Category="Device Profile")
    float TickBudgetMs = 2.5f;

    UPROPERTY(EditAnywhere, Config, Category="Device Profile")
    int32 MaxActiveNPCs = 50;
};

/**
 * Per-platform simulation overrides loaded from DefaultGame.ini.
 * UHardwareAdaptiveScaler reads this to apply device-profile-specific configs
 * that override the auto-detected hardware tier.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Simulation Device Profiles",
       CategoryName="Plugins/Simulation"))
class MAHLANYARPG_API USimulationDeviceSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Device Profiles")
    TArray<FDeviceSimulationProfile> DeviceProfiles;

    // Returns the profile matching the given device profile name, or nullptr.
    const FDeviceSimulationProfile* FindProfileByName(const FString& ProfileName) const;
};
