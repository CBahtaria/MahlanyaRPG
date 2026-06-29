// Copyright Charles Bartaria. All Rights Reserved.

#include "MahlanyaCoopBridgeComponent.h"
#include "MahlanyaGameState.h"
#include "Kismet/GameplayStatics.h"

UMahlanyaCoopBridgeComponent::UMahlanyaCoopBridgeComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = PollInterval;
}

void UMahlanyaCoopBridgeComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UWorld* World = GetWorld())
    {
        CachedGameState = World->GetGameState<AMahlanyaGameState>();
    }
}

void UMahlanyaCoopBridgeComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    PollGameState();
}

void UMahlanyaCoopBridgeComponent::PollGameState()
{
    if (!CachedGameState)
    {
        return;
    }

    // Fire weather delegate when the replicated pressure value changes —
    // this drives client-side LocomotionPhysicsPlugin wet-friction and
    // GeometricAudioPlugin atmospheric propagation without any server RPC.
    const float CurrentPressure = CachedGameState->WeatherState.PressureHPa;
    if (!FMath::IsNearlyEqual(CurrentPressure, LastPressure, 0.01f))
    {
        LastPressure = CurrentPressure;
        OnWeatherStateReceived.Broadcast(CachedGameState->WeatherState);
    }

    const int32 CurrentYear = CachedGameState->CurrentGameYear;
    if (CurrentYear != LastGameYear)
    {
        LastGameYear = CurrentYear;
        OnGameYearChanged.Broadcast(CurrentYear);
    }
}
