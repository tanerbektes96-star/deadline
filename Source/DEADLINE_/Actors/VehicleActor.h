// Copyright DEADLINE. All Rights Reserved.
//
// A truck standing in the world. The vehicle is a VIEW of its cargo, not the
// owner of it — the goods live in UFleetSubsystem, keyed by VehicleKey, so a
// loaded truck survives a save, a level reload and a valuation query.
//
// Loading is the GDD 10.1 flow: stand in the zone behind the bed with a box in
// your hands, press F, and the box leaves your hands and glides into the first
// free slot over half a second. When it lands it stops being an actor and
// becomes one Instanced Static Mesh entry, so a full trailer is one draw call
// and never 320 physics bodies. Empty-handed, F takes the last box back off.
//
// No driving, ever (GDD 9.1). There is no throttle, no collision, no traffic.
// E on the truck opens the travel map instead (GDD 9.2 step 2); the map picks
// the destination and UTravelSubsystem does the rest.

#pragma once

#include "CoreMinimal.h"
#include "Data/VehicleRow.h"
#include "GameFramework/Actor.h"
#include "Player/Interactable.h"
#include "VehicleActor.generated.h"

class AContainerActor;
class ADeadlinePlayerCharacter;
class UBoxComponent;
class UFleetSubsystem;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

UCLASS()
class DEADLINE__API AVehicleActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AVehicleActor();

	/** Identifies this particular truck across saves. Leave it None and the
	    actor's own name is used, which is stable for a level-placed actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Vehicle")
	FName VehicleKey;

	/** Row key into DT_Vehicles: which model this is (GDD 9.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Vehicle")
	FName VehicleID = TEXT("V01");

	/** Centre-to-centre cargo spacing in cm. Column and row counts are worked
	    out from the bed size, so a bigger truck simply gets more slots.
	    Keep it wider than the biggest container the truck carries by hand
	    (Box L is 60 cm): set it to the box width exactly and the load renders
	    as one unbroken slab instead of a stack of boxes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Vehicle|Cargo")
	FVector SlotSpacing = FVector(72.f, 72.f, 70.f);

	/** How long a box takes to glide into its slot (GDD 10.1: half a second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Vehicle|Cargo")
	float SlideSeconds = 0.5f;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Vehicle")
	FName GetVehicleKey() const { return ResolvedKey; }

	/** What the HUD shows while you are standing in the loading zone. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Vehicle")
	FText GetLoadPrompt(APawn* Interactor) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Vehicle")
	bool CanLoadFrom(APawn* Interactor) const;

	/** F with a box in your hands. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Vehicle")
	bool TryLoadFrom(ADeadlinePlayerCharacter* Character);

	/** What E would take off the bed right now, None if nothing. Answers
	    "is the player at the tail with room for what is on it?" */
	FName ResolveTakeProduct(const ADeadlinePlayerCharacter* Character, bool& bOutLoose) const;

	/** Could one more of these goods go onto the stack the player is already
	    pushing? Same question the pallet bay asks, asked of the bed. */
	bool CanAddToLoad(const ADeadlinePlayerCharacter* Character,
		FName ProductID, bool bLoose) const;

	/** Could the player take this off the bed right now? A crate or a loaded
	    pallet needs the jack (GDD 5.2); a loose box never does. */
	bool CanReceiveByHand(const ADeadlinePlayerCharacter* Character,
		FName ProductID, bool bLoose) const;

	/** F with your hands empty. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Vehicle")
	bool TryUnloadTo(ADeadlinePlayerCharacter* Character);

	// IInteractable — E opens the travel map (GDD 9.2). Loading is F, and the
	// two never compete: one is what you look at, the other is where you stand.
	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor) const override;
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual void Interact_Implementation(APawn* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION()
	void HandleLoadZoneBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& Sweep);

	UFUNCTION()
	void HandleLoadZoneEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void HandleCargoChanged(FName ChangedKey);

	/** Redraw the load from the fleet ledger. */
	void RefreshCargoStack();

	/** Shape the greybox from the DT_Vehicles body size. */
	void ApplyBodyLayout();

	/** Called when a box finishes its glide: it becomes cargo geometry. */
	void HandleSlideFinished(AContainerActor* Container);

	UFleetSubsystem* GetFleet() const;
	const FVehicleRow* GetRow() const;

	/** Where slot Index sits, in world space, for a box of this size. */
	FTransform SlotTransform(int32 Index, const FVector& BoxSizeM) const;

	/** Slots the bed can show at once, from its size and the spacing. */
	int32 GetSlotCount() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Vehicle")
	TObjectPtr<UStaticMeshComponent> CabMesh;

	/** The flat bed. Cargo stands on top of it, in the open, so a filling
	    truck is something you can see (GDD 10.1). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Vehicle")
	TObjectPtr<UStaticMeshComponent> BedMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Vehicle")
	TObjectPtr<UInstancedStaticMeshComponent> CargoMesh;

	/** Stand here and F loads or unloads (GDD 10.1 step 2). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Vehicle")
	TObjectPtr<UBoxComponent> LoadZone;

	/** Where an unloaded box appears in your hands' place. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Vehicle")
	TObjectPtr<USceneComponent> HandoverPoint;

private:
	/** VehicleKey, or the actor name when that was left blank. */
	FName ResolvedKey;

	/** Boxes still in the air. Their slots are held open until they land, so
	    the stack does not draw a box that is also flying towards it. */
	int32 BoxesInFlight = 0;
};
