// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MahlanyaGameState.generated.h"

/**
 * Snapshot of simulation state replicated from the server to all clients each
 * game-day tick.  Clients use this read-only snapshot rather than running the
 * simulation themselves — only the server runs UEconomySimulatorSubsystem,
 * USibayaEngineSubsystem, and UMicroclimateSubsystem authoritatively.
 */
USTRUCT(BlueprintType)
struct FReplicatedWeatherState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Replicated)
    float PressureHPa = 1013.25f;

    UPROPERTY(BlueprintReadOnly, Replicated)
    float PrecipitationIntensity = 0.f;   // mm/hr

    UPROPERTY(BlueprintReadOnly, Replicated)
    float WindSpeed_ms = 0.f;

    UPROPERTY(BlueprintReadOnly, Replicated)
    float WindBearing_deg = 0.f;

    UPROPERTY(BlueprintReadOnly, Replicated)
    float TurbidityParam = 0.f;           // 0 = clean Highveld, 1 = dusty Lowveld

    UPROPERTY(BlueprintReadOnly, Replicated)
    float FogBaseAltitude_m = 9999.f;
};

USTRUCT(BlueprintType)
struct FReplicatedClanSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Replicated)
    FName ClanID;

    UPROPERTY(BlueprintReadOnly, Replicated)
    int32 CattleCount = 0;

    UPROPERTY(BlueprintReadOnly, Replicated)
    float PoliticalStrength = 0.5f;

    UPROPERTY(BlueprintReadOnly, Replicated)
    float ColonialPressure = 0.f;
};

UCLASS()
class MAHLANYARPG_API AMahlanyaGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    AMahlanyaGameState();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Called server-side by UMicroclimateSubsystem each tick.
    void ServerUpdateWeather(const FReplicatedWeatherState& NewState);

    // Called server-side by UEconomySimulatorSubsystem each monthly tick.
    void ServerUpdateClanSnapshot(const FReplicatedClanSnapshot& Snapshot);

    // Client-readable snapshots (replicated from server).
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Simulation|Weather")
    FReplicatedWeatherState WeatherState;

    // Per-clan economy snapshot array (replicated, max 32 clans).
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Simulation|Economy")
    TArray<FReplicatedClanSnapshot> ClanSnapshots;

    // In-game year (drives KnowledgeGraph temporal queries on all clients).
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Simulation|Calendar")
    int32 CurrentGameYear = 1820;

    // Advance game year — server only; triggers calendar subsystem events.
    UFUNCTION(BlueprintCallable, Category="Simulation|Calendar",
              meta=(BlueprintProtected))
    void ServerAdvanceYear(int32 Delta = 1);
};
