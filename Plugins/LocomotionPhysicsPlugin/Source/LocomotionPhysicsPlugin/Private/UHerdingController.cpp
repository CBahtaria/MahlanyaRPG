#include "UHerdingController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

// ── UHerdingController ────────────────────────────────────────────────────────

UHerdingController::UHerdingController()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UHerdingController::AddCattleAgent(AActor* CattleActor, float Mass_kg)
{
    if (!CattleActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("UHerdingController::AddCattleAgent — null actor, skipping."));
        return;
    }

    FCattleAgent NewAgent;
    NewAgent.Actor    = CattleActor;
    NewAgent.Mass_kg  = Mass_kg;
    NewAgent.Velocity = FVector::ZeroVector;

    CattleAgents.Add(NewAgent);

    UE_LOG(LogTemp, Log, TEXT("UHerdingController: Added cattle agent '%s'. Herd size: %d"),
           *CattleActor->GetName(), CattleAgents.Num());
}

void UHerdingController::RemoveCattleAgent(AActor* CattleActor)
{
    const int32 RemovedCount = CattleAgents.RemoveAll([CattleActor](const FCattleAgent& Agent)
    {
        return Agent.Actor == CattleActor;
    });

    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("UHerdingController: Removed cattle agent '%s'. Herd size: %d"),
               *CattleActor->GetName(), CattleAgents.Num());
    }
}

void UHerdingController::ApplySteeringForce(FVector Origin, FVector Direction, float Magnitude)
{
    for (FCattleAgent& Agent : CattleAgents)
    {
        if (!Agent.Actor)
        {
            continue;
        }

        const float Dist = FVector::Dist(Origin, Agent.Actor->GetActorLocation());
        if (Dist < RepulsionRadius_cm)
        {
            // Closer cattle feel stronger repulsion; max impulse of 800 cm/s
            const float RepulsionStrength = Magnitude * (1.f - Dist / RepulsionRadius_cm) * 800.f;
            // Push cattle away from the force direction (repulsion)
            Agent.Velocity += -Direction * RepulsionStrength;
        }
    }

    if (HerdState == EHerdState::Calm)
    {
        HerdState = EHerdState::Agitated;
        UE_LOG(LogTemp, Log, TEXT("UHerdingController: Herd state -> Agitated (steering force applied)."));
    }
}

void UHerdingController::UpdateAggregateMomentum()
{
    float TotalMomentum = 0.f;
    for (const FCattleAgent& Agent : CattleAgents)
    {
        TotalMomentum += Agent.Mass_kg * Agent.Velocity.Size();
    }
    AggregateMomentum = TotalMomentum;
}

void UHerdingController::DampenVelocities(float DeltaTime)
{
    float DragFactor = 0.f;

    switch (HerdState)
    {
        case EHerdState::Calm:
            DragFactor = 2.f;       // Strong drag — cattle quickly decelerate
            break;
        case EHerdState::Agitated:
            DragFactor = 0.5f;      // Light drag — momentum lingers
            break;
        case EHerdState::Stampeding:
            DragFactor = 0.1f;      // Almost no drag — stampede sustains itself
            break;
    }

    for (FCattleAgent& Agent : CattleAgents)
    {
        Agent.Velocity *= (1.f - DragFactor * DeltaTime);
    }
}

void UHerdingController::CheckStampedeTransitions()
{
    const float TickDelta = LastDeltaTime;

    // Agitated + momentum surpasses threshold → begin stampede
    if (HerdState == EHerdState::Agitated && AggregateMomentum > StampedeThreshold)
    {
        HerdState     = EHerdState::Stampeding;
        StampedeTimer = 0.f;
        OnStampede.Broadcast();
        UE_LOG(LogTemp, Warning, TEXT("UHerdingController: STAMPEDE BEGIN — momentum=%.0f kg·cm/s"), AggregateMomentum);
    }

    // Stampeding — advance timer and check if herd has calmed
    if (HerdState == EHerdState::Stampeding)
    {
        StampedeTimer += TickDelta;

        if (StampedeTimer > 15.f && AggregateMomentum < StampedeThreshold * 0.3f)
        {
            HerdState = EHerdState::Calm;
            OnHerdCalmed.Broadcast();
            UE_LOG(LogTemp, Log, TEXT("UHerdingController: Stampede ended — herd calmed after %.1f s."), StampedeTimer);
            StampedeTimer = 0.f;
        }
    }
}

void UHerdingController::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    LastDeltaTime = DeltaTime;

    // 1 + 2. Sync each agent's velocity back to its actor
    for (FCattleAgent& Agent : CattleAgents)
    {
        if (!Agent.Actor)
        {
            continue;
        }

        // Attempt to drive velocity through a PawnMovementComponent
        APawn* OwningPawn = Cast<APawn>(Agent.Actor);
        if (OwningPawn)
        {
            UPawnMovementComponent* MoveComp = OwningPawn->GetMovementComponent();
            if (MoveComp)
            {
                MoveComp->Velocity = Agent.Velocity;
                continue;
            }
        }

        // Fallback: directly offset the actor by velocity * DeltaTime
        Agent.Actor->AddActorWorldOffset(Agent.Velocity * DeltaTime, true);
    }

    // 3. Apply drag model
    DampenVelocities(DeltaTime);

    // 4. Recalculate aggregate momentum
    UpdateAggregateMomentum();

    // 5. Evaluate stampede state transitions
    CheckStampedeTransitions();
}
