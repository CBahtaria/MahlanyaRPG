// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UHardwareAdaptiveScaler.generated.h"

UENUM(BlueprintType)
enum class EHardwareTier : uint8
{
    UltraLowEnd = 0,  // integrated GPU / no dedicated VRAM — iGPU path
    LowEnd      = 1,
    MidRange    = 2,
    HighEnd     = 3,
    Ultra       = 4
};

USTRUCT(BlueprintType)
struct FHardwareProfile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 CoreCount = 0;

    UPROPERTY(BlueprintReadOnly)
    float RAM_GB = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float VRAM_GB = 0.f;

    UPROPERTY(BlueprintReadOnly)
    bool bIsMobile = false;

    UPROPERTY(BlueprintReadOnly)
    bool bIsConsole = false;

    UPROPERTY(BlueprintReadOnly)
    FString AdapterName;

    UPROPERTY(BlueprintReadOnly)
    bool bIsIntegratedGPU = false;
};

USTRUCT(BlueprintType)
struct FAdaptiveSimulationConfig
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float ReplicationUpdateFrequency = 30.f;

    UPROPERTY(BlueprintReadOnly)
    int32 MaxReplicatedProperties = 100;

    UPROPERTY(BlueprintReadOnly)
    float SimulationTickBudgetMs = 2.5f;

    UPROPERTY(BlueprintReadOnly)
    float TrustUpdateRateHz = 15.f;

    UPROPERTY(BlueprintReadOnly)
    int32 MaxActiveNPCs = 50;
};

UCLASS()
class MAHLANYARPG_API UHardwareAdaptiveScaler : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Called by UDynamicRuntimeThrottle to reduce settings mid-session
    void DynamicApplyConfig(const FAdaptiveSimulationConfig& NewConfig);

    UFUNCTION(BlueprintPure, Category="Mahlanya|Performance")
    EHardwareTier GetDetectedTier() const { return DetectedTier; }

    UFUNCTION(BlueprintPure, Category="Mahlanya|Performance")
    const FAdaptiveSimulationConfig& GetSimulationConfig() const { return CurrentConfig; }

    UFUNCTION(BlueprintPure, Category="Mahlanya|Performance")
    const FHardwareProfile& GetHardwareProfile() const { return HardwareProfile; }

private:
    EHardwareTier DetectedTier = EHardwareTier::MidRange;
    FHardwareProfile HardwareProfile;
    FAdaptiveSimulationConfig CurrentConfig;
    bool bProfileDetected = false;

    EHardwareTier DetectHardwareTier();
    FAdaptiveSimulationConfig BuildConfigForTier(EHardwareTier Tier) const;
    void WriteCVarsForTier(EHardwareTier Tier);
    void ApplyDeviceProfileOverride();
};
