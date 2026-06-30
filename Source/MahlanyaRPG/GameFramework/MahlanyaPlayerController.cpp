// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "GameFramework/MahlanyaPlayerController.h"

AMahlanyaPlayerController::AMahlanyaPlayerController()
{
	bShowMouseCursor = false;
}

void AMahlanyaPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());
}
