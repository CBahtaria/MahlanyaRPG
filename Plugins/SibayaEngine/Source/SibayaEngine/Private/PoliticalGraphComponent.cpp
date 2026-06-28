#include "PoliticalGraphComponent.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

UPoliticalGraphComponent::UPoliticalGraphComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UPoliticalGraphComponent::LoadFromFile(const FString& FilePath)
{
    FString FileContents;
    if (!FFileHelper::LoadFileToString(FileContents, *FilePath))
    {
        UE_LOG(LogTemp, Warning, TEXT("UPoliticalGraphComponent::LoadFromFile — could not read file: %s"), *FilePath);
        return false;
    }

    AdjacencyList.Empty();
    SettlementPositions.Empty();

    TArray<FString> Lines;
    FileContents.ParseIntoArrayLines(Lines, /*bCullEmpty=*/true);

    int32 ParsedCount = 0;
    for (const FString& Line : Lines)
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);

        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("UPoliticalGraphComponent::LoadFromFile — failed to parse JSON line: %s"), *Line);
            continue;
        }

        FSettlementRelation Relation;

        // Parse "from" field
        FString FromStr;
        if (JsonObject->TryGetStringField(TEXT("from"), FromStr))
        {
            Relation.From.ID = FName(*FromStr);
        }
        else
        {
            continue;  // "from" is mandatory
        }

        // Parse "to" field
        FString ToStr;
        if (JsonObject->TryGetStringField(TEXT("to"), ToStr))
        {
            Relation.To.ID = FName(*ToStr);
        }
        else
        {
            continue;  // "to" is mandatory
        }

        // Parse "relation" field
        FString RelationStr;
        if (JsonObject->TryGetStringField(TEXT("relation"), RelationStr))
        {
            Relation.RelationType = FName(*RelationStr);
        }

        // Parse "cattle_tribute" field
        int32 Tribute = 0;
        JsonObject->TryGetNumberField(TEXT("cattle_tribute"), Tribute);
        Relation.CattleTribute = Tribute;

        // Parse "distance_km" field
        double DistKm = 0.0;
        JsonObject->TryGetNumberField(TEXT("distance_km"), DistKm);
        Relation.DistanceKm = static_cast<float>(DistKm);

        // Parse optional settlement world positions
        // Expected format: "from_position": [x, y, z]
        const TArray<TSharedPtr<FJsonValue>>* FromPosArray = nullptr;
        if (JsonObject->TryGetArrayField(TEXT("from_position"), FromPosArray) &&
            FromPosArray && FromPosArray->Num() >= 3)
        {
            FVector Pos(
                static_cast<float>((*FromPosArray)[0]->AsNumber()),
                static_cast<float>((*FromPosArray)[1]->AsNumber()),
                static_cast<float>((*FromPosArray)[2]->AsNumber()));
            SettlementPositions.FindOrAdd(Relation.From) = Pos;
        }

        const TArray<TSharedPtr<FJsonValue>>* ToPosArray = nullptr;
        if (JsonObject->TryGetArrayField(TEXT("to_position"), ToPosArray) &&
            ToPosArray && ToPosArray->Num() >= 3)
        {
            FVector Pos(
                static_cast<float>((*ToPosArray)[0]->AsNumber()),
                static_cast<float>((*ToPosArray)[1]->AsNumber()),
                static_cast<float>((*ToPosArray)[2]->AsNumber()));
            SettlementPositions.FindOrAdd(Relation.To) = Pos;
        }

        // Add to adjacency list
        AdjacencyList.FindOrAdd(Relation.From).Add(Relation);
        ++ParsedCount;
    }

    UE_LOG(LogTemp, Log, TEXT("UPoliticalGraphComponent::LoadFromFile — loaded %d relations from %s"),
           ParsedCount, *FilePath);
    return ParsedCount > 0;
}

TArray<FSettlementRelation> UPoliticalGraphComponent::GetAlliesOf(FSettlementID SettlementID) const
{
    TArray<FSettlementRelation> Allies;

    const TArray<FSettlementRelation>* Edges = AdjacencyList.Find(SettlementID);
    if (Edges)
    {
        for (const FSettlementRelation& Relation : *Edges)
        {
            if (Relation.RelationType == FName(TEXT("ally")))
            {
                Allies.Add(Relation);
            }
        }
    }

    return Allies;
}

int32 UPoliticalGraphComponent::GetCattleTribute(FSettlementID From, FSettlementID To) const
{
    const TArray<FSettlementRelation>* Edges = AdjacencyList.Find(From);
    if (Edges)
    {
        for (const FSettlementRelation& Relation : *Edges)
        {
            if (Relation.To == To)
            {
                return Relation.CattleTribute;
            }
        }
    }

    return 0;
}

TArray<FVector> UPoliticalGraphComponent::GetTradeRoutePath(FSettlementID From, FSettlementID To) const
{
    // Full A* on heightmap gradient deferred to Phase 4 terrain integration.
    // For now return a straight-line two-point path.
    TArray<FVector> Path;

    const FVector* FromPos = SettlementPositions.Find(From);
    const FVector* ToPos   = SettlementPositions.Find(To);

    if (FromPos)
    {
        Path.Add(*FromPos);
    }
    if (ToPos)
    {
        Path.Add(*ToPos);
    }

    return Path;
}

bool UPoliticalGraphComponent::IsInFeud(FSettlementID A, FSettlementID B) const
{
    const FName FeudName(TEXT("feud"));

    // Check A→B
    const TArray<FSettlementRelation>* EdgesA = AdjacencyList.Find(A);
    if (EdgesA)
    {
        for (const FSettlementRelation& Relation : *EdgesA)
        {
            if (Relation.To == B && Relation.RelationType == FeudName)
            {
                return true;
            }
        }
    }

    // Check B→A
    const TArray<FSettlementRelation>* EdgesB = AdjacencyList.Find(B);
    if (EdgesB)
    {
        for (const FSettlementRelation& Relation : *EdgesB)
        {
            if (Relation.To == A && Relation.RelationType == FeudName)
            {
                return true;
            }
        }
    }

    return false;
}

void UPoliticalGraphComponent::UpsertRelation(const FSettlementRelation& Relation)
{
    TArray<FSettlementRelation>& Edges = AdjacencyList.FindOrAdd(Relation.From);

    // Update existing edge if one exists between the same pair, otherwise append
    for (FSettlementRelation& Existing : Edges)
    {
        if (Existing.To == Relation.To)
        {
            Existing = Relation;
            return;
        }
    }

    Edges.Add(Relation);
}
