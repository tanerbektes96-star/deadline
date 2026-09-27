// Copyright DEADLINE. All Rights Reserved.
//
// A face-to-face counter (GDD 6.1 channel 1). You walk up, trade, and carry
// the boxes yourself — that is why it is the cheapest channel.
//
// A counter trade never touches the warehouse ledger: buying hands you a box,
// selling takes the box out of your hands. The ledger only learns about goods
// when they are actually stored (see AStorageZoneActor).

#pragma once

#include "CoreMinimal.h"
#include "Economy/EconomySubsystem.h"
#include "GameFramework/Actor.h"
#include "Player/Interactable.h"
#include "TradePostActor.generated.h"

class AContainerActor;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETradePostRole : uint8
{
	/** Sells to the player. Interact with empty hands to buy a container. */
	Supplier,
	/** Buys from the player. Interact while carrying to sell what you hold. */
	Buyer
};

UCLASS()
class DEADLINE__API ATradePostActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATradePostActor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Trade")
	ETradePostRole PostRole = ETradePostRole::Supplier;

	/** What a Supplier hands over. Ignored by a Buyer unless AcceptedProducts is set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Trade")
	FName ProductID = TEXT("S01");

	/** A Buyer only takes these products. Empty means it takes anything. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Trade")
	TArray<FName> AcceptedProducts;

	/** How the counter settles up. Grey trades force cash (GDD 6.3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Trade")
	EPaymentMethod Payment = EPaymentMethod::Bank;

	/** Class spawned when a Supplier hands over a container. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Trade")
	TSubclassOf<AContainerActor> ContainerClass;

	/** Display name for the HUD, e.g. "Dockside Supplier". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Deadline|Trade")
	FText PostName;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Trade")
	bool AcceptsProduct(FName InProductID) const;

	// IInteractable
	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor) const override;
	virtual bool CanInteract_Implementation(APawn* Interactor) const override;
	virtual void Interact_Implementation(APawn* Interactor) override;

protected:
	/** Supplier path: charge the player and spawn a box in front of the counter. */
	bool ServeSupplier(class ADeadlinePlayerCharacter* Character);

	/** You carry counter goods away yourself, so a pallet needs the jack. */
	bool CanCarryPurchase(const class ADeadlinePlayerCharacter* Character) const;

	/** Could this counter's goods go on the stack already on the forks? A
	    counter sells one container at a time, but there is no reason to walk
	    each one to the truck separately when you brought a jack. */
	bool CanStackPurchase(const class ADeadlinePlayerCharacter* Character) const;

	/** Buyer path: take the carried box and pay for it. */
	bool ServeBuyer(class ADeadlinePlayerCharacter* Character);

	UEconomySubsystem* GetEconomy() const;

	/** Where a bought box appears. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Trade")
	TObjectPtr<USceneComponent> HandoverPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Deadline|Trade")
	TObjectPtr<UStaticMeshComponent> MeshComponent;
};
