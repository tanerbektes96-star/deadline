// Copyright DEADLINE. All Rights Reserved.

#include "Actors/StorageZoneActor.h"

#include "Actors/ContainerActor.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Inventory/InventorySubsystem.h"
#include "Player/DeadlinePlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

AStorageZoneActor::AStorageZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// A bare scene root, so the greybox frame can be scaled to the class
	// footprint without stretching the boxes stacked on it.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComponent->SetupAttachment(Root);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}
	MeshComponent->SetWorldScale3D(ClassFootprintMetres());
	RefreshInteractionVolume();

	// The stack. One instance per stored container (GDD 10.1): no actors, no
	// physics, one draw call.
	StackMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Stack"));
	StackMesh->SetupAttachment(Root);
	StackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StackMesh->SetCastShadow(false);
	if (CubeMesh.Succeeded())
	{
		StackMesh->SetStaticMesh(CubeMesh.Object);
	}

	// Only the interaction trace sees this. It blocks Visibility and ignores
	// everything else, so it never gets in the player's way.
	InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
	InteractionVolume->SetupAttachment(Root);
	InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionVolume->SetHiddenInGame(true);

	HandoverPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HandoverPoint"));
	HandoverPoint->SetupAttachment(Root);
	HandoverPoint->SetRelativeLocation(FVector(0.f, -100.f, 0.f));

	ContainerClass = AContainerActor::StaticClass();
}

void AStorageZoneActor::RefreshInteractionVolume()
{
	if (!InteractionVolume)
	{
		return;
	}

	// Wide as the zone, deep enough to hit from a step back, and tall enough
	// to be at eye level even when the zone itself is a floor pad.
	const FVector FootprintCm = ClassFootprintMetres() * 100.f;
	const float HalfX = FMath::Max(FootprintCm.X, 100.f) * 0.5f;
	const float HalfY = FMath::Max(FootprintCm.Y, 100.f) * 0.5f;
	const float HalfZ = FMath::Max(FootprintCm.Z, 180.f) * 0.5f;

	InteractionVolume->SetBoxExtent(FVector(HalfX, HalfY, HalfZ));
	// Standing on the floor rather than centred on the actor, so a floor pad
	// and a two-metre rack both reach the same height.
	InteractionVolume->SetRelativeLocation(FVector(0.f, 0.f, HalfZ - FootprintCm.Z * 0.5f));
}

FVector AStorageZoneActor::ClassFootprintMetres() const
{
	// Greybox only. Real racks, bays and cold rooms are Month 9 art.
	//
	// The shelf classes are a thin back panel rather than a solid block: the
	// stock stands in front of it, and a metre-deep slab would simply swallow
	// the boxes it is supposed to be showing.
	switch (StorageClass)
	{
	case EStorageClass::Rack:       return FVector(2.4f, 0.2f, 2.0f);
	case EStorageClass::PalletBay:  return FVector(2.8f, 2.0f, 0.15f);
	case EStorageClass::ColdZone:   return FVector(2.4f, 0.2f, 2.2f);
	case EStorageClass::SecureRack: return FVector(1.6f, 0.2f, 2.0f);
	}
	return FVector(2.4f, 0.2f, 2.0f);
}

void AStorageZoneActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Change the class in the editor and the footprint follows immediately.
	RefreshInteractionVolume();

	if (MeshComponent)
	{
		MeshComponent->SetWorldScale3D(ClassFootprintMetres());
	}
}

void AStorageZoneActor::BeginPlay()
{
	Super::BeginPlay();

	if (UInventorySubsystem* Inventory = GetInventory())
	{
		// CLAUDE.md tick rule 2: the stack redraws when the ledger says so.
		Inventory->OnStockChanged.AddDynamic(this, &AStorageZoneActor::HandleStockChanged);
		Inventory->OnCapacityChanged.AddDynamic(this, &AStorageZoneActor::HandleCapacityChanged);
	}
	RefreshStack();
}

void AStorageZoneActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UInventorySubsystem* Inventory = GetInventory())
	{
		Inventory->OnStockChanged.RemoveDynamic(this, &AStorageZoneActor::HandleStockChanged);
		Inventory->OnCapacityChanged.RemoveDynamic(this, &AStorageZoneActor::HandleCapacityChanged);
	}
	Super::EndPlay(Reason);
}

void AStorageZoneActor::HandleStockChanged(FName ProductID)
{
	RefreshStack();
}

void AStorageZoneActor::HandleCapacityChanged()
{
	RefreshStack();
}

UInventorySubsystem* AStorageZoneActor::GetInventory() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
}

// --- The visible stack -------------------------------------------------------

void AStorageZoneActor::RefreshStack()
{
	if (!StackMesh)
	{
		return;
	}

	StackMesh->ClearInstances();

	const UInventorySubsystem* Inventory = GetInventory();
	if (!Inventory)
	{
		return;
	}

	const TArray<FStoredContainer> Contents = Inventory->GetClassContents(StorageClass);
	const int32 Columns = FMath::Max(1, SlotColumns);
	const int32 Slots = GetSlotCount();

	// Slot 0 is derived from the footprint, so moving or resizing a zone does
	// not leave its stack floating somewhere behind it.
	const FVector FootprintCm = ClassFootprintMetres() * 100.f;
	const float FirstColumnX = -FootprintCm.X * 0.5f + SlotSpacing.X * 0.5f;
	const float FloorZ = IsFloorZone() ? FootprintCm.Z * 0.5f    // stand on the pad
	                                   : -FootprintCm.Z * 0.5f;  // stand on the ground

	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		const int32 ContentIndex = DisplayStartSlot + Slot;
		if (!Contents.IsValidIndex(ContentIndex))
		{
			break;
		}

		const FVector SizeM = AContainerActor::PlaceholderSizeMetres(
			Contents[ContentIndex].ContainerType);
		const int32 Column = Slot % Columns;
		const int32 Level = Slot / Columns;

		FVector Location = SlotOrigin
			+ FVector(FirstColumnX + Column * SlotSpacing.X, 0.f, FloorZ)
			+ FVector(0.f, 0.f, SizeM.Z * 50.f);   // cube pivot is its centre

		if (IsFloorZone())
		{
			// A pad is stacked across and back, not upwards.
			Location.Y += -FootprintCm.Y * 0.5f + SlotSpacing.Y * 0.5f
				+ Level * SlotSpacing.Y;
		}
		else
		{
			// Out in front of the panel, so the shelf does not hide its stock.
			Location.Y -= FootprintCm.Y * 0.5f + SizeM.Y * 50.f;
			Location.Z += Level * SlotSpacing.Y;
		}

		StackMesh->AddInstance(FTransform(FRotator::ZeroRotator, Location, SizeM));
	}
}

bool AStorageZoneActor::HasRoomFor(const AContainerActor& Box) const
{
	const UInventorySubsystem* Inventory = GetInventory();
	if (!Inventory)
	{
		return false;
	}
	return Box.bLooseBox
		? Inventory->HasSpaceForLooseBoxes(Box.ProductID, Box.Containers)
		: Inventory->HasSpaceFor(Box.ProductID, Box.Containers);
}

bool AStorageZoneActor::IsWithdrawLoose(FName ProductID) const
{
	const UInventorySubsystem* Inventory = GetInventory();
	return Inventory && !ProductID.IsNone()
		&& Inventory->GetStorageClassFor(ProductID) != StorageClass;
}

// --- What this zone acts on --------------------------------------------------

FName AStorageZoneActor::ResolveWithdrawProduct() const
{
	const UInventorySubsystem* Inventory = GetInventory();
	if (!Inventory)
	{
		return NAME_None;
	}

	const TArray<FName> Candidates = Inventory->GetStockedProductIDsInClass(StorageClass);
	if (!WithdrawProductID.IsNone() && Candidates.Contains(WithdrawProductID))
	{
		return WithdrawProductID;
	}
	return Candidates.Num() > 0 ? Candidates[0] : NAME_None;
}

FName AStorageZoneActor::ResolvePalletToOpen() const
{
	const UInventorySubsystem* Inventory = GetInventory();
	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	if (!Inventory || !Catalogue || StorageClass != EStorageClass::PalletBay)
	{
		return NAME_None;
	}

	FName First = NAME_None;
	for (const FName& ID : Inventory->GetStockedProductIDsInClass(EStorageClass::PalletBay))
	{
		const FProductRow* Row = Catalogue->FindProduct(ID);
		if (!Row || Row->ContainerType != EContainerType::Pallet
			|| Inventory->GetPhysicalStock(ID) <= 0)
		{
			continue;   // crates do not come apart
		}
		if (ID == WithdrawProductID)
		{
			return ID;
		}
		if (First.IsNone())
		{
			First = ID;
		}
	}
	return First;
}

EContainerType AStorageZoneActor::ContainerTypeOf(FName ProductID) const
{
	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	return Row ? Row->ContainerType : EContainerType::BoxM;
}

FName AStorageZoneActor::ResolvePalletToCloseIgnoringPallets() const
{
	const UInventorySubsystem* Inventory = GetInventory();
	if (!Inventory || StorageClass != EStorageClass::PalletBay)
	{
		return NAME_None;
	}

	for (const FName& ID : Inventory->GetStockedProductIDs())
	{
		if (Inventory->GetLooseBoxes(ID) >= FInventoryEntry::BoxesPerPallet)
		{
			return ID;
		}
	}
	return NAME_None;
}

FName AStorageZoneActor::ResolvePalletToClose() const
{
	const UInventorySubsystem* Inventory = GetInventory();
	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	if (!Inventory || !Catalogue || StorageClass != EStorageClass::PalletBay)
	{
		return NAME_None;
	}

	// Loose boxes live on the racks, so this looks at the whole ledger rather
	// than at what is in the bay.
	// Sixteen boxes and nothing to stack them on is not a pallet (GDD 5.6).
	if (Inventory->GetEmptyPallets() <= 0)
	{
		return NAME_None;
	}

	FName First = NAME_None;
	for (const FName& ID : Inventory->GetStockedProductIDs())
	{
		if (Inventory->GetLooseBoxes(ID) < FInventoryEntry::BoxesPerPallet)
		{
			continue;
		}
		if (ID == WithdrawProductID)
		{
			return ID;
		}
		if (First.IsNone())
		{
			First = ID;
		}
	}
	return First;
}

// --- IInteractable -----------------------------------------------------------

FText AStorageZoneActor::GetInteractionPrompt_Implementation(APawn* Interactor) const
{
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	const UInventorySubsystem* Inventory = GetInventory();
	if (!Character || !Inventory)
	{
		return FText::GetEmpty();
	}

	const FText ClassName = FStorageClassRules::DisplayName(StorageClass);

	// Hands or forks already full of something this zone cannot add to: the
	// only thing left is to hand it over, and that is F (GDD 3.3).
	if (Character->IsCarrying() && !CanAddToLoad(Character))
	{
		return GetStorePrompt(const_cast<ADeadlinePlayerCharacter*>(Character));
	}

	if (StorageClass == EStorageClass::PalletBay)
	{
		// With the jack you are here to move a pallet. Without it you are here
		// to work on one: open it or rebuild it.
		if (Character->HasPalletJack())
		{
			const FName ToTake = ResolveWithdrawProduct();
			return ToTake.IsNone()
				? FText::Format(NSLOCTEXT("Deadline", "StorageBayEmpty", "{0} empty"), ClassName)
				: FText::Format(
					NSLOCTEXT("Deadline", "StorageWithdrawJack", "E - Load {0} onto the jack  ({1} left)"),
					FText::FromName(ToTake),
					FText::AsNumber(Inventory->GetPhysicalStock(ToTake)));
		}

		if (const FName ToOpen = ResolvePalletToOpen(); !ToOpen.IsNone())
		{
			return FText::Format(
				NSLOCTEXT("Deadline", "StorageOpenPallet", "E - Open pallet {0}  (into {1} boxes)"),
				FText::FromName(ToOpen),
				FText::AsNumber(FInventoryEntry::BoxesPerPallet));
		}
		if (const FName ToClose = ResolvePalletToClose(); !ToClose.IsNone())
		{
			return FText::Format(
				NSLOCTEXT("Deadline", "StorageClosePallet", "E - Close pallet {0}  ({1} loose boxes)"),
				FText::FromName(ToClose),
				FText::AsNumber(Inventory->GetLooseBoxes(ToClose)));
		}
		// There is stock here, it is just not something arms can lift. Say so,
		// rather than calling a full bay empty.
		if (!ResolveWithdrawProduct().IsNone())
		{
			return NSLOCTEXT("Deadline", "StorageNeedsJack",
				"Too heavy by hand - fetch the pallet jack");
		}
		if (!Inventory->GetStockedProductIDs().IsEmpty()
			&& !ResolvePalletToCloseIgnoringPallets().IsNone())
		{
			return NSLOCTEXT("Deadline", "StorageNoEmptyPallets",
				"No empty pallet to stack them on");
		}
		return FText::Format(NSLOCTEXT("Deadline", "StorageBayEmpty", "{0} empty"), ClassName);
	}

	const FName Product = ResolveWithdrawProduct();
	if (Product.IsNone())
	{
		return Character->IsCarrying()
			? GetStorePrompt(const_cast<ADeadlinePlayerCharacter*>(Character))
			: FText::Format(NSLOCTEXT("Deadline", "StorageEmpty", "{0} empty"), ClassName);
	}

	// Stacking onto the forks says how full they are, because that is the
	// number the player is actually working towards.
	if (Character->IsCarrying())
	{
		const AContainerActor* Carried = Character->GetCarriedContainer();
		return FText::Format(
			NSLOCTEXT("Deadline", "StorageStackOnJack", "E - Add {0} to the stack  ({1} on, {2} BU free)"),
			FText::FromName(Product),
			FText::AsNumber(Carried->Containers),
			FText::AsNumber(FMath::FloorToInt(Character->GetJackFreeBU())));
	}

	if (IsWithdrawLoose(Product))
	{
		return FText::Format(
			NSLOCTEXT("Deadline", "StorageWithdrawLoose", "E - Take out a box of {0}  ({1} loose)"),
			FText::FromName(Product),
			FText::AsNumber(Inventory->GetLooseBoxes(Product)));
	}
	return FText::Format(
		NSLOCTEXT("Deadline", "StorageWithdraw", "E - Take out {0}  (x{1})"),
		FText::FromName(Product),
		FText::AsNumber(Inventory->GetPhysicalStock(Product)));
}

// --- F: hand the load over (GDD 3.3) ------------------------------------------

FText AStorageZoneActor::GetStorePrompt(ADeadlinePlayerCharacter* Character) const
{
	const UInventorySubsystem* Inventory = GetInventory();
	const AContainerActor* Carried = Character ? Character->GetCarriedContainer() : nullptr;
	if (!Inventory || !Carried)
	{
		return FText::GetEmpty();
	}

	const FText ClassName = FStorageClassRules::DisplayName(StorageClass);

	if (!FStorageClassRules::Accepts(StorageClass, Carried->ContainerType))
	{
		return FText::Format(
			NSLOCTEXT("Deadline", "StorageWrongClass", "{0} does not take this - needs the {1}"),
			ClassName,
			FStorageClassRules::DisplayName(
				FStorageClassRules::ClassForContainer(Carried->ContainerType)));
	}
	if (!HasRoomFor(*Carried))
	{
		return FText::Format(NSLOCTEXT("Deadline", "StorageFull", "{0} full"), ClassName);
	}

	return FText::Format(
		Carried->bLooseBox
			? NSLOCTEXT("Deadline", "StorageStoreLoose", "F - Put {1} box(es) of {0} away  ({2} / {3} BU)")
			: NSLOCTEXT("Deadline", "StorageStore", "F - Put {1} x {0} away  ({2} / {3} BU)"),
		FText::FromName(Carried->ProductID),
		FText::AsNumber(Carried->Containers),
		FText::AsNumber(FMath::RoundToInt(Inventory->GetClassUsedBU(StorageClass))),
		FText::AsNumber(FMath::RoundToInt(Inventory->GetClassCapacityBU(StorageClass))));
}

bool AStorageZoneActor::TryStoreFrom(ADeadlinePlayerCharacter* Character)
{
	return Character && Character->IsCarrying() && StoreCarried(Character);
}

bool AStorageZoneActor::CanAddToLoad(const ADeadlinePlayerCharacter* Character) const
{
	const UInventorySubsystem* Inventory = GetInventory();
	if (!Inventory || !Character || !Character->IsCarrying())
	{
		return false;
	}

	const FName Product = ResolveWithdrawProduct();
	if (Product.IsNone())
	{
		return false;
	}

	const bool bLoose = IsWithdrawLoose(Product);
	const bool bRecorded = bLoose ? Inventory->GetRecordedLooseBoxes(Product) > 0
	                              : Inventory->GetRecordedStock(Product) > 0;
	const float VolumeBU = bLoose
		? FProductRow::ContainerVolumeBU(EContainerType::BoxM)
		: FProductRow::ContainerVolumeBU(ContainerTypeOf(Product));

	return Character->CanStackOnJack(Product, bLoose, bRecorded, VolumeBU);
}

bool AStorageZoneActor::CanInteract_Implementation(APawn* Interactor) const
{
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	const UInventorySubsystem* Inventory = GetInventory();
	if (!Character || !Inventory)
	{
		return false;
	}

	if (Character->IsCarrying())
	{
		// E adds to what you are pushing. Putting it away is F.
		return CanAddToLoad(Character);
	}

	if (StorageClass == EStorageClass::PalletBay)
	{
		if (Character->HasPalletJack())
		{
			return !ResolveWithdrawProduct().IsNone();
		}
		return !ResolvePalletToOpen().IsNone() || !ResolvePalletToClose().IsNone();
	}
	return !ResolveWithdrawProduct().IsNone();
}

void AStorageZoneActor::Interact_Implementation(APawn* Interactor)
{
	ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	UInventorySubsystem* Inventory = GetInventory();
	if (!Character || !Inventory)
	{
		return;
	}

	if (StorageClass == EStorageClass::PalletBay && !Character->HasPalletJack()
		&& !Character->IsCarrying())
	{
		if (const FName ToOpen = ResolvePalletToOpen(); !ToOpen.IsNone())
		{
			Inventory->OpenPallet(ToOpen, 1);
			return;
		}
		if (const FName ToClose = ResolvePalletToClose(); !ToClose.IsNone())
		{
			Inventory->ClosePallet(ToClose, 1);
		}
		return;
	}

	WithdrawOne(Character);
}

// --- Storage -----------------------------------------------------------------

bool AStorageZoneActor::StoreCarried(ADeadlinePlayerCharacter* Character)
{
	UInventorySubsystem* Inventory = GetInventory();
	AContainerActor* Carried = Character->GetCarriedContainer();
	if (!Inventory || !Carried)
	{
		return false;
	}

	if (!FStorageClassRules::Accepts(StorageClass, Carried->ContainerType))
	{
		return false;
	}

	// The box carries its own white/grey flag straight into the ledger, and its
	// own age with it: a crate does not get younger for having been carried
	// across the warehouse.
	const bool bStored = Carried->bLooseBox
		? Inventory->AddLooseBoxesAged(Carried->ProductID, Carried->Containers,
			Carried->bRecorded, Carried->AcquiredAtMinute)
		: Inventory->AddStockAged(Carried->ProductID, Carried->Containers,
			Carried->bRecorded, Carried->AcquiredAtMinute);
	if (!bStored)
	{
		return false;
	}

	Character->ReleaseCarried();
	Carried->Destroy();
	return true;
}

bool AStorageZoneActor::WithdrawOne(ADeadlinePlayerCharacter* Character)
{
	UInventorySubsystem* Inventory = GetInventory();
	if (!Inventory || !ContainerClass)
	{
		return false;
	}
	// Carrying something is fine as long as it is the same goods and the forks
	// have room -- that is how a pallet gets built a box at a time.
	if (Character->IsCarrying() && !CanAddToLoad(Character))
	{
		return false;
	}

	const FName Product = ResolveWithdrawProduct();
	if (Product.IsNone())
	{
		return false;
	}

	// A pallet product on the racks is there as loose boxes, and one box is
	// what comes off — not the whole pallet it was cut from.
	const bool bLoose = IsWithdrawLoose(Product);

	// Loaded pallets and crates come out on the jack or not at all (GDD 5.2).
	if (!bLoose && !Character->HasPalletJack()
		&& !AContainerActor::IsHandCarryable(ContainerTypeOf(Product)))
	{
		return false;
	}

	// Taking goods back out is a physical move, not a sale: the recorded
	// ledger must not drop, or the books would show goods that never left.
	const bool bWasRecorded = bLoose ? Inventory->GetRecordedLooseBoxes(Product) > 0
	                                 : Inventory->GetRecordedStock(Product) > 0;

	// Oldest first, so this is the stamp the box leaves with. Read it now:
	// after the removal there is nothing left to ask.
	const double AcquiredAt = Inventory->GetOldestAcquiredMinute(Product, bLoose);

	const bool bTaken = bLoose
		? Inventory->RemoveLooseBoxes(Product, 1, bWasRecorded)
		: Inventory->RemoveStock(Product, 1, bWasRecorded);
	if (!bTaken)
	{
		return false;
	}

	// Already pushing this product: no new actor, just a taller stack.
	if (AContainerActor* Stack = Character->GetCarriedContainer())
	{
		Stack->AddToStack(1, AcquiredAt);
		Character->RefreshJackLoad();
		return true;
	}

	const FTransform SpawnAt(FRotator::ZeroRotator, HandoverPoint->GetComponentLocation());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AContainerActor* Box = GetWorld()->SpawnActor<AContainerActor>(ContainerClass, SpawnAt, Params);
	if (!Box)
	{
		// Put it back exactly as it was taken.
		if (bLoose)
		{
			Inventory->AddLooseBoxesAged(Product, 1, bWasRecorded, AcquiredAt);
		}
		else
		{
			Inventory->AddStockAged(Product, 1, bWasRecorded, AcquiredAt);
		}
		return false;
	}

	if (bLoose)
	{
		Box->InitAsLooseBox(Product, bWasRecorded);
	}
	else
	{
		Box->InitFromProduct(Product, bWasRecorded);
	}
	// Init stamps the box as new; it is not. Put the shelf's age back on it.
	Box->AcquiredAtMinute = AcquiredAt;

	// Taking stock off the shelf puts it straight in your hands; the caller
	// already checked the hands were empty.
	Character->PickUp(Box);
	return true;
}
