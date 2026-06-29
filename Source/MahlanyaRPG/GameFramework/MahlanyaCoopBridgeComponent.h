// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MahlanyaCoopBridgeComponent.generated.h"

class AMahlanyaGameState;

/**
 * Attached to the player pawn.  Bridges the replicated simulation state in
 * AMahlanyaGameState (server-authoritative) with per-player systems that need
 * to react to weather, economy, and calendar changes.
 *
 * Pattern: client reads GameState snapshots; component fires local delegates
 * that Blueprint (and simulation plugins already loaded on clients) subscribe
 * to.  No client runs simulation maths — it only observes replicated state.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnWeatherStateReceived, const FReplicatedWeatherState&, NewWeather);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnGameYearChanged, int32, NewYear);

UCLASS(ClassGroup=(MahlanyaRPG), meta=(BlueprintSpawnableComponent))
class MAHLANYARPG_API UMahlanyaCoopBridgeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMahlanyaCoopBridgeComponent();

    virtual void BeginPlay() override;
    virtual void TickComponent(
        float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    // Fired whenever the replicated WeatherState snapshot changes.
    UPROPERTY(BlueprintAssignable, Category="Mahlanya|Co-op")
    FOnWeatherStateReceived OnWeatherStateReceived;

    // Fired whenever CurrentGameYear advances.
    UPROPERTY(BlueprintAssignable, Category="Mahlanya|Co-op")
    FOnGameYearChanged OnGameYearChanged;

private:
    void PollGameState();

    UPROPERTY()
    TObjectPtr<AMahlanyaGameState> CachedGameState;

    float LastPressure    = -1.f;
    int32 LastGameYear    = -1;
    float PollInterval    = 0.5f;  // seconds between snapshot polls
    float TimeSincePoll   = 0.f;
};
