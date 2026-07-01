// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UHardwareAdaptiveScaler.h"
#include "UGraphicsSettingsWidget.generated.h"

UCLASS(Abstract, BlueprintType, Blueprintable)
class MAHLANYARPG_API UGraphicsSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ── Tier info (read-only) ──────────────────────────────────────────────
    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    EHardwareTier GetDetectedTier() const;

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    FString GetDetectedTierName() const;

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    FString GetHardwareSummary() const;   // "Intel UHD 620 · 4 cores · 16.0GB RAM"

    // ── Override tier (calls mahlanya.ForceHardwareTier + re-applies CVars) ─
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void SetTierOverride(EHardwareTier NewTier);

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void ClearTierOverride();   // sets ForceHardwareTier=-1, re-detects

    // ── Individual settings (read current CVar, write on change) ──────────
    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    int32 GetShadowQuality() const;

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void SetShadowQuality(int32 Quality);   // 0–3, clamps to tier bounds

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    float GetScreenPercentage() const;

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void SetScreenPercentage(float Pct);    // 50–100, clamps to tier bounds

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    bool GetDynamicResEnabled() const;

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void SetDynamicResEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    float GetMaxFPS() const;

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void SetMaxFPS(float FPS);   // 30, 60, 120, 144, 0 (unlimited)

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    float GetCurrentFPS() const;   // from UPerformanceAutoTuner

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    int32 GetTextureQuality() const;

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void SetTextureQuality(int32 Quality);  // 0–3

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    int32 GetEffectsQuality() const;

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void SetEffectsQuality(int32 Quality);

    // ── Persistence ────────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void SaveSettings();

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void ResetToTierDefaults();

private:
    IConsoleVariable* GetCVar(const TCHAR* Name) const;
    int32 GetCVarInt(const TCHAR* Name, int32 Default = 0) const;
    float GetCVarFloat(const TCHAR* Name, float Default = 0.f) const;
    void SetCVarInt(const TCHAR* Name, int32 Val);
    void SetCVarFloat(const TCHAR* Name, float Val);
};
