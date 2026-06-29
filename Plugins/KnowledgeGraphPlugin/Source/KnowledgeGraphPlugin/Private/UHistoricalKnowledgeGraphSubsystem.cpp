// Copyright (c) 2024 cbartaria1. All Rights Reserved.

#include "UHistoricalKnowledgeGraphSubsystem.h"
#include "FHistoricalGraphQuery.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ---------------------------------------------------------------------------
// UWorldSubsystem interface
// ---------------------------------------------------------------------------

void UHistoricalKnowledgeGraphSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Attempt to load from the default path. Silently ignore failure — the
    // NDJSON file may not be present yet in editor or during cook.
    LoadGraph(DefaultGraphPath());
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool UHistoricalKnowledgeGraphSubsystem::LoadGraph(const FString& NdjsonPath)
{
    // Clear previous state regardless of outcome.
    Entities.Empty();
    Relations.Empty();
    bLoaded = false;

    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *NdjsonPath))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UHistoricalKnowledgeGraphSubsystem: Could not read graph file: %s"),
            *NdjsonPath);
        return false;
    }

    int32 EntityCount = 0;

    for (const FString& Line : Lines)
    {
        // Skip blank / comment lines.
        if (Line.TrimStartAndEnd().IsEmpty())
        {
            continue;
        }

        FHistoricalEntity Entity;
        if (FHistoricalGraphQuery::ParseEntityLine(Line, Entity))
        {
            Entities.Add(Entity.EntityID, Entity);
            ++EntityCount;
            continue;
        }

        FHistoricalRelation Relation;
        if (FHistoricalGraphQuery::ParseRelationLine(Line, Relation))
        {
            Relations.Add(Relation);
        }
    }

    bLoaded = (EntityCount > 0);

    UE_LOG(LogTemp, Log,
        TEXT("UHistoricalKnowledgeGraphSubsystem: Loaded %d entities and %d relations from %s"),
        EntityCount, Relations.Num(), *NdjsonPath);

    return bLoaded;
}

FHistoricalEntity UHistoricalKnowledgeGraphSubsystem::GetEntity(FName EntityID) const
{
    // FindRef returns a default-constructed value when key is absent.
    return Entities.FindRef(EntityID);
}

TArray<FHistoricalRelation> UHistoricalKnowledgeGraphSubsystem::GetRelationsOf(
    FName EntityID, FName RelType) const
{
    TArray<FHistoricalRelation> Result;

    for (const FHistoricalRelation& Rel : Relations)
    {
        const bool bMatchesEntity = (Rel.FromEntity == EntityID || Rel.ToEntity == EntityID);
        if (!bMatchesEntity)
        {
            continue;
        }

        // NAME_None means "return all relation types".
        if (RelType != NAME_None && Rel.Relation != RelType)
        {
            continue;
        }

        Result.Add(Rel);
    }

    return Result;
}

bool UHistoricalKnowledgeGraphSubsystem::ExistedDuring(FName EntityID, int32 InGameYear) const
{
    const FHistoricalEntity* EntityPtr = Entities.Find(EntityID);
    if (!EntityPtr)
    {
        return false;
    }

    const FHistoricalEntity& Entity = *EntityPtr;

    if (Entity.EntityType == TEXT("Person"))
    {
        return Entity.BornYear <= InGameYear && InGameYear <= Entity.DiedYear;
    }
    else if (Entity.EntityType == TEXT("Place"))
    {
        if (Entity.Period.Num() >= 2)
        {
            return Entity.Period[0] <= InGameYear && InGameYear <= Entity.Period[1];
        }
        // No period data — assume the place always existed.
        return true;
    }
    else if (Entity.EntityType == TEXT("Event") || Entity.EntityType == TEXT("Battle"))
    {
        if (Entity.Period.Num() >= 2)
        {
            return Entity.Period[0] <= InGameYear && InGameYear <= Entity.Period[1];
        }
        // Fall back to BornYear as the single year field for instantaneous events.
        return Entity.BornYear <= InGameYear;
    }

    // For "Item" and any unknown types, assume always present once introduced.
    return true;
}

FString UHistoricalKnowledgeGraphSubsystem::GetPeriodAccurateTitle(
    FName PersonID, int32 InGameYear) const
{
    const FHistoricalEntity Entity = GetEntity(PersonID);

    if (ExistedDuring(PersonID, InGameYear))
    {
        return Entity.Title;
    }

    return Entity.Title + TEXT(" (anachronistic)");
}

TArray<FName> UHistoricalKnowledgeGraphSubsystem::GetAlliesOf(
    FName ClanOrPersonID, int32 InGameYear) const
{
    static const FName AlliedWithRelation(TEXT("allied_with"));

    TArray<FName> Allies;

    for (const FHistoricalRelation& Rel : Relations)
    {
        if (Rel.Relation != AlliedWithRelation)
        {
            continue;
        }

        FName AllyID = NAME_None;

        if (Rel.FromEntity == ClanOrPersonID)
        {
            AllyID = Rel.ToEntity;
        }
        else if (Rel.ToEntity == ClanOrPersonID)
        {
            AllyID = Rel.FromEntity;
        }

        if (AllyID == NAME_None)
        {
            continue;
        }

        // Only include the ally if it was in existence at the given game year.
        if (ExistedDuring(AllyID, InGameYear))
        {
            Allies.AddUnique(AllyID);
        }
    }

    return Allies;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

FString UHistoricalKnowledgeGraphSubsystem::DefaultGraphPath()
{
    return FPaths::ProjectContentDir() / TEXT("History/knowledge_graph.ndjson");
}
