#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/MahlanyaAbilityComponent.h" // for ESwaziEffectType
#include "MahlanyaEffectsComponent.generated.h"

// ---------------------------------------------------------------------------
// FMahlanyaActiveEffect
//
// Describes one active status effect applied to the owning character.
// Effects are driven by Swazi cultural logic:
//   - Instant   — consumed immediately (e.g. a calabash of muti)
//   - Duration  — expires after Duration seconds (e.g. Umhlanga ceremony buff)
//   - Infinite  — persists until explicitly removed (e.g. ancestral protection
//                 broken only by a taboo violation)
//
// Magnitude maps to curve table entries such as CT_AltitudeFatigue, allowing
// environment-driven effect scaling without hard-coded constants.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FMahlanyaActiveEffect
{
    GENERATED_BODY()

    /** Unique effect identifier, e.g. "Effect.AncestralShield" or "Effect.MutiHeal". */
    UPROPERTY()
    FName EffectID;

    /** Lifecycle category controlling how the effect is ticked and expired. */
    UPROPERTY()
    ESwaziEffectType Type = ESwaziEffectType::Instant;

    /**
     * Scalar applied to the target stat.
     * Sourced from curve tables (e.g. CT_AltitudeFatigue) so designers can
     * author values in the editor rather than hard-coding them here.
     */
    UPROPERTY()
    float Magnitude = 1.0f;

    /**
     * Total active duration in seconds.
     * Only meaningful when Type == Duration; ignored for Instant and Infinite.
     */
    UPROPERTY()
    float Duration = 0.0f;

    /**
     * Time (seconds) the effect has been active since application.
     * Incremented each tick; compared against Duration to determine expiry.
     */
    UPROPERTY()
    float Elapsed = 0.0f;
};

// ---------------------------------------------------------------------------
// UMahlanyaEffectsComponent
//
// Tracks all active status effects on the owning character.
// This component is intentionally separate from UMahlanyaAbilityComponent:
//   - Abilities own the WHAT (which powers the character has)
//   - Effects own the HOW LONG and HOW MUCH (active buffs/debuffs)
//
// Ticked locally on both server and client; authoritative removal happens
// on the server. Duration and Infinite effects are processed in TickComponent.
// Instant effects are applied and removed in the same call to ApplyEffect.
//
// Note: ActiveEffects is not replicated — the server applies effects and
// clients receive them via gameplay cues or a separate replication layer
// (to be added when the Swazi Gameplay Cue system is implemented).
// ---------------------------------------------------------------------------
UCLASS(ClassGroup=(Mahlanya), meta=(BlueprintSpawnableComponent))
class MAHLANYARPG_API UMahlanyaEffectsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMahlanyaEffectsComponent();

    // -----------------------------------------------------------------------
    // Effect Application
    // -----------------------------------------------------------------------

    /**
     * Apply an effect to the owning character.
     * - Instant effects are consumed immediately (no ongoing tick entry created).
     * - Duration effects are added to ActiveEffects and ticked until expiry.
     * - Infinite effects are added and remain until RemoveEffect is called.
     * If an effect with the same EffectID already exists its Elapsed is reset.
     */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Effects")
    void ApplyEffect(const FMahlanyaActiveEffect& Effect);

    /**
     * Remove an active effect by ID.
     * Safe to call even if the effect is not currently active (no-op).
     */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Effects")
    void RemoveEffect(FName EffectID);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /** Returns true if an effect with the given ID is currently active. */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Effects")
    bool HasEffect(FName EffectID) const;

    /**
     * Accumulates the Magnitude values of all active effects whose EffectID
     * begins with StatCategory (e.g. "Stamina" matches "Stamina.MutiHeal").
     * Returns 1.0f when no effects are active for that category so callers
     * can safely multiply without guarding for the zero case.
     *
     * Example: GetCombinedMagnitude("Stamina") aggregates all stamina effects.
     */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Effects")
    float GetCombinedMagnitude(FName StatCategory) const;

    // -----------------------------------------------------------------------
    // UActorComponent overrides
    // -----------------------------------------------------------------------

    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;

    /** All currently active Duration and Infinite effects. */
    UPROPERTY()
    TArray<FMahlanyaActiveEffect> ActiveEffects;
};
