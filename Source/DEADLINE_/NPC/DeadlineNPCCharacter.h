// Copyright DEADLINE. All Rights Reserved.
//
// Base class for every walking NPC: staff (GDD 10.4), state inspectors
// (GDD 7.9) and ambient pedestrians. The mesh is any SK_Deadline_Human
// character and the animation is ABP_NPC_Base, so a new character is a new
// Blueprint child with a different mesh and nothing else
// (DEADLINE_Karakter_Pipeline.md 1.1).
//
// Movement belongs to ADeadlineNPCController. This class only holds what a
// level designer sets per placed NPC: where it walks and how long it lingers.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DeadlineNPCCharacter.generated.h"

UCLASS()
class DEADLINE__API ADeadlineNPCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADeadlineNPCCharacter();

	/** Walked in order, then from the start again. Leave empty and the NPC
	    wanders the NavMesh around where it was placed instead. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Deadline|NPC")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	/** Seconds spent standing at each point before moving on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|NPC", meta = (ClampMin = "0"))
	float PauseAtPoint = 2.f;

	/** How far from home a wandering NPC picks its next spot, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|NPC", meta = (ClampMin = "100"))
	float WanderRadius = 1500.f;
};
