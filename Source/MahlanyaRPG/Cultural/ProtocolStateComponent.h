#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProtocolStateComponent.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// EProtocolStatus
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Lifecycle status of a single cultural protocol obligation within one NPC interaction.
 */
UENUM(BlueprintType)
enum class EProtocolStatus : uint8
{
    Required  UMETA(DisplayName = "Required"),   // Must be performed in this interaction
    Optional  UMETA(DisplayName = "Optional"),   // Socially expected but not mandatory
    Violated  UMETA(DisplayName = "Violated"),   // Player failed to observe
    Observed  UMETA(DisplayName = "Observed"),   // Player correctly performed
};

// ─────────────────────────────────────────────────────────────────────────────
// FProtocol
// ─────────────────────────────────────────────────────────────────────────────

/**
 * A single cultural protocol (e.g. PROT_GreetElder, PROT_AddressInkosi).
 *
 * Pre-loaded defaults are stored in UProtocolStateComponent::DefaultProtocols.
 * Relationship impact is applied:
 *   - Positively when Status == Observed
 *   - Negatively (negated) when Status == Violated
 */
USTRUCT(BlueprintType)
struct FProtocol
{
    GENERATED_BODY()

    /** Unique identifier, e.g. "PROT_GreetElder". */
    UPROPERTY(EditAnywhere, Category = "Protocol")
    FName ID;

    /** Human-readable description for UI and debug display. */
    UPROPERTY(EditAnywhere, Category = "Protocol")
    FString Description;

    /** Current lifecycle status within the active interaction. */
    UPROPERTY(BlueprintReadOnly, Category = "Protocol")
    EProtocolStatus Status = EProtocolStatus::Optional;

    /**
     * Relationship delta applied when correctly Observed (+) or Violated (−).
     * Example values: +0.08 for PROT_GreetElder, −0.15 if violated.
     * The component negates this value automatically on violation.
     */
    UPROPERTY(EditAnywhere, Category = "Protocol")
    float RelationshipImpact = 0.f;
};

// ─────────────────────────────────────────────────────────────────────────────
// FProtocolState
// ─────────────────────────────────────────────────────────────────────────────

/**
 * One state in the protocol state machine.
 *
 * Lifecycle mirrors the gdquest-demos/godot-open-rpg pattern:
 *   OnEnter → (OnTick each frame) → OnExit(bCompleted)
 *
 * Bind delegates before calling EnterState().
 */
USTRUCT()
struct FProtocolState
{
    GENERATED_BODY()

    /** Identifier matching the key in UProtocolStateComponent::States. */
    UPROPERTY()
    FName StateID;

    /** Called once when the state machine enters this state. */
    TDelegate<void()>      OnEnter;

    /** Called each component tick while this state is active. */
    TDelegate<void(float)> OnTick;

    /**
     * Called once when the state machine leaves this state.
     * @param bCompleted  True if the state was exited via normal completion,
     *                    false if interrupted (e.g. interaction cancelled).
     */
    TDelegate<void(bool)>  OnExit;
};

// ─────────────────────────────────────────────────────────────────────────────
// UProtocolStateComponent
// ─────────────────────────────────────────────────────────────────────────────

/**
 * UProtocolStateComponent
 *
 * Manages the set of cultural protocols that apply during an NPC interaction
 * and drives a lightweight protocol state machine (EnterState / TickState / ExitCurrentState).
 *
 * Pattern source: gdquest-demos/godot-open-rpg state machine with
 * OnEnter / OnTick / OnExit lifecycle delegates.
 *
 * Swazi cultural protocols implemented:
 *   PROT_GreetElder      — crouch + clap + "Sawubona"       → +0.08 relationship
 *   PROT_AddressInkosi   — remove headgear + indirect speech → +0.15 relationship
 *   PROT_InhloniphhoLaw  — substitute vocabulary near in-laws → +0.05 relationship
 */
UCLASS(ClassGroup = (Mahlanya), meta = (BlueprintSpawnableComponent))
class UProtocolStateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UProtocolStateComponent();

    // ── Protocol registry ─────────────────────────────────────────────────────

    /**
     * Register a protocol that applies within the current NPC interaction.
     * If a protocol with the same ID already exists it is replaced.
     */
    UFUNCTION(BlueprintCallable, Category = "Protocol|Registry")
    void RegisterProtocol(const FProtocol& Protocol);

    /**
     * Mark a protocol as correctly Observed and apply its positive
     * relationship impact to the owning actor.
     *
     * @param ProtocolID  Must match an ID previously passed to RegisterProtocol.
     */
    UFUNCTION(BlueprintCallable, Category = "Protocol|Registry")
    void ObserveProtocol(FName ProtocolID);

    /**
     * Mark a protocol as Violated and apply a negative relationship impact.
     * Typically called at interaction end for Required protocols that were never Observed.
     *
     * @param ProtocolID  Must match an ID previously passed to RegisterProtocol.
     */
    UFUNCTION(BlueprintCallable, Category = "Protocol|Registry")
    void ViolateProtocol(FName ProtocolID);

    /**
     * Returns the signed sum of RelationshipImpact across all Observed and
     * Violated protocols in the current interaction.
     */
    UFUNCTION(BlueprintPure, Category = "Protocol|Registry")
    float GetTotalRelationshipImpact() const;

    /**
     * Clears all active protocols.
     * Call this when an interaction ends (dialogue closed, NPC dismissed, etc.).
     */
    UFUNCTION(BlueprintCallable, Category = "Protocol|Registry")
    void ClearProtocols();

    // ── State machine ─────────────────────────────────────────────────────────

    /**
     * Transition into a named protocol state.
     * Calls ExitCurrentState(false) on the outgoing state, then OnEnter on the new one.
     */
    void EnterState(FName StateID);

    /**
     * Exit the currently active state.
     * @param bCompleted  Passed through to the state's OnExit delegate.
     */
    void ExitCurrentState(bool bCompleted);

    /**
     * Forward the tick to the active state's OnTick delegate.
     * Called automatically by TickComponent.
     */
    void TickState(float DeltaTime);

    // ── UActorComponent interface ─────────────────────────────────────────────

    virtual void TickComponent(
        float                        DeltaTime,
        ELevelTick                   TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // ── Pre-defined cultural protocols ────────────────────────────────────────

    /**
     * Default protocol definitions shipped with the component.
     * Populate ActiveProtocols by calling RegisterProtocol with entries from this array.
     */
    static const TArray<FProtocol> DefaultProtocols;

protected:
    /** Protocols registered for the current interaction. */
    UPROPERTY()
    TArray<FProtocol> ActiveProtocols;

    /** Named states available to the protocol state machine. */
    TMap<FName, FProtocolState> States;

    /** ID of the currently active state, or NAME_None if idle. */
    FName CurrentStateID;

private:
    /**
     * Accumulated signed relationship impact across the current interaction.
     * Updated by ObserveProtocol and ViolateProtocol.
     */
    float AccumulatedRelationshipImpact = 0.f;

    /** Find a mutable pointer to a protocol by ID, or nullptr. */
    FProtocol* FindProtocol(FName ProtocolID);
};
