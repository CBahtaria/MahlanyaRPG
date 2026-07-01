// Copyright Charles Bartaria. All Rights Reserved.

#include "UGraphicsSettingsWidget.h"
#include "UPerformanceAutoTuner.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"

// ── Tier info ─────────────────────────────────────────────────────────────────

EHardwareTier UGraphicsSettingsWidget::GetDetectedTier() const
{
    if (UGameInstance* GI = GetGameInstance())
        if (UHardwareAdaptiveScaler* S = GI->GetSubsystem<UHardwareAdaptiveScaler>())
            return S->GetDetectedTier();
    return EHardwareTier::MidRange;
}

FString UGraphicsSettingsWidget::GetDetectedTierName() const
{
    switch (GetDetectedTier())
    {
    case EHardwareTier::UltraLowEnd: return TEXT("Ultra Low (Integrated GPU)");
    case EHardwareTier::LowEnd:      return TEXT("Low");
    case EHardwareTier::MidRange:    return TEXT("Medium");
    case EHardwareTier::HighEnd:     return TEXT("High");
    case EHardwareTier::Ultra:       return TEXT("Ultra");
    default:                         return TEXT("Unknown");
    }
}

FString UGraphicsSettingsWidget::GetHardwareSummary() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UHardwareAdaptiveScaler* S = GI->GetSubsystem<UHardwareAdaptiveScaler>())
        {
            const FHardwareProfile& P = S->GetHardwareProfile();
            return FString::Printf(TEXT("%s · %d cores · %.1f GB RAM"),
                *P.AdapterName, P.CoreCount, P.RAM_GB);
        }
    }
    return TEXT("Unknown hardware");
}

// ── Tier override ─────────────────────────────────────────────────────────────

void UGraphicsSettingsWidget::SetTierOverride(EHardwareTier NewTier)
{
    SetCVarInt(TEXT("mahlanya.ForceHardwareTier"), (int32)NewTier);
    if (UGameInstance* GI = GetGameInstance())
        if (UHardwareAdaptiveScaler* S = GI->GetSubsystem<UHardwareAdaptiveScaler>())
            S->WriteCVarsForCurrentTier();
}

void UGraphicsSettingsWidget::ClearTierOverride()
{
    SetCVarInt(TEXT("mahlanya.ForceHardwareTier"), -1);
}

// ── Individual settings ───────────────────────────────────────────────────────

int32 UGraphicsSettingsWidget::GetShadowQuality() const   { return GetCVarInt(TEXT("sg.ShadowQuality"), 1); }
void  UGraphicsSettingsWidget::SetShadowQuality(int32 Q)  { SetCVarInt(TEXT("sg.ShadowQuality"), FMath::Clamp(Q, 0, 3)); }

float UGraphicsSettingsWidget::GetScreenPercentage() const       { return GetCVarFloat(TEXT("r.ScreenPercentage"), 80.f); }
void  UGraphicsSettingsWidget::SetScreenPercentage(float P)      { SetCVarFloat(TEXT("r.ScreenPercentage"), FMath::Clamp(P, 50.f, 100.f)); }

bool  UGraphicsSettingsWidget::GetDynamicResEnabled() const      { return GetCVarInt(TEXT("r.DynamicRes.Enabled"), 0) != 0; }
void  UGraphicsSettingsWidget::SetDynamicResEnabled(bool b)      { SetCVarInt(TEXT("r.DynamicRes.Enabled"), b ? 1 : 0); }

float UGraphicsSettingsWidget::GetMaxFPS() const                 { return GetCVarFloat(TEXT("t.MaxFPS"), 60.f); }
void  UGraphicsSettingsWidget::SetMaxFPS(float F)                { SetCVarFloat(TEXT("t.MaxFPS"), F); }

int32 UGraphicsSettingsWidget::GetTextureQuality() const         { return GetCVarInt(TEXT("sg.TextureQuality"), 1); }
void  UGraphicsSettingsWidget::SetTextureQuality(int32 Q)        { SetCVarInt(TEXT("sg.TextureQuality"), FMath::Clamp(Q, 0, 3)); }

int32 UGraphicsSettingsWidget::GetEffectsQuality() const         { return GetCVarInt(TEXT("sg.EffectsQuality"), 1); }
void  UGraphicsSettingsWidget::SetEffectsQuality(int32 Q)        { SetCVarInt(TEXT("sg.EffectsQuality"), FMath::Clamp(Q, 0, 3)); }

float UGraphicsSettingsWidget::GetCurrentFPS() const
{
    if (UWorld* W = GetWorld())
        if (UPerformanceAutoTuner* T = W->GetSubsystem<UPerformanceAutoTuner>())
            return T->GetCurrentFPS();
    return 0.f;
}

// ── Persistence ───────────────────────────────────────────────────────────────

void UGraphicsSettingsWidget::SaveSettings()
{
    // Persist via GConfig to Saved/Config/Windows/MahlanyaGraphics.ini
    const FString ConfigPath = FPaths::ProjectSavedDir() / TEXT("Config/Windows/MahlanyaGraphics.ini");
    GConfig->SetInt(TEXT("Graphics"), TEXT("ShadowQuality"),  GetShadowQuality(), ConfigPath);
    GConfig->SetFloat(TEXT("Graphics"), TEXT("ScreenPct"),    GetScreenPercentage(), ConfigPath);
    GConfig->SetInt(TEXT("Graphics"), TEXT("TextureQuality"), GetTextureQuality(), ConfigPath);
    GConfig->SetInt(TEXT("Graphics"), TEXT("EffectsQuality"), GetEffectsQuality(), ConfigPath);
    GConfig->SetInt(TEXT("Graphics"), TEXT("TierOverride"),   GetCVarInt(TEXT("mahlanya.ForceHardwareTier"), -1), ConfigPath);
    GConfig->Flush(false, ConfigPath);
}

void UGraphicsSettingsWidget::ResetToTierDefaults()
{
    if (UGameInstance* GI = GetGameInstance())
        if (UHardwareAdaptiveScaler* S = GI->GetSubsystem<UHardwareAdaptiveScaler>())
            S->WriteCVarsForCurrentTier();
}

// ── CVar helpers ──────────────────────────────────────────────────────────────

IConsoleVariable* UGraphicsSettingsWidget::GetCVar(const TCHAR* Name) const
{
    return IConsoleManager::Get().FindConsoleVariable(Name);
}

int32 UGraphicsSettingsWidget::GetCVarInt(const TCHAR* Name, int32 Default) const
{
    if (IConsoleVariable* V = GetCVar(Name)) return V->GetInt();
    return Default;
}

float UGraphicsSettingsWidget::GetCVarFloat(const TCHAR* Name, float Default) const
{
    if (IConsoleVariable* V = GetCVar(Name)) return V->GetFloat();
    return Default;
}

void UGraphicsSettingsWidget::SetCVarInt(const TCHAR* Name, int32 Val)
{
    if (IConsoleVariable* V = GetCVar(Name)) V->Set(Val, ECVF_SetByCode);
}

void UGraphicsSettingsWidget::SetCVarFloat(const TCHAR* Name, float Val)
{
    if (IConsoleVariable* V = GetCVar(Name)) V->Set(Val, ECVF_SetByCode);
}
