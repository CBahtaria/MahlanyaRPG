// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

#include "GameFramework/MahlanyaGameMode.h"
#include "Characters/MahlanyaCharacter.h"
#include "GameFramework/MahlanyaPlayerController.h"
#include "GameFramework/MahlanyaGameState.h"

AMahlanyaGameMode::AMahlanyaGameMode()
{
	DefaultPawnClass      = AMahlanyaCharacter::StaticClass();
	PlayerControllerClass = AMahlanyaPlayerController::StaticClass();
	GameStateClass        = AMahlanyaGameState::StaticClass();
}
