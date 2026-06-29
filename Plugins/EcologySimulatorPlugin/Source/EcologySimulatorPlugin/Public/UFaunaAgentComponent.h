#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UFaunaAgentComponent.generated.h"

UCLASS(ClassGroup=("Ecology"), meta=(BlueprintSpawnableComponent))
class ECOLOGYSIMULATORPLUGIN_API UFaunaAgentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SpeciesID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsHerd = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HerdSize = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsMigratory = false;

    UFUNCTION(BlueprintCallable, Category="Fauna Agent")
    void UpdateMigration(float GameDayOfYear, const FVector& WaterSourceLocation);
};
