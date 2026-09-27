// Copyright DEADLINE. All Rights Reserved.
//
// First person only (GDD 3.1): no third-person camera, no character creator,
// no full body. Arms and a carried box are all that is ever on screen.
//
// The focus trace runs on a 10 Hz timer rather than Tick (CLAUDE.md tick
// rules, option 3). 60 Hz line traces buy nothing a player can perceive.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DeadlinePlayerCharacter.generated.h"

class AContainerActor;
class APalletJackActor;
class AVehicleActor;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USceneComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFocusChanged, AActor*, NewFocus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCarryChanged, AContainerActor*, Carried);

UCLASS()
class DEADLINE__API ADeadlinePlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ADeadlinePlayerCharacter();

	// --- Events for the HUD -------------------------------------------------

	/** Fires when the actor under the crosshair changes. nullptr = nothing. */
	UPROPERTY(BlueprintAssignable, Category = "Deadline|Interaction")
	FOnFocusChanged OnFocusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Carry")
	FOnCarryChanged OnCarryChanged;

	// --- Carrying -----------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	bool IsCarrying() const { return CarriedContainer != nullptr; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	AContainerActor* GetCarriedContainer() const { return CarriedContainer; }

	/** Take a box into your hands. Fails if hands are full, or if it is a
	    pallet or crate and you have no jack to put it on. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Carry")
	bool PickUp(AContainerActor* Container);

	/** Can you move this container at all, right now, with what you are
	    holding? A loaded pallet is a yes with the jack and a no without it,
	    which is the whole of the rule: the tool decides what you can lift. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	bool CanCarry(const AContainerActor* Container) const;

	// --- The pallet jack (GDD 5.2) ------------------------------------------
	//
	// A person cannot pick up a loaded pallet -- 16 boxes, well over half a
	// tonne on the heavier products. They can carry an empty one, and they can
	// walk goods onto it a box at a time. The jack is what makes the loaded
	// pallet movable, and it is the only thing that does.

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	bool HasPalletJack() const { return PalletJack != nullptr; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	APalletJackActor* GetPalletJack() const { return PalletJack; }

	/** Push the jack. Fails with your hands full: both go on the handle. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Carry")
	bool TakePalletJack(APalletJackActor* Jack);

	/** Let go of it. Refused while a pallet is still on the forks. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Carry")
	bool ReleasePalletJack();

	/**
	 * Can another container of these goods go on the stack you are pushing?
	 *
	 * Same goods, same ledger, and room within the forks' Box Units. Mixing
	 * white and grey on one stack would lose the distinction the whole grey
	 * market rests on (GDD 7.1), so a mismatched box is refused rather than
	 * quietly folded in.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	bool CanStackOnJack(FName ProductID, bool bLoose, bool bRecorded, float VolumeBU) const;

	/** Box Units left on the forks. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	float GetJackFreeBU() const;

	/** Redraw the boxes on the forks. Whoever grows the stack calls this. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Carry")
	void RefreshJackLoad();

	/** Put the carried box down in front of you. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Carry")
	bool DropCarried();

	/** Hand the carried box off without placing it in the world (a sale, a
	    rack, a vehicle). Returns the box and clears the hands. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Carry")
	AContainerActor* ReleaseCarried();

	// --- Loading zones (GDD 10.1) -------------------------------------------
	//
	// A vehicle's tail zone is not something you look at, it is somewhere you
	// stand, so it is driven by overlap rather than by the focus trace.

	/** Called by a vehicle's load trigger. */
	void EnterLoadZone(AVehicleActor* Vehicle);
	void LeaveLoadZone(AVehicleActor* Vehicle);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	AVehicleActor* GetLoadZoneVehicle() const { return LoadZoneVehicle; }

	/** Prompt for the zone you are standing in. Empty when you are not in one. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	FText GetLoadPrompt() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Carry")
	bool CanUseLoadZone() const;

	// --- Interaction --------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Interaction")
	AActor* GetFocusedActor() const { return FocusedActor; }

	/** Prompt text for whatever is currently focused. Empty if nothing. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Interaction")
	FText GetFocusPrompt() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Interaction")
	bool CanInteractWithFocus() const;

	// --- Camera -------------------------------------------------------------

	/** Clamped to the 70-100 range from GDD 3.2. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Camera")
	void SetFieldOfView(float NewFOV);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Camera")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

protected:
	virtual void BeginPlay() override;
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// --- Input handlers -----------------------------------------------------
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_SprintStart(const FInputActionValue& Value);
	void Input_SprintStop(const FInputActionValue& Value);
	void Input_Interact(const FInputActionValue& Value);
	void Input_Drop(const FInputActionValue& Value);
	void Input_Load(const FInputActionValue& Value);
	void Input_ToggleMarket(const FInputActionValue& Value);
	void Input_ToggleMap(const FInputActionValue& Value);
	void Input_ToggleForecast(const FInputActionValue& Value);

	/** Push DefaultMappingContext into the Enhanced Input subsystem. Returns
	    false if the context did not stick, which happens while the player
	    input object does not exist yet. */
	bool ApplyInputMappings();

	/** Timer callback that keeps trying until the context sticks. */
	void RetryInputMappings();

	/** Timer callback: line trace for whatever is in front of the camera. */
	void UpdateFocus();

	void RefreshMovementSpeed();

	// --- Components ---------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Where a carried box sits: in front of the camera, low in the view. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Carry")
	TObjectPtr<USceneComponent> CarrySocket;

	// --- Input assets (assign on the Blueprint subclass) --------------------

	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputAction> DropAction;

	/** IA_Market. Opens and closes the market screen (GDD 17). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputAction> MarketAction;

	/** IA_Load. F — hand what you are carrying to the rack or truck bed in
	    front of you (GDD 3.3). Taking is E. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputAction> LoadAction;

	/** IA_Map. M — the travel map (GDD 3.3). It has its own key because E at
	    the back of a truck is busy handling cargo. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputAction> MapAction;

	/** IA_Forecast. N — the forecast board (GDD 3.3 puts forecasts and their
	    results on N; the notebook joins it as a second tab). */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Input")
	TObjectPtr<UInputAction> ForecastAction;

	/** How often the focus trace runs, in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Interaction")
	float FocusTraceInterval = 0.1f;

private:
	UPROPERTY(Transient)
	TObjectPtr<AContainerActor> CarriedContainer;

	/** The jack in front of you, or null. */
	UPROPERTY(Transient)
	TObjectPtr<APalletJackActor> PalletJack;

	/** Where the jack rides while you push it. */
	UPROPERTY(VisibleAnywhere, Category = "Deadline|Carry")
	TObjectPtr<USceneComponent> JackSocket;

	UPROPERTY(Transient)
	TObjectPtr<AActor> FocusedActor;

	/** The vehicle whose tail zone you are standing in, if any. */
	UPROPERTY(Transient)
	TObjectPtr<AVehicleActor> LoadZoneVehicle;

	bool bWantsToSprint = false;

	FTimerHandle FocusTimer;
	FTimerHandle InputRetryTimer;
	int32 InputMappingAttempts = 0;
};
