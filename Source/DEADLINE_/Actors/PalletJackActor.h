// Copyright DEADLINE. All Rights Reserved.
//
// The pallet jack (transpalet). GDD 5.2 says pallets and crates move "sadece
// forklift/transpalet" — never in your arms.
//
// Why a jack and not a driveable forklift. GDD 21 lists manual vehicle driving
// as out of scope and GDD 9.1 gives the reason: for one developer a driveable
// vehicle is three times the estimate, and it is drudgery after the fifth hour.
// The deciding argument is not the player's though, it is the automation ladder
// (GDD 10.3 step 4): everything the player does here, a hired hand does in
// Month 5. A walking worker with a jack is an AIController and a NavMesh, which
// the engine gives you. A driving worker is warehouse vehicle navigation —
// turning circles, reversing, fork alignment — written twice, once for the
// player and once for the AI. That is the class of problem GDD 9.1 already
// killed once for the city.
//
// So the jack is a tool you take, not a vehicle you board. It attaches in front
// of the player, slows them down, and turns Pallet and Crate from "needs a
// forklift" into "carryable". The pallet on it is the same AContainerActor the
// rest of the game already moves, so storing, loading and selling need no new
// paths.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/Interactable.h"
#include "PalletJackActor.generated.h"

class ADeadlinePlayerCharacter;
class UStaticMeshComponent;

UCLASS()
class DEADLINE__API APalletJackActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	APalletJackActor();

	/** Who is pushing it, or null when it is parked. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|PalletJack")
	ADeadlinePlayerCharacter* GetHolder() const { return Holder; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|PalletJack")
	bool IsHeld() const { return Holder != nullptr; }

	/** Where a pallet sits once it is on the forks. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|PalletJack")
	USceneComponent* GetLoadSocket() const { return LoadSocket; }

	/**
	 * Redraw the boxes sitting on the forks.
	 *
	 * One instance per container, at the container's real size, rather than
	 * one block that grows: the player is counting how many more will fit, and
	 * a single stretched cube does not answer that. Same reasoning as the rack
	 * stacks (GDD 10.1) and the same technique -- instances, not actors.
	 *
	 * @param Load  what is on the forks, or null for nothing.
	 */
	void RefreshLoadVisual(const class AContainerActor* Load);

	/** Attach to the player. Refused if they are already holding something. */
	bool Take(ADeadlinePlayerCharacter* Character);

	/** Park it where the player is standing. Refused while a pallet is on it:
	    letting go of a loaded jack in the middle of the floor would strand the
	    goods somewhere no ledger knows about. */
	bool Park(ADeadlinePlayerCharacter* Character);

	// IInteractable
	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor) const override;
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual void Interact_Implementation(APawn* Interactor) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|PalletJack")
	TObjectPtr<USceneComponent> Root;

	/** Greybox: a low body and two forks. Replaced with a real mesh in Month 9. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|PalletJack")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|PalletJack")
	TObjectPtr<UStaticMeshComponent> ForkMesh;

	/** The crossbar at the top, which is what you actually spot from across
	    the warehouse. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|PalletJack")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	/** Sits just above the forks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|PalletJack")
	TObjectPtr<USceneComponent> LoadSocket;

	/** The boxes on the forks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|PalletJack")
	TObjectPtr<class UInstancedStaticMeshComponent> LoadMesh;

private:
	UPROPERTY()
	TObjectPtr<ADeadlinePlayerCharacter> Holder;
};
