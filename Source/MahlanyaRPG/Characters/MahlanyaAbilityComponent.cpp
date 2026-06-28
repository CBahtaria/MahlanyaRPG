#include "Characters/MahlanyaAbilityComponent.h"
#include "Characters/MahlanyaEffectsComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogMahlanyaAbility, Log, All);

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UMahlanyaAbilityComponent::UMahlanyaAbilityComponent()
{
    // Ability state must replicate; no per-frame tick required.
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

// ---------------------------------------------------------------------------
// UActorComponent overrides
// ---------------------------------------------------------------------------

void UMahlanyaAbilityComponent::BeginPlay()
{
    Super::BeginPlay();

    // Cache the sibling effects component so ability activation can
    // queue status effects without a runtime search each frame.
    if (AActor* Owner = GetOwner())
    {
        Effects = Owner->FindComponentByClass<UMahlanyaEffectsComponent>();
        if (!Effects)
        {
            UE_LOG(LogMahlanyaAbility, Warning,
                TEXT("[%s] UMahlanyaAbilityComponent: no UMahlanyaEffectsComponent found on owner. "
                     "Effect integration will be unavailable."),
                *GetNameSafe(Owner));
        }
    }
}

void UMahlanyaAbilityComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // FMahlanyaAbilityContainer uses FFastArraySerializer; DOREPLIFETIME is
    // sufficient — the container's NetDeltaSerialize drives delta compression.
    DOREPLIFETIME(UMahlanyaAbilityComponent, Abilities);
}

// ---------------------------------------------------------------------------
// Ability Management
// ---------------------------------------------------------------------------

bool UMahlanyaAbilityComponent::GrantAbility(FName AbilityID, int32 Level)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogMahlanyaAbility, Warning,
            TEXT("GrantAbility called without server authority for '%s'. Ignoring."),
            *AbilityID.ToString());
        return false;
    }

    if (AbilityID.IsNone())
    {
        UE_LOG(LogMahlanyaAbility, Warning, TEXT("GrantAbility called with None AbilityID."));
        return false;
    }

    if (FindSpec(AbilityID) != nullptr)
    {
        UE_LOG(LogMahlanyaAbility, Log,
            TEXT("GrantAbility: ability '%s' already granted — skipping duplicate."),
            *AbilityID.ToString());
        return false;
    }

    FMahlanyaAbilitySpec& NewSpec = Abilities.Items.AddDefaulted_GetRef();
    NewSpec.AbilityID = AbilityID;
    NewSpec.Level     = FMath::Max(1, Level);
    NewSpec.bIsActive = false;

    // Notify FFastArraySerializer that an item was added so it marks the
    // container dirty and queues a delta for connected clients.
    Abilities.MarkItemDirty(NewSpec);

    UE_LOG(LogMahlanyaAbility, Log,
        TEXT("GrantAbility: granted '%s' at level %d to '%s'."),
        *AbilityID.ToString(), NewSpec.Level, *GetNameSafe(GetOwner()));

    return true;
}

bool UMahlanyaAbilityComponent::RevokeAbility(FName AbilityID)
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        UE_LOG(LogMahlanyaAbility, Warning,
            TEXT("RevokeAbility called without server authority for '%s'. Ignoring."),
            *AbilityID.ToString());
        return false;
    }

    const int32 NumItems = Abilities.Items.Num();
    for (int32 i = 0; i < NumItems; ++i)
    {
        if (Abilities.Items[i].AbilityID == AbilityID)
        {
            Abilities.Items.RemoveAt(i);
            // Mark the whole array dirty so FastArraySerializer sends a
            // baseline-reset to clients (removal is a structural change).
            Abilities.MarkArrayDirty();

            UE_LOG(LogMahlanyaAbility, Log,
                TEXT("RevokeAbility: removed '%s' from '%s'."),
                *AbilityID.ToString(), *GetNameSafe(GetOwner()));
            return true;
        }
    }

    UE_LOG(LogMahlanyaAbility, Warning,
        TEXT("RevokeAbility: ability '%s' not found on '%s'."),
        *AbilityID.ToString(), *GetNameSafe(GetOwner()));
    return false;
}

void UMahlanyaAbilityComponent::ServerActivateAbility_Implementation(FName AbilityID)
{
    FMahlanyaAbilitySpec* Spec = FindSpec(AbilityID);
    if (!Spec)
    {
        UE_LOG(LogMahlanyaAbility, Warning,
            TEXT("ServerActivateAbility: ability '%s' not granted on '%s'. Cannot activate."),
            *AbilityID.ToString(), *GetNameSafe(GetOwner()));
        return;
    }

    if (Spec->bIsActive)
    {
        UE_LOG(LogMahlanyaAbility, Log,
            TEXT("ServerActivateAbility: ability '%s' is already active — no-op."),
            *AbilityID.ToString());
        return;
    }

    Spec->bIsActive = true;
    Abilities.MarkItemDirty(*Spec);

    UE_LOG(LogMahlanyaAbility, Log,
        TEXT("ServerActivateAbility: activated '%s' (level %d) on '%s'."),
        *AbilityID.ToString(), Spec->Level, *GetNameSafe(GetOwner()));

    // Effects integration: ability activation may queue a companion effect
    // (e.g. stamina drain, ritual aura). Wiring is deferred to the
    // calling gameplay code via UMahlanyaEffectsComponent::ApplyEffect.
    if (Effects)
    {
        UE_LOG(LogMahlanyaAbility, Verbose,
            TEXT("ServerActivateAbility: UMahlanyaEffectsComponent present — "
                 "callers may apply companion effects via Effects->ApplyEffect()."));
    }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool UMahlanyaAbilityComponent::HasAbility(FName AbilityID) const
{
    return FindSpec(AbilityID) != nullptr;
}

int32 UMahlanyaAbilityComponent::GetAbilityLevel(FName AbilityID) const
{
    const FMahlanyaAbilitySpec* Spec = FindSpec(AbilityID);
    return Spec ? Spec->Level : -1;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

FMahlanyaAbilitySpec* UMahlanyaAbilityComponent::FindSpec(FName AbilityID)
{
    for (FMahlanyaAbilitySpec& Spec : Abilities.Items)
    {
        if (Spec.AbilityID == AbilityID)
        {
            return &Spec;
        }
    }
    return nullptr;
}

const FMahlanyaAbilitySpec* UMahlanyaAbilityComponent::FindSpec(FName AbilityID) const
{
    for (const FMahlanyaAbilitySpec& Spec : Abilities.Items)
    {
        if (Spec.AbilityID == AbilityID)
        {
            return &Spec;
        }
    }
    return nullptr;
}
