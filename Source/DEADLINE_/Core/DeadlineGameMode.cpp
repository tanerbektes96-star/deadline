// Copyright DEADLINE. All Rights Reserved.

#include "Core/DeadlineGameMode.h"

#include "Core/DeadlineHUD.h"
#include "Core/DeadlinePlayerController.h"
#include "Player/DeadlinePlayerCharacter.h"

ADeadlineGameMode::ADeadlineGameMode()
{
	DefaultPawnClass = ADeadlinePlayerCharacter::StaticClass();
	PlayerControllerClass = ADeadlinePlayerController::StaticClass();
	HUDClass = ADeadlineHUD::StaticClass();
}
