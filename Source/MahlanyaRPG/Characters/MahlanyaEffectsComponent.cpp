#include "Characters/MahlanyaEffectsComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogMahlanyaEffects, Log, All);

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UMahlanyaEffectsComponent::UMahlanyaEffectsComponent()
{
    // Effects need a per-frame tick to advance Duration effects and expire them.
    PrimaryComponentTick.bCanEverTick = true;
    // No replication of the component itself; effect state is server-authoritative
    // and communicated to clients through gameplay cues (future implementation).
    SetIsReplicatedByDefault(false);
}

// ---------------------------------------------------------------------------
// UActorComponent overrides
// ---------------------------------------------------------------------------

void UMahlanyaEffectsComponent::BeginPlay()
{
    Super::BeginPlay();
    ActiveEffects.Reserve(8); // Pre-allocate for typical max simultaneous effects
}

void UMahlanyaEffectsComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Iterate in reverse so we can remove expired effects without index fixup.
    for (int32 i = ActiveEffects.Num() - 1; i >= 0; --i)
    {
        FMahlanyaActiveEffect& Effect = ActiveEffects[i];

        if (Effect.Type == ESwaziEffectType::Infinite)
        {
            // Infinite effects have no expiry; they tick but never self-remove.
            continue;
        }

        // ESwaziEffectType::Duration — advance elapsed time and check expiry.
        // (Instant effects are never stored in ActiveEffects, so only Duration
        //  reaches this branch.)
        Effect.Elapsed += DeltaTime;
        if (Effect.Elapsed >= Effect.Duration)
        {
            UE_LOG(LogMahlanyaEffects, Log,
                TEXT("TickComponent: effect '%s' expired after %.2f seconds on '%s'."),
                *Effect.EffectID.ToString(), Effect.Elapsed, *GetNameSafe(GetOwner()));

            ActiveEffects.RemoveAt(i);
        }
    }
}

// ---------------------------------------------------------------------------
// Effect Application
// ---------------------------------------------------------------------------

void UMahlanyaEffectsComponent::ApplyEffect(const FMahlanyaActiveEffect& Effect)
{
    if (Effect.EffectID.IsNone())
    {
        UE_LOG(LogMahlanyaEffects, Warning,
            TEXT("ApplyEffect called with None EffectID on '%s'. Ignoring."),
            *GetNameSafe(GetOwner()));
        return;
    }

    // Instant effects are consumed at the point of application.
    // They are intentionally not stored — callers should read Magnitude
    // from the struct before calling ApplyEffect if they need to act on it.
    if (Effect.Type == ESwaziEffectType::Instant)
    {
        UE_LOG(LogMahlanyaEffects, Log,
            TEXT("ApplyEffect: instant effect '%s' (magnitude %.2f) consumed on '%s'."),
            *Effect.EffectID.ToString(), Effect.Magnitude, *GetNameSafe(GetOwner()));
        // Future: broadcast a gameplay cue so the UI and audio system react.
        return;
    }

    // For Duration and Infinite effects, check whether the same effect is
    // already active; if so, reset its elapsed time (re-application refreshes).
    for (FMahlanyaActiveEffect& Existing : ActiveEffects)
    {
        if (Existing.EffectID == Effect.EffectID)
        {
            UE_LOG(LogMahlanyaEffects, Log,
                TEXT("ApplyEffect: refreshing existing effect '%s' on '%s' "
                     "(elapsed was %.2f, resetting to 0)."),
                *Effect.EffectID.ToString(), *GetNameSafe(GetOwner()), Existing.Elapsed);

            Existing.Elapsed    = 0.0f;
            Existing.Magnitude  = Effect.Magnitude;
            Existing.Duration   = Effect.Duration;
            return;
        }
    }

    // New effect — add to active list.
    FMahlanyaActiveEffect NewEffect = Effect;
    NewEffect.Elapsed = 0.0f; // Ensure elapsed starts at zero regardless of input
    ActiveEffects.Add(NewEffect);

    UE_LOG(LogMahlanyaEffects, Log,
        TEXT("ApplyEffect: applied '%s' (type=%d, magnitude=%.2f, duration=%.2f) on '%s'."),
        *Effect.EffectID.ToString(),
        static_cast<int32>(Effect.Type),
        Effect.Magnitude,
        Effect.Duration,
        *GetNameSafe(GetOwner()));
}

void UMahlanyaEffectsComponent::RemoveEffect(FName EffectID)
{
    const int32 NumRemoved = ActiveEffects.RemoveAll(
        [&EffectID](const FMahlanyaActiveEffect& Effect)
        {
            return Effect.EffectID == EffectID;
        });

    if (NumRemoved > 0)
    {
        UE_LOG(LogMahlanyaEffects, Log,
            TEXT("RemoveEffect: removed '%s' from '%s'."),
            *EffectID.ToString(), *GetNameSafe(GetOwner()));
    }
    else
    {
        UE_LOG(LogMahlanyaEffects, Verbose,
            TEXT("RemoveEffect: effect '%s' was not active on '%s' — no-op."),
            *EffectID.ToString(), *GetNameSafe(GetOwner()));
    }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool UMahlanyaEffectsComponent::HasEffect(FName EffectID) const
{
    for (const FMahlanyaActiveEffect& Effect : ActiveEffects)
    {
        if (Effect.EffectID == EffectID)
        {
            return true;
        }
    }
    return false;
}

float UMahlanyaEffectsComponent::GetCombinedMagnitude(FName StatCategory) const
{
    if (StatCategory.IsNone())
    {
        return 1.0f;
    }

    const FString CategoryStr = StatCategory.ToString();
    float Combined = 1.0f;
    bool bFoundAny = false;

    for (const FMahlanyaActiveEffect& Effect : ActiveEffects)
    {
        // Match effects whose ID starts with the requested stat category.
        // Convention: EffectID = "StatCategory.SpecificEffectName"
        // e.g. StatCategory="Stamina" matches "Stamina.MutiHeal", "Stamina.AltitudeFatigue"
        if (Effect.EffectID.ToString().StartsWith(CategoryStr))
        {
            Combined *= Effect.Magnitude;
            bFoundAny = true;
        }
    }

    if (!bFoundAny)
    {
        // No active effects for this category; return neutral multiplier.
        return 1.0f;
    }

    return Combined;
}
