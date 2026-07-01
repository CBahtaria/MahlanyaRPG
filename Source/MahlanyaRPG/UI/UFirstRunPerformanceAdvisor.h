// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UHardwareAdaptiveScaler.h"
#include "UFirstRunPerformanceAdvisor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAdvisorDismissed);

UCLASS(Abstract, BlueprintType, Blueprintable)
class MAHLANYARPG_API UFirstRunPerformanceAdvisor : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeOnInitialized() override;

    // Called by Blueprint "Confirm" button
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void ConfirmAndDismiss();

    // Called by Blueprint "Use Recommended" button
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Settings")
    void ApplyRecommended();

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    FString GetDetectedTierName() const;

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    FString GetHardwareSummary() const;

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    FString GetTierDescription() const;   // human-readable description of what the tier enables

    UFUNCTION(BlueprintPure, Category="Mahlanya|Settings")
    bool IsFirstRun() const;   // reads Saved/Config/MahlanyaGraphics.ini — true if not found

    UPROPERTY(BlueprintAssignable, Category="Mahlanya|Settings")
    FOnAdvisorDismissed OnAdvisorDismissed;

private:
    void MarkFirstRunComplete();
};
