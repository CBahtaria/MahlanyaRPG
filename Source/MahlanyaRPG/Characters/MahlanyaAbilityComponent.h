#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "MahlanyaAbilityComponent.generated.h"

class UMahlanyaEffectsComponent;

// ---------------------------------------------------------------------------
// ESwaziEffectType
// Classifies the lifecycle of a Swazi ritual or combat effect.
//   Instant  — e.g. consuming a calabash of muti; fires once and expires
//   Duration — e.g. Umhlanga ceremony blessing; active for a fixed time
//   Infinite — e.g. ancestral protection; persists until broken by a taboo
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class ESwaziEffectType : uint8
{
    Instant  UMETA(DisplayName = "Instant (Muti Consumed)"),
    Duration UMETA(DisplayName = "Duration (Ritual Blessing)"),
    Infinite UMETA(DisplayName = "Infinite (Ancestral Protection)"),
};

// ---------------------------------------------------------------------------
// FMahlanyaAbilitySpec
// A single ability entry within the replicated ability container.
// Inherits from FFastArraySerializerItem so the container can delta-replicate
// only the changed entries each network frame.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FMahlanyaAbilitySpec : public FFastArraySerializerItem
{
    GENERATED_BODY()

    /** Unique ability identifier, e.g. "Ability.UmshizaThrow". */
    UPROPERTY()
    FName AbilityID;

    /** Power level of this ability (1 = base; higher = ritual mastery). */
    UPROPERTY()
    int32 Level = 1;

    /** True while the ability is actively executing on the server. */
    UPROPERTY()
    bool bIsActive = false;
};

// ---------------------------------------------------------------------------
// FMahlanyaAbilityContainer
// FFastArraySerializer wrapper that holds all granted ability specs and
// produces delta-compressed replication payloads automatically.
// ---------------------------------------------------------------------------
USTRUCT()
struct FMahlanyaAbilityContainer : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FMahlanyaAbilitySpec> Items;

    /**
     * Called by the replication system to produce / consume delta payloads.
     * Forwards to the templated FastArrayDeltaSerialize helper.
     */
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FMahlanyaAbilitySpec, FMahlanyaAbilityContainer>(
            Items, DeltaParms, *this);
    }
};

/** Required UE5 trait so the engine knows FMahlanyaAbilityContainer uses delta serialization. */
template<>
struct TStructOpsTypeTraits<FMahlanyaAbilityContainer> : public TStructOpsTypeTraitsBase2<FMahlanyaAbilityContainer>
{
    enum { WithNetDeltaSerializer = true };
};

// ---------------------------------------------------------------------------
// UMahlanyaAbilityComponent
//
// Actor component that manages a character's set of granted Swazi abilities.
// Abilities are stored in FMahlanyaAbilityContainer and replicated to clients
// via FFastArraySerializer delta compression.
//
// Authority (server) is the only endpoint allowed to mutate ability state;
// activation is gated behind a Server-reliable RPC.
// ---------------------------------------------------------------------------
UCLASS(ClassGroup=(Mahlanya), meta=(BlueprintSpawnableComponent))
class MAHLANYARPG_API UMahlanyaAbilityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMahlanyaAbilityComponent();

    // -----------------------------------------------------------------------
    // Ability Management
    // -----------------------------------------------------------------------

    /**
     * Grant an ability to the owning character.
     * Must be called on the server (authority).
     * @return false if the ability was already granted (no duplicate entries).
     */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Abilities")
    bool GrantAbility(FName AbilityID, int32 Level = 1);

    /**
     * Remove a previously granted ability.
     * Must be called on the server (authority).
     * @return false if the ability was not found.
     */
    UFUNCTION(BlueprintCallable, Category = "Mahlanya|Abilities")
    bool RevokeAbility(FName AbilityID);

    /**
     * Server RPC — activates an ability by setting bIsActive = true on the spec.
     * The FFastArraySerializer replicates only the changed entry to clients.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Mahlanya|Abilities")
    void ServerActivateAbility(FName AbilityID);

    // -----------------------------------------------------------------------
    // Queries (safe to call on any connection)
    // -----------------------------------------------------------------------

    /** Returns true if the character has been granted the specified ability. */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Abilities")
    bool HasAbility(FName AbilityID) const;

    /**
     * Returns the Level of the specified ability, or -1 if not granted.
     */
    UFUNCTION(BlueprintPure, Category = "Mahlanya|Abilities")
    int32 GetAbilityLevel(FName AbilityID) const;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // -----------------------------------------------------------------------
    // Replicated state
    // -----------------------------------------------------------------------

    /** Delta-replicated ability set. Only diffs are sent over the network. */
    UPROPERTY(Replicated)
    FMahlanyaAbilityContainer Abilities;

    /**
     * Sibling effects component; resolved at BeginPlay via GetOwner().
     * Allows ability activation to apply game-effects (e.g. stamina drain).
     * Not replicated — both server and client resolve it locally.
     */
    UPROPERTY()
    TObjectPtr<UMahlanyaEffectsComponent> Effects;

private:
    /** Internal helper: find a spec by AbilityID; returns nullptr if absent. */
    FMahlanyaAbilitySpec* FindSpec(FName AbilityID);
    const FMahlanyaAbilitySpec* FindSpec(FName AbilityID) const;
};
