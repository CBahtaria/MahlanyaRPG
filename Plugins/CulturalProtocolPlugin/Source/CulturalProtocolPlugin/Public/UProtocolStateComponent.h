#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FProtocol.h"
#include "UProtocolStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProtocolOutcome, FName, ProtocolID, bool, bObserved);

UCLASS(ClassGroup=("CulturalProtocol"), meta=(BlueprintSpawnableComponent))
class CULTURALPROTOCOLPLUGIN_API UProtocolStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable) FOnProtocolOutcome OnProtocolOutcome;

    UFUNCTION(BlueprintCallable, Category="Protocol")
    bool LoadProtocols(const FString& JsonPath);

    UFUNCTION(BlueprintCallable, Category="Protocol")
    FName ActivateProtocol(const FString& TriggerCondition);

    UFUNCTION(BlueprintCallable, Category="Protocol")
    void RecordGesture(const FString& Gesture);

    UFUNCTION(BlueprintCallable, Category="Protocol")
    void RecordWord(const FString& Word);

    UFUNCTION(BlueprintCallable, Category="Protocol")
    float ResolveActiveProtocol();

    UFUNCTION(BlueprintCallable, Category="Protocol")
    FProtocol GetProtocol(FName ProtocolID) const;

    UFUNCTION(BlueprintCallable, Category="Protocol")
    float GetCumulativeRelationshipDelta() const { return CumulativeDelta; }

private:
    TArray<FProtocol> Protocols;
    FName ActiveProtocolID = NAME_None;
    TArray<FString> PerformedGestures;
    TArray<FString> SpokenWords;
    float CumulativeDelta = 0.f;

    const FProtocol* FindByTrigger(const FString& TriggerCondition) const;
    bool HasAllRequirements(const FProtocol& Protocol) const;
};
