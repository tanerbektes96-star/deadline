// Copyright DEADLINE. All Rights Reserved.
//
// Walks an ADeadlineNPCCharacter around the NavMesh: its patrol points in
// order with a pause at each, or random reachable spots near home when it has
// none. This is the "walking worker is an AIController and a NavMesh" of the
// automation ladder (GDD 10.3 step 4, PalletJackActor.h); the jobs themselves
// (carry, store, load) come later and will steer this same controller.
//
// No Tick (CLAUDE.md tick rules): the next move starts from OnMoveCompleted,
// and the pause is a one-shot timer.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DeadlineNPCController.generated.h"

class ADeadlineNPCCharacter;

UCLASS()
class DEADLINE__API ADeadlineNPCController : public AAIController
{
	GENERATED_BODY()

public:
	ADeadlineNPCController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

	/** How close counts as arrived, in cm. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|NPC")
	float AcceptanceRadius = 50.f;

	/** Wait before trying again when a point cannot be reached. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|NPC")
	float RetryDelay = 1.f;

private:
	void MoveToNext();
	void WaitThenMove(float Seconds);

	UPROPERTY(Transient)
	TObjectPtr<ADeadlineNPCCharacter> NPC;

	FVector Home = FVector::ZeroVector;
	int32 NextPoint = 0;
	FTimerHandle WaitTimer;
};
