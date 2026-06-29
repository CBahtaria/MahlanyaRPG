// Copyright Charles Bartaria. All Rights Reserved.

#include "UHardwareAdaptiveScaler.h"
#include "MahlanyaPerformanceCVars.h"
#include "Core/MahlanyaLogChannels.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformProperties.h"
#include "RHI.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "DeviceProfiles/DeviceProfile.h"

// CVar names for the 5 simulation CVars registered by UMahlanyaScalabilitySubsystem
static const TCHAR* CVar_RuntimeErosion  = TEXT("mahlanya.RuntimeErosion.Enabled");
static const TCHAR* CVar_RuntimeVoronoi  = TEXT("mahlanya.RuntimeVoronoi.Enabled");
static const TCHAR* CVar_AudioRayCount   = TEXT("mahlanya.GeometricAudio.RayCount");
static const TCHAR* CVar_AudioMaxBounces = TEXT("mahlanya.GeometricAudio.MaxBounces");
static const TCHAR* CVar_DrawDistance    = TEXT("mahlanya.DrawDistance.Km");

void UHardwareAdaptiveScaler::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Developer override: skip detection when ForceHardwareTier != -1
    const int32 ForcedTier = MahlanyaPerformanceCVars::ForceHardwareTier.GetValueOnGameThread();
    if (ForcedTier >= 0 && ForcedTier <= 3)
    {
        DetectedTier = static_cast<EHardwareTier>(ForcedTier);
        bProfileDetected = true;
        UE_LOG(LogMahlanyaHardware, Log,
               TEXT("Hardware tier forced to %d via mahlanya.ForceHardwareTier"), ForcedTier);
    }
    else
    {
        DetectedTier = DetectHardwareTier();
        bProfileDetected = true;
    }

    CurrentConfig = BuildConfigForTier(DetectedTier);
    ApplyDeviceProfileOverride();
    WriteCVarsForTier(DetectedTier);

    UE_LOG(LogMahlanyaHardware, Log,
           TEXT("Hardware adaptive scaler initialized. Tier=%d Cores=%d RAM=%.1fGB VRAM=%.1fGB"),
           (int32)DetectedTier, HardwareProfile.CoreCount, HardwareProfile.RAM_GB, HardwareProfile.VRAM_GB);
}

EHardwareTier UHardwareAdaptiveScaler::DetectHardwareTier()
{
    HardwareProfile.CoreCount = FPlatformMisc::NumberOfCores();
    HardwareProfile.bIsMobile = FPlatformProperties::IsServer() ? false :
        (FString(FPlatformProperties::IniPlatformName()).Contains(TEXT("Android")) ||
         FString(FPlatformProperties::IniPlatformName()).Contains(TEXT("IOS")));
    HardwareProfile.bIsConsole = FPlatformProperties::IsConsole();

    // RAM
    const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
    HardwareProfile.RAM_GB = static_cast<float>(MemStats.TotalPhysical) / (1024.f * 1024.f * 1024.f);

    // VRAM: get adapter name from RHI; leave VRAM_GB = 0 as a safe cross-platform fallback.
    // The scoring formula (cores * 2 + RAM) is the primary signal; VRAM is additive when
    // a version-stable RHI query API is available in a future task.
    HardwareProfile.VRAM_GB = 0.f;
    if (GDynamicRHI)
    {
        HardwareProfile.AdapterName = FString(GDynamicRHI->RHIGetAdapterName());
    }

    // Consoles: always Ultra
    if (HardwareProfile.bIsConsole)
    {
        return EHardwareTier::Ultra;
    }

    // Score: cores * 2 + RAM_GB + VRAM_GB
    const float Score = HardwareProfile.CoreCount * 2.f
                      + HardwareProfile.RAM_GB
                      + HardwareProfile.VRAM_GB;

    EHardwareTier Tier;
    if (Score >= 28.f)
        Tier = EHardwareTier::Ultra;
    else if (Score >= 16.f)
        Tier = EHardwareTier::HighEnd;
    else if (Score >= 8.f)
        Tier = EHardwareTier::MidRange;
    else
        Tier = EHardwareTier::LowEnd;

    // Mobile caps at MidRange regardless of score
    if (HardwareProfile.bIsMobile && Tier > EHardwareTier::MidRange)
    {
        Tier = EHardwareTier::MidRange;
    }

    return Tier;
}

FAdaptiveSimulationConfig UHardwareAdaptiveScaler::BuildConfigForTier(EHardwareTier Tier) const
{
    FAdaptiveSimulationConfig Cfg;
    switch (Tier)
    {
    case EHardwareTier::LowEnd:
        Cfg.ReplicationUpdateFrequency = 10.f;
        Cfg.MaxReplicatedProperties    = 30;
        Cfg.SimulationTickBudgetMs     = 1.2f;
        Cfg.TrustUpdateRateHz          = 5.f;
        Cfg.MaxActiveNPCs              = 25;
        break;
    case EHardwareTier::MidRange:
        Cfg.ReplicationUpdateFrequency = 20.f;
        Cfg.MaxReplicatedProperties    = 60;
        Cfg.SimulationTickBudgetMs     = 2.0f;
        Cfg.TrustUpdateRateHz          = 10.f;
        Cfg.MaxActiveNPCs              = 50;
        break;
    case EHardwareTier::HighEnd:
        Cfg.ReplicationUpdateFrequency = 30.f;
        Cfg.MaxReplicatedProperties    = 150;
        Cfg.SimulationTickBudgetMs     = 3.0f;
        Cfg.TrustUpdateRateHz          = 15.f;
        Cfg.MaxActiveNPCs              = 100;
        break;
    case EHardwareTier::Ultra:
        Cfg.ReplicationUpdateFrequency = 60.f;
        Cfg.MaxReplicatedProperties    = 250;
        Cfg.SimulationTickBudgetMs     = 4.5f;
        Cfg.TrustUpdateRateHz          = 30.f;
        Cfg.MaxActiveNPCs              = 200;
        break;
    default:
        break;
    }
    return Cfg;
}

void UHardwareAdaptiveScaler::WriteCVarsForTier(EHardwareTier Tier)
{
    IConsoleManager& CM = IConsoleManager::Get();

    auto SetInt = [&](const TCHAR* Name, int32 Val)
    {
        if (IConsoleVariable* CVar = CM.FindConsoleVariable(Name))
            CVar->Set(Val, ECVF_SetByCode);
    };
    auto SetFloat = [&](const TCHAR* Name, float Val)
    {
        if (IConsoleVariable* CVar = CM.FindConsoleVariable(Name))
            CVar->Set(Val, ECVF_SetByCode);
    };

    switch (Tier)
    {
    case EHardwareTier::LowEnd:
        SetInt  (CVar_RuntimeErosion,  0);
        SetInt  (CVar_RuntimeVoronoi,  0);
        SetInt  (CVar_AudioRayCount,   0);
        SetInt  (CVar_AudioMaxBounces, 0);
        SetFloat(CVar_DrawDistance,    1.0f);
        break;
    case EHardwareTier::MidRange:
        SetInt  (CVar_RuntimeErosion,  0);
        SetInt  (CVar_RuntimeVoronoi,  0);
        SetInt  (CVar_AudioRayCount,   0);
        SetInt  (CVar_AudioMaxBounces, 0);
        SetFloat(CVar_DrawDistance,    2.0f);
        break;
    case EHardwareTier::HighEnd:
        SetInt  (CVar_RuntimeErosion,  1);
        SetInt  (CVar_RuntimeVoronoi,  1);
        SetInt  (CVar_AudioRayCount,   32);
        SetInt  (CVar_AudioMaxBounces, 4);
        SetFloat(CVar_DrawDistance,    6.0f);
        break;
    case EHardwareTier::Ultra:
        SetInt  (CVar_RuntimeErosion,  1);
        SetInt  (CVar_RuntimeVoronoi,  1);
        SetInt  (CVar_AudioRayCount,   64);
        SetInt  (CVar_AudioMaxBounces, 6);
        SetFloat(CVar_DrawDistance,    8.0f);
        break;
    default:
        break;
    }
}

void UHardwareAdaptiveScaler::ApplyDeviceProfileOverride()
{
    // Stub: will be fully wired in Task 10.2 when USimulationDeviceSettings is created.
    // For now, log the active device profile name for diagnostics.
    const UDeviceProfileManager* DPMgr = UDeviceProfileManager::TryGet();
    if (!DPMgr)
        return;

    const UDeviceProfile* ActiveProfile = DPMgr->GetActiveProfile();
    if (!ActiveProfile)
        return;

    UE_LOG(LogMahlanyaHardware, Verbose,
           TEXT("Active device profile: %s"), *ActiveProfile->GetName());
}

void UHardwareAdaptiveScaler::DynamicApplyConfig(const FAdaptiveSimulationConfig& NewConfig)
{
    // The throttle system computes the reduced config externally and passes it in.
    // Simply store it — orchestrators and replication managers poll GetSimulationConfig().
    // The 5 simulation CVars were written at tier-detection time and are not re-written
    // here; they reflect hardware capability, not session throttle state.
    CurrentConfig = NewConfig;
}
