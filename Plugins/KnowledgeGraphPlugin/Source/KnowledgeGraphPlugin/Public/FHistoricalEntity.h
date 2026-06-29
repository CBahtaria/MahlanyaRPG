// Copyright (c) 2024 cbartaria1. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FHistoricalEntity.generated.h"

/**
 * Represents a historical entity in the Swazi knowledge graph.
 * EntityType can be one of: "Person", "Place", "Battle", "Event", "Item".
 */
USTRUCT(BlueprintType)
struct KNOWLEDGEGRAPHPLUGIN_API FHistoricalEntity
{
    GENERATED_BODY()

    /** Unique identifier for this entity (e.g., "mswati_ii", "battle_of_lubuya"). */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    FName EntityID;

    /** Display name of the entity. */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    FString Name;

    /** Type classification: "Person", "Place", "Battle", "Event", or "Item". */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    FString EntityType;

    /** Formal title or role (e.g., "Ngwenyama", "Chief"). */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    FString Title;

    /** Birth year for persons; used as start year for events without a Period array. */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    int32 BornYear = 0;

    /** Death year for persons; defaults to 9999 meaning "still alive / unknown end". */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    int32 DiedYear = 9999;

    /** Year the item was introduced to the region (for EntityType == "Item"). */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    int32 IntroducedYear = 0;

    /** [start, end] year range for places, battles, and events. May be empty. */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    TArray<int32> Period;

    /** Narrative description used to populate NPC dialogue. */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    FString Description;
};
