// Copyright DEADLINE. All Rights Reserved.

#include "NPC/DeadlineNPCController.h"

#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "NPC/DeadlineNPCCharacter.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeadlineNPC, Log, All);

ADeadlineNPCController::ADeadlineNPCController()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADeadlineNPCController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	NPC = Cast<ADeadlineNPCCharacter>(InPawn);
	if (!NPC)
	{
		return;
	}
	Home = NPC->GetActorLocation();
	NextPoint = 0;
	// One frame late on purpose: the NavMesh and the other placed actors are
	// not guaranteed to be ready while the level is still possessing pawns.
	WaitThenMove(0.f);
}

void ADeadlineNPCController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(WaitTimer);
	NPC = nullptr;
	Super::OnUnPossess();
}

void ADeadlineNPCController::MoveToNext()
{
	if (!NPC)
	{
		return;
	}

	EPathFollowingRequestResult::Type Request = EPathFollowingRequestResult::Failed;
	FString Target;

	// Skip points deleted from the level instead of stalling on them.
	NPC->PatrolPoints.RemoveAll([](const TObjectPtr<AActor>& Point) { return Point == nullptr; });

	if (NPC->PatrolPoints.Num() > 0)
	{
		NextPoint %= NPC->PatrolPoints.Num();
		AActor* Point = NPC->PatrolPoints[NextPoint];
		// Advance now, once. A failed request can also come back through
		// OnMoveCompleted, so advancing there as well would skip points.
		++NextPoint;
		Target = Point->GetName();
		Request = MoveToActor(Point, AcceptanceRadius, /*bStopOnOverlap=*/true);
	}
	else
	{
		FNavLocation Spot;
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (Nav && Nav->GetRandomReachablePointInRadius(Home, NPC->WanderRadius, Spot))
		{
			Target = Spot.Location.ToCompactString();
			Request = MoveToLocation(Spot.Location, AcceptanceRadius);
		}
	}

	if (Request == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		WaitThenMove(NPC->PauseAtPoint);
	}
	else if (Request == EPathFollowingRequestResult::Failed)
	{
		// Usually no NavMesh under the NPC or the point: say so once per try
		// rather than silently standing still.
		UE_LOG(LogDeadlineNPC, Warning, TEXT("%s: cannot path to %s, retrying"),
			*NPC->GetName(), Target.IsEmpty() ? TEXT("anything") : *Target);
		WaitThenMove(RetryDelay);
	}
}

void ADeadlineNPCController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	if (!NPC)
	{
		return;
	}
	// A new MoveTo aborts the old one and lands here too; the new move is
	// already running, so there is nothing to schedule.
	if (Result.HasFlag(FPathFollowingResultFlags::NewRequest))
	{
		return;
	}
	WaitThenMove(Result.IsSuccess() ? NPC->PauseAtPoint : RetryDelay);
}

void ADeadlineNPCController::WaitThenMove(float Seconds)
{
	if (Seconds <= 0.f)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ADeadlineNPCController::MoveToNext);
		return;
	}
	GetWorldTimerManager().SetTimer(WaitTimer, this, &ADeadlineNPCController::MoveToNext, Seconds, false);
}
