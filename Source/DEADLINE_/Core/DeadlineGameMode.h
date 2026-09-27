// Copyright DEADLINE. All Rights Reserved.
//
// Wires the default pawn and controller together. Deliberately thin: game
// state lives in the GameInstance subsystems so it survives level travel.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DeadlineGameMode.generated.h"

UCLASS()
class DEADLINE__API ADeadlineGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADeadlineGameMode();
};
