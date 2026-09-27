// Copyright DEADLINE. All Rights Reserved.
//
// Where a row of DT_Destinations actually is. One of these per place the player
// can travel to; arriving means being put down at its two points.
//
// It is a marker, not a place with rules: the counters, the terminal and the
// grey window are their own actors that happen to stand nearby. Keeping the
// arrival point separate from what you do when you get there means a level can
// be rearranged without touching the travel table.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestinationActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class DEADLINE__API ADestinationActor : public AActor
{
	GENERATED_BODY()

public:
	ADestinationActor();

	/** Row key into DT_Destinations. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Travel")
	FName DestinationID;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	FVector GetVehicleParkLocation() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	FRotator GetVehicleParkRotation() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	FVector GetPlayerArrivalLocation() const;

protected:
	/** Greybox pad, so the spot is visible while the city is grey. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Travel")
	TObjectPtr<UStaticMeshComponent> PadMesh;

	/** Where the truck ends up. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Travel")
	TObjectPtr<USceneComponent> VehicleParkPoint;

	/** Where the player ends up: beside the truck, not inside it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Travel")
	TObjectPtr<USceneComponent> PlayerArrivalPoint;
};
