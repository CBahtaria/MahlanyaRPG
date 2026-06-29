#include "ProtocolStateComponent.h"

// ─────────────────────────────────────────────────────────────────────────────
// DefaultProtocols — Swazi cultural protocol definitions
// ─────────────────────────────────────────────────────────────────────────────

// Static initialiser: three canonical protocols matching the design spec.
// Callers can pass individual entries to RegisterProtocol() to activate them.
const TArray<FProtocol> UProtocolStateComponent::DefaultProtocols = []()
{
    TArray<FProtocol> Defaults;

    // PROT_GreetElder
    // Correct performance: crouch + double-clap + verbal greeting "Sawubona"
    {
        FProtocol P;
        P.ID                 = FName("PROT_GreetElder");
        P.Description        = TEXT("Greet an elder correctly: crouch, clap twice, and say 'Sawubona'.");
        P.Status             = EProtocolStatus::Required;
        P.RelationshipImpact = 0.08f;
        Defaults.Add(P);
    }

    // PROT_AddressInkosi
    // Correct performance: remove headgear before speaking + use indirect address forms
    {
        FProtocol P;
        P.ID                 = FName("PROT_AddressInkosi");
        P.Description        = TEXT("Address the Inkosi: remove headgear and use indirect speech forms.");
        P.Status             = EProtocolStatus::Required;
        P.RelationshipImpact = 0.15f;
        Defaults.Add(P);
    }

    // PROT_InhloniphhoLaw
    // Correct performance: use substitute (hlonipha) vocabulary when near in-laws
    {
        FProtocol P;
        P.ID                 = FName("PROT_InhloniphhoLaw");
        P.Description        = TEXT("Observe inhlonipho law: use substitute vocabulary in the presence of in-laws.");
        P.Status             = EProtocolStatus::Optional;
        P.RelationshipImpact = 0.05f;
        Defaults.Add(P);
    }

    return Defaults;
}();

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

UProtocolStateComponent::UProtocolStateComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    CurrentStateID = NAME_None;
}

// ─────────────────────────────────────────────────────────────────────────────
// TickComponent
// ─────────────────────────────────────────────────────────────────────────────

void UProtocolStateComponent::TickComponent(
    float                        DeltaTime,
    ELevelTick                   TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    TickState(DeltaTime);
}

// ─────────────────────────────────────────────────────────────────────────────
// RegisterProtocol
// ─────────────────────────────────────────────────────────────────────────────

void UProtocolStateComponent::RegisterProtocol(const FProtocol& Protocol)
{
    // Replace any existing entry with the same ID
    for (FProtocol& Existing : ActiveProtocols)
    {
        if (Existing.ID == Protocol.ID)
        {
            Existing = Protocol;
            UE_LOG(LogTemp, Verbose, TEXT("UProtocolStateComponent: Replaced protocol '%s'."), *Protocol.ID.ToString());
            return;
        }
    }

    ActiveProtocols.Add(Protocol);
    UE_LOG(LogTemp, Verbose, TEXT("UProtocolStateComponent: Registered protocol '%s'."), *Protocol.ID.ToString());
}

// ─────────────────────────────────────────────────────────────────────────────
// ObserveProtocol
// ─────────────────────────────────────────────────────────────────────────────

void UProtocolStateComponent::ObserveProtocol(FName ProtocolID)
{
    FProtocol* Protocol = FindProtocol(ProtocolID);
    if (!Protocol)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UProtocolStateComponent: ObserveProtocol — unknown protocol '%s'."),
            *ProtocolID.ToString());
        return;
    }

    // Idempotent: skip if already observed
    if (Protocol->Status == EProtocolStatus::Observed)
    {
        return;
    }

    Protocol->Status = EProtocolStatus::Observed;
    AccumulatedRelationshipImpact += Protocol->RelationshipImpact;  // positive delta

    UE_LOG(LogTemp, Log,
        TEXT("UProtocolStateComponent: Protocol '%s' Observed (+%.2f relationship)."),
        *ProtocolID.ToString(), Protocol->RelationshipImpact);
}

// ─────────────────────────────────────────────────────────────────────────────
// ViolateProtocol
// ─────────────────────────────────────────────────────────────────────────────

void UProtocolStateComponent::ViolateProtocol(FName ProtocolID)
{
    FProtocol* Protocol = FindProtocol(ProtocolID);
    if (!Protocol)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UProtocolStateComponent: ViolateProtocol — unknown protocol '%s'."),
            *ProtocolID.ToString());
        return;
    }

    // Idempotent: skip if already violated (or already correctly observed)
    if (Protocol->Status == EProtocolStatus::Violated || Protocol->Status == EProtocolStatus::Observed)
    {
        return;
    }

    Protocol->Status = EProtocolStatus::Violated;
    AccumulatedRelationshipImpact -= Protocol->RelationshipImpact;  // negate → negative delta

    UE_LOG(LogTemp, Log,
        TEXT("UProtocolStateComponent: Protocol '%s' Violated (−%.2f relationship)."),
        *ProtocolID.ToString(), Protocol->RelationshipImpact);
}

// ─────────────────────────────────────────────────────────────────────────────
// GetTotalRelationshipImpact
// ─────────────────────────────────────────────────────────────────────────────

float UProtocolStateComponent::GetTotalRelationshipImpact() const
{
    // Recompute from source of truth rather than trusting the accumulator alone,
    // so this function is safe to call from Blueprint at any point.
    float Total = 0.f;
    for (const FProtocol& P : ActiveProtocols)
    {
        if (P.Status == EProtocolStatus::Observed)
        {
            Total += P.RelationshipImpact;
        }
        else if (P.Status == EProtocolStatus::Violated)
        {
            Total -= P.RelationshipImpact;
        }
    }
    return Total;
}

// ─────────────────────────────────────────────────────────────────────────────
// ClearProtocols
// ─────────────────────────────────────────────────────────────────────────────

void UProtocolStateComponent::ClearProtocols()
{
    ActiveProtocols.Empty();
    AccumulatedRelationshipImpact = 0.f;
    UE_LOG(LogTemp, Verbose, TEXT("UProtocolStateComponent: All protocols cleared."));
}

// ─────────────────────────────────────────────────────────────────────────────
// EnterState
// ─────────────────────────────────────────────────────────────────────────────

void UProtocolStateComponent::EnterState(FName StateID)
{
    // Exit the current state first (interrupted, not completed)
    if (CurrentStateID != NAME_None)
    {
        ExitCurrentState(false);
    }

    CurrentStateID = StateID;

    FProtocolState* NewState = States.Find(StateID);
    if (!NewState)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UProtocolStateComponent: EnterState — state '%s' not registered."),
            *StateID.ToString());
        return;
    }

    if (NewState->OnEnter.IsBound())
    {
        NewState->OnEnter.Execute();
    }

    UE_LOG(LogTemp, Verbose, TEXT("UProtocolStateComponent: Entered state '%s'."), *StateID.ToString());
}

// ─────────────────────────────────────────────────────────────────────────────
// ExitCurrentState
// ─────────────────────────────────────────────────────────────────────────────

void UProtocolStateComponent::ExitCurrentState(bool bCompleted)
{
    if (CurrentStateID == NAME_None)
    {
        return;
    }

    FProtocolState* State = States.Find(CurrentStateID);
    if (State && State->OnExit.IsBound())
    {
        State->OnExit.Execute(bCompleted);
    }

    UE_LOG(LogTemp, Verbose,
        TEXT("UProtocolStateComponent: Exited state '%s' (completed=%s)."),
        *CurrentStateID.ToString(), bCompleted ? TEXT("true") : TEXT("false"));

    CurrentStateID = NAME_None;
}

// ─────────────────────────────────────────────────────────────────────────────
// TickState
// ─────────────────────────────────────────────────────────────────────────────

void UProtocolStateComponent::TickState(float DeltaTime)
{
    if (CurrentStateID == NAME_None)
    {
        return;
    }

    FProtocolState* State = States.Find(CurrentStateID);
    if (State && State->OnTick.IsBound())
    {
        State->OnTick.Execute(DeltaTime);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// FindProtocol (private)
// ─────────────────────────────────────────────────────────────────────────────

FProtocol* UProtocolStateComponent::FindProtocol(FName ProtocolID)
{
    for (FProtocol& P : ActiveProtocols)
    {
        if (P.ID == ProtocolID)
        {
            return &P;
        }
    }
    return nullptr;
}
