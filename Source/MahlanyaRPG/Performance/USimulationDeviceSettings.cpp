// Copyright Charles Bartaria. All Rights Reserved.

#include "USimulationDeviceSettings.h"

const FDeviceSimulationProfile* USimulationDeviceSettings::FindProfileByName(
    const FString& ProfileName) const
{
    for (const FDeviceSimulationProfile& Profile : DeviceProfiles)
    {
        if (Profile.DeviceProfileName == ProfileName)
        {
            return &Profile;
        }
    }
    return nullptr;
}
