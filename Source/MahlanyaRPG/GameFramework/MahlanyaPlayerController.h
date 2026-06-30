// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MahlanyaPlayerController.generated.h"

UCLASS()
class MAHLANYARPG_API AMahlanyaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMahlanyaPlayerController();

protected:
	virtual void BeginPlay() override;
};
