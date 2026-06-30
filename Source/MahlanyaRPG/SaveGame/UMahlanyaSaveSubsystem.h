// Copyright Charles Bartaria. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MahlanyaSaveGame.h"
#include "UMahlanyaSaveSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSaveComplete, bool, bSuccess, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoadComplete, bool, bSuccess, int32, SlotIndex);

UCLASS()
class MAHLANYARPG_API UMahlanyaSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Mahlanya|Save")
    void SaveGame(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Save")
    void LoadGame(int32 SlotIndex);

    UFUNCTION(BlueprintCallable, Category="Mahlanya|Save")
    UMahlanyaSaveGame* GetActiveSave() const { return ActiveSave; }

    UPROPERTY(BlueprintAssignable) FOnSaveComplete OnSaveComplete;
    UPROPERTY(BlueprintAssignable) FOnLoadComplete OnLoadComplete;

private:
    UPROPERTY() UMahlanyaSaveGame* ActiveSave = nullptr;

    FString MakeSlotName(int32 SlotIndex) const;
    void    SnapshotWorldState(UMahlanyaSaveGame* Save) const;
    void    RestoreWorldState(const UMahlanyaSaveGame* Save) const;

    UFUNCTION()
    void OnLoadFinished(const FString& SlotName, const int32 UserIndex, USaveGame* SaveGameObject);
};
