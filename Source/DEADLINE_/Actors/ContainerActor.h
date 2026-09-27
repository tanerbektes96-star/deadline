// Copyright DEADLINE. All Rights Reserved.
//
// A physical box in the world. One actor = one container of one product.
//
// No physics simulation, ever (GDD 10.1, CLAUDE.md mistake #5). Boxes never
// collide with each other, never topple. When a box goes into a vehicle or
// onto a rack it stops being an actor and becomes an Instanced Static Mesh
// entry — that conversion arrives in Month 3.
//
// The box also carries bRecorded: a box bought on an invoice is white, a box
// bought off the books is grey. Whoever consumes the box must pass that on to
// the inventory ledger.

#pragma once

#include "CoreMinimal.h"
#include "Data/ProductRow.h"
#include "GameFramework/Actor.h"
#include "Player/Interactable.h"
#include "ContainerActor.generated.h"

class UStaticMeshComponent;

/** Fires once a box has finished sliding into its slot. */
DECLARE_DELEGATE_OneParam(FOnContainerSlideFinished, class AContainerActor*);

UCLASS()
class DEADLINE__API AContainerActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AContainerActor();

	/** Row key into DT_Products. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Container")
	FName ProductID;

	/** How many containers this actor represents. One, for now. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Container")
	int32 Containers = 1;

	/** True = on the books (white), false = grey goods (GDD 7.1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Container")
	bool bRecorded = true;

	/** Set from the product row when the box is created. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Container")
	EContainerType ContainerType = EContainerType::BoxM;

	/** True if this is one box off an opened pallet rather than a whole
	    container (GDD 10.2). It is worth a sixteenth of the pallet, so it goes
	    back to the ledger as a loose box and no counter will buy it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Container")
	bool bLooseBox = false;

	/**
	 * Clock reading (UTimeSubsystem total minutes) when these goods became
	 * yours. Carried by the box itself, so age survives the round trip out of
	 * the ledger and back: without it, picking a crate off the shelf and
	 * setting it down again would make it new, and every shelf life in the
	 * game would be infinite for the price of a keypress.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Container")
	double AcquiredAtMinute = 0.0;

	UFUNCTION(BlueprintCallable, Category = "Deadline|Container")
	void InitFromProduct(FName InProductID, bool bInRecorded);

	/** Make this actor one box off an opened pallet of InProductID. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Container")
	void InitAsLooseBox(FName InProductID, bool bInRecorded);

	/**
	 * Put another container of the same goods on the stack.
	 *
	 * This is what the pallet jack is FOR. By hand you carry one box; on the
	 * forks you build a pallet of them and make one trip instead of sixteen.
	 * A stack is still a single actor with a count, so storing, loading and
	 * selling already know what to do with it -- every one of those paths took
	 * a container count from the first day.
	 *
	 * @param AcquiredAt  the age of the goods being added. The stack keeps the
	 *        OLDEST stamp: piling fresh boxes on old ones must not make the old
	 *        ones young again (GDD 5.1).
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Container")
	void AddToStack(int32 Count, double AcquiredAt);

	/** Age in days, read off the clock. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Container")
	float GetAgeDays() const;

	/** Whole days before this box spoils. -1 = these goods never spoil. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Container")
	int32 GetDaysUntilSpoilage() const;

	/** What this box is worth as a fraction of the quote, after obsolescence. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Container")
	float GetConditionMultiplier() const;

	/** Box Units this box occupies. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Container")
	float GetVolumeBU() const;

	/** Movement speed multiplier while this box is in your hands (GDD 3.2). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Container")
	float GetCarrySpeedMultiplier() const { return CarrySpeedMultiplier(ContainerType); }

	/** Crates and loaded pallets need a jack, not hands (GDD 5.2). So does a
	    stack: one box in your arms, more than one only on the forks. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Container")
	bool IsHandCarryable() const { return Containers <= 1 && IsHandCarryable(ContainerType); }

	static float CarrySpeedMultiplier(EContainerType Type);
	static bool IsHandCarryable(EContainerType Type);

	/** Greybox size of a container, in metres. Shared with the rack stacks so
	    a box looks the same in your hands as it does on the shelf. */
	static FVector PlaceholderSizeMetres(EContainerType Type);

	/** Turn collision and focus on or off as the box is picked up / dropped. */
	void SetCarried(bool bCarried);

	// --- Sliding into a slot (GDD 10.1) -------------------------------------

	/**
	 * Glide to a resting place over Duration seconds, then report back.
	 *
	 * This is the one place a container ticks. CLAUDE.md tick rule 6:
	 * conditional tick, on only while the box is in the air, off the frame it
	 * lands. Half a second of movement is what makes a box look handed over
	 * rather than teleported.
	 */
	void StartSlideTo(const FVector& TargetLocation, const FRotator& TargetRotation, float Duration);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Container")
	bool IsSliding() const { return bSliding; }

	/** Bound by whoever asked for the slide. */
	FOnContainerSlideFinished OnSlideFinished;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Container")
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }

	// IInteractable
	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor) const override;
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual void Interact_Implementation(APawn* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Container")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** Scale the placeholder cube gets per container type, in cm (GDD 5.2 /
	    Roadmap 2.2 greybox sizes). Replaced by real meshes in Month 9. */
	void ApplyPlaceholderScale();

private:
	double NowMinute() const;
	const struct FProductRow* FindRow() const;

	bool bSliding = false;
	float SlideElapsed = 0.f;
	float SlideDuration = 0.f;
	FVector SlideFrom = FVector::ZeroVector;
	FVector SlideTo = FVector::ZeroVector;
	FRotator SlideFromRotation = FRotator::ZeroRotator;
	FRotator SlideToRotation = FRotator::ZeroRotator;
};
