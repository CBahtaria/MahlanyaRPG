// Copyright Charles Bartaria. All Rights Reserved.

#include "ASimulationReplicationManager.h"
#include "MahlanyaGameState.h"
#include "Net/UnrealNetwork.h"
#include "HAL/PlatformTime.h"
#include "Performance/MahlanyaPerformanceCVars.h"
#include "Core/MahlanyaLogChannels.h"

// ── FBandwidthBucket ──────────────────────────────────────────────────────────

void FBandwidthBucket::Initialize(float MaxBytes, float RefillBytesPerSec)
{
    MaxTokens      = MaxBytes;
    Tokens         = MaxBytes;
    RefillRate     = RefillBytesPerSec;
    LastRefillTime = FPlatformTime::Seconds();
}

bool FBandwidthBucket::Consume(float BytesRequested)
{
    if (Tokens < BytesRequested)
        return false;
    Tokens -= BytesRequested;
    return true;
}

void FBandwidthBucket::Refill()
{
    const double Now     = FPlatformTime::Seconds();
    const float  Elapsed = static_cast<float>(Now - LastRefillTime);
    Tokens         = FMath::Min(MaxTokens, Tokens + RefillRate * Elapsed);
    LastRefillTime = Now;
}

// ── ASimulationReplicationManager ────────────────────────────────────────────

ASimulationReplicationManager::ASimulationReplicationManager()
{
    bReplicates = true;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = true;
}

void ASimulationReplicationManager::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        // Reserve 30% of the configured net bandwidth for simulation sync RPCs.
        const float BandwidthShare =
            MahlanyaPerformanceCVars::SimulationBandwidthShare.GetValueOnGameThread();
        // 64000 bytes/s * share = bytes/s for simulation; max burst = 1s worth
        const float RefillRate = 64000.f * BandwidthShare;
        SimulationBandwidthBucket.Initialize(RefillRate, RefillRate);
    }
}

void ASimulationReplicationManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority())
    {
        SimulationBandwidthBucket.Refill();
    }
    else
    {
        // Client: advance extrapolation and check trust score
        ClientMatrix.ExtrapolateProperties(ServerTickCounter, DeltaTime);

        const float TrustScore = ClientMatrix.GetOverallTrustScore();
        const float MinTrust   =
            MahlanyaPerformanceCVars::MinTrustScore.GetValueOnGameThread();

        if (TrustScore < MinTrust)
        {
            OnTrustScoreChanged.Broadcast(TrustScore);
            UE_LOG(LogMahlanyaTrust, Verbose,
                   TEXT("Trust score %.2f below threshold %.2f"), TrustScore, MinTrust);
        }
    }
}

void ASimulationReplicationManager::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASimulationReplicationManager, ServerTickCounter);
    DOREPLIFETIME(ASimulationReplicationManager, LastBroadcastPressureHPa);
}

// ── Server_RequestFullSync ────────────────────────────────────────────────────

bool ASimulationReplicationManager::Server_RequestFullSync_Validate()
{
    // Consume ~2KB for a full sync payload; deny if bandwidth budget is exhausted.
    return SimulationBandwidthBucket.Consume(2048.f);
}

void ASimulationReplicationManager::Server_RequestFullSync_Implementation()
{
    CollectAndSendSnapshot();
}

void ASimulationReplicationManager::CollectAndSendSnapshot()
{
    const AMahlanyaGameState* GS =
        GetWorld() ? GetWorld()->GetGameState<AMahlanyaGameState>() : nullptr;
    if (!GS)
        return;

    TArray<FName>  Props;
    TArray<float>  Values;

    const int32 MaxBatch =
        MahlanyaPerformanceCVars::MaxPropertiesPerBatch.GetValueOnGameThread();

    // Weather state
    Props.Add(TEXT("PressureHPa"));
    Values.Add(GS->WeatherState.PressureHPa);
    Props.Add(TEXT("PrecipitationIntensity"));
    Values.Add(GS->WeatherState.PrecipitationIntensity);
    Props.Add(TEXT("WindSpeed_ms"));
    Values.Add(GS->WeatherState.WindSpeed_ms);
    Props.Add(TEXT("WindBearing_deg"));
    Values.Add(GS->WeatherState.WindBearing_deg);
    Props.Add(TEXT("TurbidityParam"));
    Values.Add(GS->WeatherState.TurbidityParam);
    Props.Add(TEXT("FogBaseAltitude_m"));
    Values.Add(GS->WeatherState.FogBaseAltitude_m);

    // Clan snapshots (first N to stay within MaxBatch)
    int32 Added = Props.Num();
    for (const FReplicatedClanSnapshot& Snap : GS->ClanSnapshots)
    {
        if (Added + 2 > MaxBatch)
            break;
        Props.Add(FName(*FString::Printf(TEXT("Clan.%s.Cattle"), *Snap.ClanID.ToString())));
        Values.Add(static_cast<float>(Snap.CattleCount));
        Props.Add(FName(*FString::Printf(TEXT("Clan.%s.Strength"), *Snap.ClanID.ToString())));
        Values.Add(Snap.PoliticalStrength);
        Added += 2;
    }

    ++ServerTickCounter;
    LastBroadcastPressureHPa = GS->WeatherState.PressureHPa;

    Client_ReceivePropertyBatch(Props, Values, ServerTickCounter);
}

// ── Client_ReceivePropertyBatch ───────────────────────────────────────────────

void ASimulationReplicationManager::Client_ReceivePropertyBatch_Implementation(
    const TArray<FName>& Props,
    const TArray<float>& Values,
    int32 Tick)
{
    const int32 Count = FMath::Min(Props.Num(), Values.Num());
    for (int32 i = 0; i < Count; ++i)
    {
        ClientMatrix.UpdateProperty(Props[i], Values[i], Tick);
    }
}
