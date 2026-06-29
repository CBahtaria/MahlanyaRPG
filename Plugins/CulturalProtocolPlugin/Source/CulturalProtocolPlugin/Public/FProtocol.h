#pragma once
#include "CoreMinimal.h"
#include "FProtocol.generated.h"

UENUM(BlueprintType)
enum class EProtocolStatus : uint8
{
    Required  UMETA(DisplayName="Required"),
    Optional  UMETA(DisplayName="Optional"),
    Violated  UMETA(DisplayName="Violated"),
    Observed  UMETA(DisplayName="Observed"),
};

USTRUCT(BlueprintType)
struct CULTURALPROTOCOLPLUGIN_API FProtocol
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName ProtocolID;
    UPROPERTY(BlueprintReadOnly) FString DisplayName;
    UPROPERTY(BlueprintReadOnly) FString Description;
    UPROPERTY(BlueprintReadOnly) FString TriggerCondition;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RequiredGestures;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RequiredWords;
    UPROPERTY(BlueprintReadOnly) float RelationshipImpact = 0.f;
    UPROPERTY(BlueprintReadOnly) EProtocolStatus Status = EProtocolStatus::Required;
    UPROPERTY(BlueprintReadOnly) FString FailConsequence;
};
