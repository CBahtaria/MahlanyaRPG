#include "ULightningSystem.h"
#include "UMicroclimateSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

ULightningSystem::ULightningSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void ULightningSystem::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CachedSubsystem && GetWorld())
    {
        CachedSubsystem = GetWorld()->GetSubsystem<UMicroclimateSubsystem>();
    }

    const bool bStormActive = CachedSubsystem
        ? CachedSubsystem->GetCurrentState().bThunderstormActive
        : false;

    AccumulateCharge(DeltaTime / 60.f, bStormActive);

    if (CurrentCharge >= StrikeThreshold)
    {
        FireStrike();
        CurrentCharge = 0.f;
    }
}

void ULightningSystem::AccumulateCharge(float GameMinuteDelta, bool bStormActive)
{
    if (bStormActive)
        CurrentCharge += ChargeAccumulationRate * GameMinuteDelta;
    else
        CurrentCharge = FMath::Max(0.f, CurrentCharge - 0.01f * GameMinuteDelta);

    CurrentCharge = FMath::Clamp(CurrentCharge, 0.f, 1.f);
}

void ULightningSystem::FireStrike()
{
    if (!GetWorld()) return;

    FVector StrikeLocation = FVector::ZeroVector;

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            const FVector PlayerLoc = Pawn->GetActorLocation();
            StrikeLocation = FVector(
                PlayerLoc.X + FMath::RandRange(-200000.f, 200000.f),
                PlayerLoc.Y + FMath::RandRange(-200000.f, 200000.f),
                PlayerLoc.Z + 10000.f  // start above terrain
            );
        }
    }

    // Line trace down to terrain surface
    FHitResult Hit;
    const FVector TraceEnd = StrikeLocation - FVector(0, 0, 20000.f);
    if (GetWorld()->LineTraceSingleByChannel(Hit, StrikeLocation, TraceEnd,
                                              ECC_WorldStatic))
    {
        StrikeLocation = Hit.Location;
    }

    OnLightningStrike.Broadcast(StrikeLocation, false);
}

void ULightningSystem::SetTreeState(AActor* Tree, ELightningTreeState State)
{
    if (Tree)
        TreeStates.Add(Tree, State);
}

ELightningTreeState ULightningSystem::GetTreeState(AActor* Tree) const
{
    if (const ELightningTreeState* State = TreeStates.Find(Tree))
        return *State;
    return ELightningTreeState::Normal;
}
