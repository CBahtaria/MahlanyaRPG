// Copyright (c) 2024 cbartaria1. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FHistoricalEntity.h"
#include "FHistoricalRelation.h"
#include "FHistoricalGraphQuery.generated.h"

/**
 * Pure static utility struct for parsing NDJSON lines from the knowledge graph data file.
 *
 * Entity lines contain an "id" field.
 * Relation lines contain a "from" field.
 *
 * Not a UCLASS — used directly by UHistoricalKnowledgeGraphSubsystem.
 */
USTRUCT()
struct KNOWLEDGEGRAPHPLUGIN_API FHistoricalGraphQuery
{
    GENERATED_BODY()

    /**
     * Parse a single NDJSON line as a historical entity.
     * Returns true and populates Out if the line is an entity record (has "id" field).
     * Returns false and leaves Out unchanged if the line is a relation record or malformed.
     */
    static bool ParseEntityLine(const FString& Line, FHistoricalEntity& Out);

    /**
     * Parse a single NDJSON line as a historical relation.
     * Returns true and populates Out if the line is a relation record (has "from" field).
     * Returns false and leaves Out unchanged if the line is an entity record or malformed.
     */
    static bool ParseRelationLine(const FString& Line, FHistoricalRelation& Out);
};
