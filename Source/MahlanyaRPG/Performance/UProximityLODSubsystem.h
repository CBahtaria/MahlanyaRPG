// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "UProximityLODSubsystem.generated.h"

UENUM(BlueprintType)
enum class EProximityLODTier : uint8
{
    Focus      = 0,   // In crosshair cone or interaction range
    Visible    = 1,   // Broadly in front of player
    Background = 2,   // Behind player — force lowest LOD, no shadow
};

UCLASS()
class MAHLANYARPG_API UProximityLODSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // FTickableGameObject
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    virtual bool IsTickableInEditor() const override { return false; }
    virtual bool IsTickableWhenPaused() const override { return false; }

    // Call from interaction systems to pin a component in Focus tier
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Performance")
    void SetInteractionFocus(UPrimitiveComponent* Component, bool bFocused);

    UFUNCTION(BlueprintPure, Category="Mahlanya|Performance")
    EProximityLODTier GetComponentTier(UPrimitiveComponent* Component) const;

private:
    // Components pinned to Focus by the interaction system
    TSet<TWeakObjectPtr<UPrimitiveComponent>> PinnedFocusComponents;

    // Per-component tier cache to avoid redundant SetCastShadow / ForcedLodModel calls
    TMap<TWeakObjectPtr<UPrimitiveComponent>, EProximityLODTier> TierCache;

    float EvalAccumulator = 0.f;
    bool bSubsystemEnabled = true;

    void EvaluateAll();
    EProximityLODTier ClassifyComponent(const FVector& CompLocation,
                                         const FVector& ViewOrigin,
                                         const FVector& ViewForward,
                                         float FocusCosAngle) const;
    void ApplyTierToStaticMesh(UStaticMeshComponent* SMC, EProximityLODTier Tier);
    void ApplyTierToSkeletalMesh(USkeletalMeshComponent* SKC, EProximityLODTier Tier);

    // Restore all overrides (called on Deinitialize)
    void RestoreAllComponents();
};
