// Copyright DEADLINE. All Rights Reserved.

#include "Actors/TradePostActor.h"

#include "Actors/ContainerActor.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Economy/EconomySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Player/DeadlinePlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATradePostActor::ATradePostActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}
	// Greybox counter: 2.0 x 0.8 x 1.0 m.
	MeshComponent->SetWorldScale3D(FVector(2.0f, 0.8f, 1.0f));

	HandoverPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HandoverPoint"));
	HandoverPoint->SetupAttachment(MeshComponent);
	// In front of the counter, on the side the player walks up from.
	HandoverPoint->SetRelativeLocation(FVector(0.f, 120.f, 60.f));
	HandoverPoint->SetAbsolute(false, false, /*bScale=*/true);

	ContainerClass = AContainerActor::StaticClass();
}

UEconomySubsystem* ATradePostActor::GetEconomy() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEconomySubsystem>() : nullptr;
}

bool ATradePostActor::AcceptsProduct(FName InProductID) const
{
	return AcceptedProducts.IsEmpty() || AcceptedProducts.Contains(InProductID);
}

// --- IInteractable -----------------------------------------------------------

FText ATradePostActor::GetInteractionPrompt_Implementation(APawn* Interactor) const
{
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	const UEconomySubsystem* Economy = GetEconomy();
	if (!Character || !Economy)
	{
		return FText::GetEmpty();
	}

	if (PostRole == ETradePostRole::Supplier)
	{
		if (Character->IsCarrying() && !CanStackPurchase(Character))
		{
			return NSLOCTEXT("Deadline", "TradeHandsFull", "Hands full");
		}
		return FText::Format(
			NSLOCTEXT("Deadline", "TradeBuy", "E - Buy {0} for ${1}"),
			FText::FromName(ProductID),
			FText::AsNumber(FMath::RoundToInt(Economy->GetBuyPrice(ProductID))));
	}

	const AContainerActor* Carried = Character->GetCarriedContainer();
	if (!Carried)
	{
		return NSLOCTEXT("Deadline", "TradeNothingToSell", "Nothing to sell");
	}
	if (!AcceptsProduct(Carried->ProductID))
	{
		return NSLOCTEXT("Deadline", "TradeNotAccepted", "Not bought here");
	}
	if (Carried->bLooseBox)
	{
		// A box off a pallet is a sixteenth of the traded unit. Counters deal
		// in whole containers, so it has to go back on a pallet first (GDD 10.2).
		return NSLOCTEXT("Deadline", "TradeLooseBox", "Rebuild the pallet first");
	}
	// The condition discount, not the bare quote: what the counter pays for
	// aged goods is less than the market number (GDD 5.1), and a prompt that
	// promises the higher one is the screen lying about the only figure the
	// player is deciding on. And the count, because a stack is not one box.
	const float Paid = Economy->GetSellPrice(Carried->ProductID)
		* Carried->GetConditionMultiplier() * Carried->Containers;

	return FText::Format(
		NSLOCTEXT("Deadline", "TradeSell", "E - Sell {1} x {0} for ${2}"),
		FText::FromName(Carried->ProductID),
		FText::AsNumber(Carried->Containers),
		FText::AsNumber(FMath::RoundToInt(Paid)));
}

bool ATradePostActor::CanInteract_Implementation(APawn* Interactor) const
{
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	const UEconomySubsystem* Economy = GetEconomy();
	if (!Character || !Economy)
	{
		return false;
	}

	if (PostRole == ETradePostRole::Supplier)
	{
		if (Character->IsCarrying() && !CanStackPurchase(Character))
		{
			return false;
		}
		const float Price = Economy->GetBuyPrice(ProductID);
		const float Funds = (Payment == EPaymentMethod::Cash) ? Economy->GetCash() : Economy->GetBank();
		return Price > 0.f && Funds >= Price;
	}

	const AContainerActor* Carried = Character->GetCarriedContainer();
	return Carried && !Carried->bLooseBox && AcceptsProduct(Carried->ProductID);
}

void ATradePostActor::Interact_Implementation(APawn* Interactor)
{
	ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	if (!Character)
	{
		return;
	}

	if (PostRole == ETradePostRole::Supplier)
	{
		ServeSupplier(Character);
	}
	else
	{
		ServeBuyer(Character);
	}
}

// --- Trading -----------------------------------------------------------------

bool ATradePostActor::CanStackPurchase(const ADeadlinePlayerCharacter* Character) const
{
	// Counter goods are invoiced and whole containers, so the only questions
	// left are "same product?" and "room on the forks?".
	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (!Character || !Row)
	{
		return false;
	}

	return Character->CanStackOnJack(ProductID, /*bLoose=*/false, /*bRecorded=*/true,
		FProductRow::ContainerVolumeBU(Row->ContainerType));
}

bool ATradePostActor::CanCarryPurchase(const ADeadlinePlayerCharacter* Character) const
{
	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (!Character || !Row)
	{
		return false;
	}
	return AContainerActor::IsHandCarryable(Row->ContainerType) || Character->HasPalletJack();
}

bool ATradePostActor::ServeSupplier(ADeadlinePlayerCharacter* Character)
{
	UEconomySubsystem* Economy = GetEconomy();
	if (!Economy || !ContainerClass)
	{
		return false;
	}

	// You carry counter goods away yourself (GDD 6.1), so you have to be able
	// to. Refuse before the money moves, not after.
	if (!CanCarryPurchase(Character))
	{
		return false;
	}

	// Counter goods are invoiced, and they do not enter the ledger yet: the
	// player walks away holding the box.
	if (!Economy->TryBuy(ProductID, 1, ETradeLedger::White, Payment, /*bAffectInventory=*/false))
	{
		return false;
	}

	// Already carrying these goods on the forks: one taller stack, no new box.
	// Bought now, so the new container is fresh -- AddToStack keeps the older
	// stamp, which is the right way round (GDD 5.1).
	if (AContainerActor* Stack = Character->GetCarriedContainer())
	{
		Stack->AddToStack(1, Stack->AcquiredAtMinute);
		Character->RefreshJackLoad();
		return true;
	}

	const FTransform SpawnAt(FRotator::ZeroRotator, HandoverPoint->GetComponentLocation());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AContainerActor* Box = GetWorld()->SpawnActor<AContainerActor>(ContainerClass, SpawnAt, Params);
	if (!Box)
	{
		// Refund rather than take the money for nothing.
		Economy->TrySell(ProductID, 1, ETradeLedger::White, Payment, /*bAffectInventory=*/false);
		return false;
	}

	Box->InitFromProduct(ProductID, /*bInRecorded=*/true);

	// GDD 6.1: at a counter you walk away carrying the goods. CanInteract
	// already refused this trade if the hands were full, so the pick up
	// succeeds; the spawned position only matters if it somehow does not.
	Character->PickUp(Box);
	return true;
}

bool ATradePostActor::ServeBuyer(ADeadlinePlayerCharacter* Character)
{
	UEconomySubsystem* Economy = GetEconomy();
	AContainerActor* Carried = Character->GetCarriedContainer();
	if (!Economy || !Carried || Carried->bLooseBox || !AcceptsProduct(Carried->ProductID))
	{
		return false;
	}

	// A grey box can only be settled in cash (GDD 6.3).
	const ETradeLedger Ledger = Carried->bRecorded ? ETradeLedger::White : ETradeLedger::Grey;
	const EPaymentMethod UsedPayment =
		(Ledger == ETradeLedger::Grey) ? EPaymentMethod::Cash : Payment;

	// The box in your hands has already left the ledger, so it has to say for
	// itself how old it is. Without this, walking stock to the counter would
	// wash the age off it and obsolescence would never cost anybody anything.
	if (!Economy->TrySell(Carried->ProductID, Carried->Containers, Ledger, UsedPayment,
		/*bAffectInventory=*/false, Carried->GetConditionMultiplier()))
	{
		return false;
	}

	Character->ReleaseCarried();
	Carried->Destroy();
	return true;
}
