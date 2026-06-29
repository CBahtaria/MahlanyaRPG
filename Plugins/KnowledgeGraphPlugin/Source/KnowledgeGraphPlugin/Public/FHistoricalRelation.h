// Copyright (c) 2024 cbartaria1. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FHistoricalRelation.generated.h"

/**
 * Represents a directed relationship between two historical entities.
 * Examples: "mswati_ii" --"succeeded"--> "sobhuza_i"
 *           "battle_of_lubuya" --"fought_at"--> "lubuya_river"
 */
USTRUCT(BlueprintType)
struct KNOWLEDGEGRAPHPLUGIN_API FHistoricalRelation
{
    GENERATED_BODY()

    /** The source entity ID. */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    FName FromEntity;

    /** The relation type (e.g., "succeeded", "fought_at", "allied_with", "ruled"). */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    FName Relation;

    /** The target entity ID. */
    UPROPERTY(BlueprintReadOnly, Category = "Knowledge Graph")
    FName ToEntity;
};
