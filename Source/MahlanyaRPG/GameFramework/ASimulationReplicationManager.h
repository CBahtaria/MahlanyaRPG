// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "SimulationTrustMatrix.h"
#include "ASimulationReplicationManager.generated.h"

// ── Bandwidth token bucket ────────────────────────────────────────────────────
// Prevents full-sync RPCs from saturating the simulation bandwidth budget.
// Server_RequestFullSync_Validate() consumes tokens; Tick refills them.
struct FBandwidthBucket
{
    float Tokens      = 0.f;
    float MaxTokens   = 0.f;
    float RefillRate  = 0.f;  // bytes/sec
    double LastRefillTime = 0.0;

    void  Initialize(float MaxBytes, float RefillBytesPerSec);
    bool  Consume(float BytesRequested);
    void  Refill();
};

// ── Delegates ─────────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrustScoreChanged, float, NewScore);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnValidationError,
    FName, PropertyName, EValidationSeverity, Severity);

// ── ASimulationReplicationManager ─────────────────────────────────────────────

UCLASS()
class MAHLANYARPG_API ASimulationReplicationManager : public AActor
{
    GENERATED_BODY()

public:
    ASimulationReplicationManager();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ── Server RPCs ───────────────────────────────────────────────────────────

    // Client requests a full property sync from server.
    // Bandwidth-gated: validate() returns false if token budget is exhausted.
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RequestFullSync();

    // ── Client RPCs ──────────────────────────────────────────────────────────

    // Server pushes a batch of property name/value pairs to the client.
    UFUNCTION(Client, Reliable)
    void Client_ReceivePropertyBatch(
        const TArray<FName>& Props,
        const TArray<float>& Values,
        int32 Tick);

    // ── Delegates ────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category="Simulation|Trust")
    FOnTrustScoreChanged OnTrustScoreChanged;

    UPROPERTY(BlueprintAssignable, Category="Simulation|Trust")
    FOnValidationError OnValidationError;

    // ── Replicated state ─────────────────────────────────────────────────────

    // Incremented server-side each time a batch is pushed; used by client
    // to detect drift between the tick counter and local extrapolation state.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Simulation|Trust")
    int32 ServerTickCounter = 0;

    // Lightweight heartbeat: current atmospheric pressure replicated every tick.
    // Client uses delta from last known to detect whether the server is still live.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Simulation|Trust")
    float LastBroadcastPressureHPa = 1013.25f;

private:
    // Client-side trust matrix (not replicated directly).
    FSimulationTrustMatrix ClientMatrix;

    // Bandwidth budget for full-sync RPCs (server-side only).
    FBandwidthBucket SimulationBandwidthBucket;

    void CollectAndSendSnapshot();
};
