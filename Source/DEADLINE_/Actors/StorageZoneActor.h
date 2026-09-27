// Copyright DEADLINE. All Rights Reserved.
//
// A place in the warehouse where goods live: a run of racks, a pallet bay, a
// cold zone or a caged rack (GDD 11). The class is picked per placed actor and
// decides what the zone will take — a Cold Tote is refused by a plain rack even
// when the rack is empty.
//
// This is where the ledger and the physical world meet: storing a box calls
// AddStock with the box's own white/grey flag, so a grey box quietly grows the
// discrepancy an inspection will one day find (GDD 7.1).
//
// The stack you see is an Instanced Static Mesh, not one actor per box
// (GDD 10.1): the ledger says how many containers of what are in this class,
// and the zone draws that many instances. Nothing is stored per box, so the
// stack survives save/load for free.
//
// Empty-handed, a pallet bay does something different from a rack: you cannot
// carry a pallet or a crate, so E opens a pallet into 16 boxes or closes 16
// boxes back into a pallet (GDD 10.2).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inventory/StorageClass.h"
#include "Player/Interactable.h"
#include "StorageZoneActor.generated.h"

class AContainerActor;
class UInstancedStaticMeshComponent;
class UInventorySubsystem;
class UStaticMeshComponent;

UCLASS()
class DEADLINE__API AStorageZoneActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AStorageZoneActor();

	/** Which of the four GDD 11 storage classes this zone is. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Storage")
	EStorageClass StorageClass = EStorageClass::Rack;

	/** Which product an empty-handed interaction acts on. If None, the first
	    product in this class that has stock is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Storage")
	FName WithdrawProductID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Storage")
	TSubclassOf<AContainerActor> ContainerClass;

	// --- Visible stack ------------------------------------------------------

	/** First container of the class this zone draws. Set it when a second zone
	    of the same class shows the rest of the shelf. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Storage|Stack", meta = (ClampMin = "0"))
	int32 DisplayStartSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Storage|Stack", meta = (ClampMin = "1"))
	int32 SlotColumns = 4;

	/** Shelf levels going up, or rows going back on a floor zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Storage|Stack", meta = (ClampMin = "1"))
	int32 SlotLevels = 3;

	/** Centre-to-centre spacing in cm: X is across, Y is up on a shelf and
	    back on a floor zone. Keep it at least as wide as the box, or the
	    stacks interpenetrate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Storage|Stack")
	FVector2D SlotSpacing = FVector2D(60.f, 65.f);

	/** Nudge for the whole stack, in cm. Slot 0 is worked out from the zone
	    footprint on its own: boxes stand on the floor in front of a shelf, and
	    on top of a floor zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Storage|Stack")
	FVector SlotOrigin = FVector::ZeroVector;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	int32 GetSlotCount() const { return FMath::Max(1, SlotColumns) * FMath::Max(1, SlotLevels); }

	/** Product an empty-handed interaction would take out. Racks, cold zones
	    and caged racks only. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	FName ResolveWithdrawProduct() const;

	/** Pallet an empty-handed interaction would open. Pallet bays only. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	FName ResolvePalletToOpen() const;

	/** Pallet an empty-handed interaction would close, when none can be opened. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	FName ResolvePalletToClose() const;

	// IInteractable
	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor) const override;
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual void Interact_Implementation(APawn* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	bool StoreCarried(class ADeadlinePlayerCharacter* Character);

public:
	/** F: hand the whole load over to this zone (GDD 3.3). Public because the
	    player character drives it, not the focus interaction. */
	bool TryStoreFrom(class ADeadlinePlayerCharacter* Character);

	/** What F would say here, empty when it would do nothing. */
	FText GetStorePrompt(class ADeadlinePlayerCharacter* Character) const;

protected:
	bool WithdrawOne(ADeadlinePlayerCharacter* Character);

	/** Could one more of what this zone stocks go onto the load the player is
	    already pushing? This is what makes E mean "take" even with full hands. */
	bool CanAddToLoad(const class ADeadlinePlayerCharacter* Character) const;

	/** Container type for a product, BoxM if the catalogue does not know it. */
	EContainerType ContainerTypeOf(FName ProductID) const;

	/** What could be rebuilt if there were a spare pallet. Only used to tell
	    "nothing to do here" apart from "you are out of pallets". */
	FName ResolvePalletToCloseIgnoringPallets() const;

	/** Room for this box, asking the right question for a whole container and
	    for a single box off an opened pallet. */
	bool HasRoomFor(const AContainerActor& Box) const;

	/** True when the resolved product only sits in this class as loose boxes —
	    a pallet product showing up on the racks (GDD 10.2). */
	bool IsWithdrawLoose(FName ProductID) const;

	UInventorySubsystem* GetInventory() const;

	/** Redraw the stack from the ledger. Cheap enough to run on every change,
	    which is why nothing polls it. */
	void RefreshStack();

	/** Ledger callbacks. The signatures are what the delegates require. */
	UFUNCTION()
	void HandleStockChanged(FName ProductID);

	UFUNCTION()
	void HandleCapacityChanged();

	/** Greybox footprint for the class, in metres. */
	FVector ClassFootprintMetres() const;

	/** Size the interaction box to the zone's footprint, chest high. */
	void RefreshInteractionVolume();

	/** True for a zone you stack on top of rather than in front of. */
	bool IsFloorZone() const { return StorageClass == EStorageClass::PalletBay; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Storage")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	/** The visible stack. One instance per stored container (GDD 10.1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Storage")
	TObjectPtr<UInstancedStaticMeshComponent> StackMesh;

	/**
	 * What the interaction trace actually hits. Invisible, and the player walks
	 * straight through it.
	 *
	 * The pallet bay is a 15 cm floor pad, so aiming at the mesh meant aiming
	 * at the floor -- fine when you are stood over it empty-handed, useless
	 * when you are behind a pallet jack looking ahead. This is a chest-high box
	 * over the same footprint, so every zone is at eye level whatever shape its
	 * greybox happens to be.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Storage")
	TObjectPtr<class UBoxComponent> InteractionVolume;

	/** Where a withdrawn box appears. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Storage")
	TObjectPtr<USceneComponent> HandoverPoint;
};
