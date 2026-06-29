// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MahlanyaScalabilitySubsystem.generated.h"

/**
 * Reads the custom mahlanya.* console variables written by the three
 * DefaultScalability.ini tiers (PC Ultra / Mobile High / Mobile Low) and
 * exposes them as typed accessors so simulation plugins branch correctly
 * without each plugin needing to know CVar name strings.
 */
UCLASS()
class MAHLANYARPG_API UMahlanyaScalabilitySubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Returns true when runtime GPU erosion compute shader is active (PC Ultra only).
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Scalability")
    bool IsRuntimeErosionEnabled() const;

    // Returns true when SibayaEngine recomputes Voronoi at runtime.
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Scalability")
    bool IsRuntimeVoronoiEnabled() const;

    // Returns true when GeometricAudioPlugin ray-casts at runtime (ray count > 0).
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Scalability")
    bool IsGeometricAudioEnabled() const;

    // Number of rays per audio frame (0 on mobile — use pre-baked IR convolution).
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Scalability")
    int32 GetGeometricAudioRayCount() const;

    // Maximum acoustic ray bounces (0 on mobile).
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Scalability")
    int32 GetGeometricAudioMaxBounces() const;

    // Streaming draw distance in km (8 PC Ultra, 2 Mobile High, 1 Mobile Low).
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Scalability")
    float GetDrawDistanceKm() const;

    // True on either Mobile tier (RuntimeErosion disabled). Used as a quick guard
    // by any plugin that needs to choose between runtime and pre-baked paths.
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Scalability")
    bool IsMobileTier() const;

private:
    IConsoleVariable* CVar_RuntimeErosion  = nullptr;
    IConsoleVariable* CVar_RuntimeVoronoi  = nullptr;
    IConsoleVariable* CVar_AudioRayCount   = nullptr;
    IConsoleVariable* CVar_AudioMaxBounces = nullptr;
    IConsoleVariable* CVar_DrawDistance    = nullptr;
};
