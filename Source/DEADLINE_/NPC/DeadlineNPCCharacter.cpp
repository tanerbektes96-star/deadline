// Copyright DEADLINE. All Rights Reserved.

#include "NPC/DeadlineNPCCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "NPC/DeadlineNPCController.h"

ADeadlineNPCCharacter::ADeadlineNPCCharacter()
{
	AIControllerClass = ADeadlineNPCController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Face where you walk, not where the controller looks.
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.f, 360.f, 0.f);

	// BS_Locomotion puts the walk cycle at 300 cm/s; any other speed slides
	// the feet. Faster NPCs belong in the jog sample (600), not in between.
	Move->MaxWalkSpeed = 300.f;

	// ABP_NPC_Base only leaves idle when there is acceleration as well as
	// speed. Path following sets velocity directly and leaves acceleration at
	// zero unless it is told to steer through acceleration, which would have
	// the NPC glide across the floor in its idle pose.
	Move->GetNavMovementProperties()->bUseAccelerationForPaths = true;
}
