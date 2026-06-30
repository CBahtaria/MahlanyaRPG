// Copyright Charles Bartaria. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MahlanyaSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FSaveSlotMeta
{
    GENERATED_BODY()
    UPROPERTY() int32  SlotIndex = 0;
    UPROPERTY() int32  GameYear = 1750;
    UPROPERTY() int32  GameDay = 0;
    UPROPERTY() FString PlayerName;
    UPROPERTY() FDateTime SaveTimestamp;
};

USTRUCT(BlueprintType)
struct FSavedClanState
{
    GENERATED_BODY()
    UPROPERTY() FName  ClanID;
    UPROPERTY() int32  CattleCount = 0;
    UPROPERTY() float  PoliticalStrength = 0.5f;
    UPROPERTY() float  ColonialPressure = 0.f;
};

UCLASS()
class MAHLANYARPG_API UMahlanyaSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() FSaveSlotMeta           Meta;
    UPROPERTY() TArray<FSavedClanState> ClanStates;
    UPROPERTY() TArray<FName>           FiredHistoricalEvents;
    UPROPERTY() int32                   CurrentGameYear = 1750;
    UPROPERTY() int32                   CurrentGameDay = 0;
    UPROPERTY() TMap<FName, int32>      SettlementWiveCounts;  // SettlementID → NumWives (for Voronoi restore)
};
