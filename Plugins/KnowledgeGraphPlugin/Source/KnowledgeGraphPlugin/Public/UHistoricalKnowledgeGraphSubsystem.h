// Copyright (c) 2024 cbartaria1. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FHistoricalEntity.h"
#include "FHistoricalRelation.h"
#include "UHistoricalKnowledgeGraphSubsystem.generated.h"

/**
 * World subsystem that owns the in-memory Swazi historical knowledge graph.
 *
 * The graph is loaded from Content/History/knowledge_graph.ndjson during world
 * initialization. NPC dialogue systems and event validators query this subsystem
 * at runtime to ensure period-accurate responses.
 *
 * Usage (C++):
 *   UHistoricalKnowledgeGraphSubsystem* KG =
 *       GetWorld()->GetSubsystem<UHistoricalKnowledgeGraphSubsystem>();
 *   if (KG && KG->IsLoaded())
 *   {
 *       FHistoricalEntity Entity = KG->GetEntity(FName("mswati_ii"));
 *   }
 */
UCLASS()
class KNOWLEDGEGRAPHPLUGIN_API UHistoricalKnowledgeGraphSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** UWorldSubsystem interface — loads the graph from the default path. */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * Load (or reload) the graph from an NDJSON file at the given absolute path.
     * Clears any previously loaded data first.
     * Returns true if at least one entity was successfully parsed.
     * Safe to call from tests or editor utilities with a custom path.
     */
    UFUNCTION(BlueprintCallable, Category = "Knowledge Graph")
    bool LoadGraph(const FString& NdjsonPath);

    /**
     * Look up an entity by its ID.
     * Returns a default-constructed FHistoricalEntity (empty EntityID) when not found.
     */
    UFUNCTION(BlueprintCallable, Category = "Knowledge Graph")
    FHistoricalEntity GetEntity(FName EntityID) const;

    /**
     * Return all relations involving EntityID (as source or target).
     * Pass NAME_None as RelType to return every relation regardless of type.
     * Pass a specific type (e.g., "allied_with") to filter.
     */
    UFUNCTION(BlueprintCallable, Category = "Knowledge Graph")
    TArray<FHistoricalRelation> GetRelationsOf(FName EntityID, FName RelType) const;

    /**
     * Returns true if the entity referenced by EntityID was in existence during InGameYear.
     * Branching logic per EntityType:
     *   Person  — BornYear <= InGameYear <= DiedYear
     *   Place   — Period[0] <= InGameYear <= Period[1] (if Period has 2 elements), else true
     *   Event / Battle — Period[0] <= InGameYear <= Period[1] (if available),
     *                    else BornYear <= InGameYear
     *   default — true (always in existence)
     */
    UFUNCTION(BlueprintCallable, Category = "Knowledge Graph")
    bool ExistedDuring(FName EntityID, int32 InGameYear) const;

    /**
     * Returns the entity's Title if it existed during InGameYear,
     * otherwise returns Title + " (anachronistic)".
     */
    UFUNCTION(BlueprintCallable, Category = "Knowledge Graph")
    FString GetPeriodAccurateTitle(FName PersonID, int32 InGameYear) const;

    /**
     * Returns the IDs of all entities that had an "allied_with" relation with
     * ClanOrPersonID and also existed during InGameYear.
     */
    UFUNCTION(BlueprintCallable, Category = "Knowledge Graph")
    TArray<FName> GetAlliesOf(FName ClanOrPersonID, int32 InGameYear) const;

    /** True after a successful LoadGraph call that parsed at least one entity. */
    bool IsLoaded() const { return bLoaded; }

private:
    /** Primary lookup table: EntityID -> entity data. */
    TMap<FName, FHistoricalEntity> Entities;

    /** All directed relations in the graph. */
    TArray<FHistoricalRelation> Relations;

    /** Set to true once the graph has been loaded with at least one valid entity. */
    bool bLoaded = false;

    /** Returns the platform-absolute path to the bundled NDJSON data file. */
    static FString DefaultGraphPath();
};
