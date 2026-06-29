// Copyright (c) 2024 cbartaria1. All Rights Reserved.

#include "FHistoricalGraphQuery.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

bool FHistoricalGraphQuery::ParseEntityLine(const FString& Line, FHistoricalEntity& Out)
{
    if (Line.TrimStartAndEnd().IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);

    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        return false;
    }

    // Entity lines must have an "id" field; relation lines have "from".
    if (!JsonObj->HasField(TEXT("id")))
    {
        return false;
    }

    FHistoricalEntity Entity;

    // Required fields
    Entity.EntityID = FName(*JsonObj->GetStringField(TEXT("id")));

    // Name — optional but expected
    if (JsonObj->HasField(TEXT("name")))
    {
        Entity.Name = JsonObj->GetStringField(TEXT("name"));
    }

    // Entity type
    if (JsonObj->HasField(TEXT("type")))
    {
        Entity.EntityType = JsonObj->GetStringField(TEXT("type"));
    }

    // Title / role
    if (JsonObj->HasField(TEXT("title")))
    {
        Entity.Title = JsonObj->GetStringField(TEXT("title"));
    }

    // Born / died years (persons)
    if (JsonObj->HasField(TEXT("born")))
    {
        Entity.BornYear = static_cast<int32>(JsonObj->GetNumberField(TEXT("born")));
    }
    else
    {
        Entity.BornYear = 0;
    }

    if (JsonObj->HasField(TEXT("died")))
    {
        Entity.DiedYear = static_cast<int32>(JsonObj->GetNumberField(TEXT("died")));
    }
    else
    {
        Entity.DiedYear = 9999;
    }

    // introduced_to_region — for items
    if (JsonObj->HasField(TEXT("introduced_to_region")))
    {
        Entity.IntroducedYear = static_cast<int32>(JsonObj->GetNumberField(TEXT("introduced_to_region")));
    }
    else
    {
        Entity.IntroducedYear = 0;
    }

    // Period array [start, end] — for places, events, battles
    if (JsonObj->HasField(TEXT("period")))
    {
        const TArray<TSharedPtr<FJsonValue>>* PeriodArray = nullptr;
        if (JsonObj->TryGetArrayField(TEXT("period"), PeriodArray) && PeriodArray != nullptr)
        {
            Entity.Period.Empty();
            for (const TSharedPtr<FJsonValue>& Val : *PeriodArray)
            {
                if (Val.IsValid())
                {
                    Entity.Period.Add(static_cast<int32>(Val->AsNumber()));
                }
            }
        }
    }

    // Description
    if (JsonObj->HasField(TEXT("description")))
    {
        Entity.Description = JsonObj->GetStringField(TEXT("description"));
    }

    Out = MoveTemp(Entity);
    return true;
}

bool FHistoricalGraphQuery::ParseRelationLine(const FString& Line, FHistoricalRelation& Out)
{
    if (Line.TrimStartAndEnd().IsEmpty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);

    if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
    {
        return false;
    }

    // Relation lines must have a "from" field; entity lines have "id".
    if (!JsonObj->HasField(TEXT("from")))
    {
        return false;
    }

    FHistoricalRelation Relation;

    Relation.FromEntity = FName(*JsonObj->GetStringField(TEXT("from")));

    if (JsonObj->HasField(TEXT("rel")))
    {
        Relation.Relation = FName(*JsonObj->GetStringField(TEXT("rel")));
    }

    if (JsonObj->HasField(TEXT("to")))
    {
        Relation.ToEntity = FName(*JsonObj->GetStringField(TEXT("to")));
    }

    Out = MoveTemp(Relation);
    return true;
}
