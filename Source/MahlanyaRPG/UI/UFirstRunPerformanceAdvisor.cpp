// Copyright Charles Bartaria. All Rights Reserved.

#include "UFirstRunPerformanceAdvisor.h"
#include "UHardwareAdaptiveScaler.h"
#include "Core/MahlanyaLogChannels.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

void UFirstRunPerformanceAdvisor::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    UE_LOG(LogMahlanyaSimulation, Log, TEXT("FirstRunAdvisor initialized. IsFirstRun=%d"), IsFirstRun());
}

bool UFirstRunPerformanceAdvisor::IsFirstRun() const
{
    const FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config/Windows/MahlanyaGraphics.ini");
    return !IFileManager::Get().FileExists(*ConfigPath);
}

void UFirstRunPerformanceAdvisor::MarkFirstRunComplete()
{
    const FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config/Windows/MahlanyaGraphics.ini");
    GConfig->SetBool(TEXT("Graphics"), TEXT("FirstRunComplete"), true, ConfigPath);
    GConfig->Flush(false, ConfigPath);
}

void UFirstRunPerformanceAdvisor::ConfirmAndDismiss()
{
    MarkFirstRunComplete();
    OnAdvisorDismissed.Broadcast();
}

void UFirstRunPerformanceAdvisor::ApplyRecommended()
{
    if (UGameInstance* GI = GetGameInstance())
        if (UHardwareAdaptiveScaler* S = GI->GetSubsystem<UHardwareAdaptiveScaler>())
            S->WriteCVarsForCurrentTier();
    ConfirmAndDismiss();
}

FString UFirstRunPerformanceAdvisor::GetDetectedTierName() const
{
    if (UGameInstance* GI = GetGameInstance())
        if (UHardwareAdaptiveScaler* S = GI->GetSubsystem<UHardwareAdaptiveScaler>())
        {
            switch (S->GetDetectedTier())
            {
            case EHardwareTier::UltraLowEnd: return TEXT("Ultra Low (Integrated GPU)");
            case EHardwareTier::LowEnd:      return TEXT("Low");
            case EHardwareTier::MidRange:    return TEXT("Medium");
            case EHardwareTier::HighEnd:     return TEXT("High");
            case EHardwareTier::Ultra:       return TEXT("Ultra");
            }
        }
    return TEXT("Medium");
}

FString UFirstRunPerformanceAdvisor::GetHardwareSummary() const
{
    if (UGameInstance* GI = GetGameInstance())
        if (UHardwareAdaptiveScaler* S = GI->GetSubsystem<UHardwareAdaptiveScaler>())
        {
            const FHardwareProfile& P = S->GetHardwareProfile();
            return FString::Printf(TEXT("%s · %d cores · %.1f GB RAM"),
                *P.AdapterName, P.CoreCount, P.RAM_GB);
        }
    return TEXT("");
}

FString UFirstRunPerformanceAdvisor::GetTierDescription() const
{
    if (UGameInstance* GI = GetGameInstance())
        if (UHardwareAdaptiveScaler* S = GI->GetSubsystem<UHardwareAdaptiveScaler>())
        {
            switch (S->GetDetectedTier())
            {
            case EHardwareTier::UltraLowEnd:
                return TEXT("Integrated GPU detected. Nanite and Lumen are disabled. Target: 30fps at 50% resolution. All simulation systems run at reduced scale.");
            case EHardwareTier::LowEnd:
                return TEXT("Entry-level GPU. Nanite disabled, basic shadows. Target: 30fps with dynamic resolution (50–75%). Simulation at reduced scale.");
            case EHardwareTier::MidRange:
                return TEXT("Mid-range GPU. Nanite enabled, Lumen disabled. Target: 60fps with dynamic resolution (60–78%). Full simulation.");
            case EHardwareTier::HighEnd:
                return TEXT("High-end GPU. Nanite and Lumen enabled. Target: 60fps with dynamic resolution (75–90%). Full simulation with geometric audio.");
            case EHardwareTier::Ultra:
                return TEXT("Flagship GPU. All features at maximum quality. 144fps target, native resolution. Full simulation at highest fidelity.");
            }
        }
    return TEXT("");
}
