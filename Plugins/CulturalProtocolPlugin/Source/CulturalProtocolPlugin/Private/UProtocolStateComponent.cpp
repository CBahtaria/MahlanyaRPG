#include "UProtocolStateComponent.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool UProtocolStateComponent::LoadProtocols(const FString& JsonPath)
{
    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *JsonPath)) return false;

    TArray<TSharedPtr<FJsonValue>> Arr;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Arr)) return false;

    Protocols.Empty();
    for (const TSharedPtr<FJsonValue>& Val : Arr)
    {
        const TSharedPtr<FJsonObject>& Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;

        FProtocol P;
        P.ProtocolID        = FName(*Obj->GetStringField(TEXT("id")));
        P.DisplayName       = Obj->GetStringField(TEXT("display_name"));
        P.Description       = Obj->GetStringField(TEXT("description"));
        P.TriggerCondition  = Obj->GetStringField(TEXT("trigger_condition"));
        P.RelationshipImpact = (float)Obj->GetNumberField(TEXT("relationship_impact"));
        P.FailConsequence   = Obj->GetStringField(TEXT("fail_consequence"));

        const TArray<TSharedPtr<FJsonValue>>* Gestures;
        if (Obj->TryGetArrayField(TEXT("required_gestures"), Gestures))
            for (const auto& G : *Gestures) P.RequiredGestures.Add(G->AsString());

        const TArray<TSharedPtr<FJsonValue>>* Words;
        if (Obj->TryGetArrayField(TEXT("required_words"), Words))
            for (const auto& W : *Words) P.RequiredWords.Add(W->AsString());

        FString StatusStr = Obj->GetStringField(TEXT("status"));
        if (StatusStr == TEXT("Optional")) P.Status = EProtocolStatus::Optional;
        else P.Status = EProtocolStatus::Required;

        Protocols.Add(P);
    }
    return Protocols.Num() > 0;
}

FName UProtocolStateComponent::ActivateProtocol(const FString& TriggerCondition)
{
    const FProtocol* Found = FindByTrigger(TriggerCondition);
    if (!Found) return NAME_None;
    ActiveProtocolID = Found->ProtocolID;
    PerformedGestures.Empty();
    SpokenWords.Empty();
    return ActiveProtocolID;
}

void UProtocolStateComponent::RecordGesture(const FString& Gesture)
{
    PerformedGestures.AddUnique(Gesture);
}

void UProtocolStateComponent::RecordWord(const FString& Word)
{
    SpokenWords.AddUnique(Word);
}

float UProtocolStateComponent::ResolveActiveProtocol()
{
    if (ActiveProtocolID == NAME_None) return 0.f;

    const FProtocol* ProtPtr = nullptr;
    for (const FProtocol& P : Protocols)
        if (P.ProtocolID == ActiveProtocolID) { ProtPtr = &P; break; }

    if (!ProtPtr) return 0.f;

    const bool bAllMet = HasAllRequirements(*ProtPtr);
    const bool bOptional = (ProtPtr->Status == EProtocolStatus::Optional);

    float Delta = 0.f;
    if (bAllMet || bOptional)
        Delta = ProtPtr->RelationshipImpact;
    else if (ProtPtr->Status == EProtocolStatus::Required)
        Delta = -ProtPtr->RelationshipImpact;

    CumulativeDelta += Delta;
    OnProtocolOutcome.Broadcast(ActiveProtocolID, bAllMet);
    ActiveProtocolID = NAME_None;
    return Delta;
}

FProtocol UProtocolStateComponent::GetProtocol(FName ProtocolID) const
{
    for (const FProtocol& P : Protocols)
        if (P.ProtocolID == ProtocolID) return P;
    return FProtocol{};
}

const FProtocol* UProtocolStateComponent::FindByTrigger(const FString& TriggerCondition) const
{
    for (const FProtocol& P : Protocols)
        if (P.TriggerCondition == TriggerCondition) return &P;
    return nullptr;
}

bool UProtocolStateComponent::HasAllRequirements(const FProtocol& Protocol) const
{
    for (const FString& G : Protocol.RequiredGestures)
        if (!PerformedGestures.Contains(G)) return false;
    for (const FString& W : Protocol.RequiredWords)
        if (!SpokenWords.Contains(W)) return false;
    return true;
}
