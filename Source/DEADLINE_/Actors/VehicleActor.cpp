// Copyright DEADLINE. All Rights Reserved.

#include "Actors/VehicleActor.h"

#include "Actors/ContainerActor.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Core/DeadlinePlayerController.h"
#include "Fleet/FleetSubsystem.h"
#include "Player/DeadlinePlayerCharacter.h"
#include "Travel/TravelSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Share of the body length taken by the cab; the rest is bed. */
	constexpr float CabLengthFraction = 0.3f;

	/** Bed deck height and thickness, in cm. */
	constexpr float BedTopCm = 70.f;
	constexpr float BedThicknessCm = 20.f;
}

AVehicleActor::AVehicleActor()
{
	// A parked truck does no per-frame work. The only thing that moves is a
	// box gliding into a slot, and that box ticks itself.
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));

	CabMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cab"));
	CabMesh->SetupAttachment(Root);
	CabMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CabMesh->SetCollisionResponseToAllChannels(ECR_Block);

	BedMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bed"));
	BedMesh->SetupAttachment(Root);
	BedMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BedMesh->SetCollisionResponseToAllChannels(ECR_Block);

	CargoMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Cargo"));
	CargoMesh->SetupAttachment(Root);
	CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CargoMesh->SetCastShadow(false);

	if (CubeMesh.Succeeded())
	{
		CabMesh->SetStaticMesh(CubeMesh.Object);
		BedMesh->SetStaticMesh(CubeMesh.Object);
		CargoMesh->SetStaticMesh(CubeMesh.Object);
	}

	LoadZone = CreateDefaultSubobject<UBoxComponent>(TEXT("LoadZone"));
	LoadZone->SetupAttachment(Root);
	LoadZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LoadZone->SetCollisionResponseToAllChannels(ECR_Overlap);
	LoadZone->SetGenerateOverlapEvents(true);

	HandoverPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HandoverPoint"));
	HandoverPoint->SetupAttachment(Root);

	ApplyBodyLayout();
}

// --- Layout ------------------------------------------------------------------

const FVehicleRow* AVehicleActor::GetRow() const
{
	const UFleetSubsystem* Fleet = GetFleet();
	return Fleet ? Fleet->FindVehicleRow(VehicleID) : nullptr;
}

void AVehicleActor::ApplyBodyLayout()
{
	// The fleet subsystem is not available in the constructor or in the editor
	// viewport, so fall back to the default row values when it is not there.
	FVehicleRow Fallback;
	const FVehicleRow* Row = GetRow();
	const FVehicleRow& V = Row ? *Row : Fallback;

	const float L = V.BodyLengthM * 100.f;
	const float W = V.BodyWidthM * 100.f;
	const float H = V.BodyHeightM * 100.f;
	const float CabLen = L * CabLengthFraction;
	const float BedLen = L - CabLen;

	if (CabMesh)
	{
		CabMesh->SetRelativeScale3D(FVector(CabLen, W * 0.95f, H) / 100.f);
		CabMesh->SetRelativeLocation(FVector(L * 0.5f - CabLen * 0.5f, 0.f, H * 0.5f));
	}
	if (BedMesh)
	{
		BedMesh->SetRelativeScale3D(FVector(BedLen, W, BedThicknessCm) / 100.f);
		BedMesh->SetRelativeLocation(
			FVector(-L * 0.5f + BedLen * 0.5f, 0.f, BedTopCm - BedThicknessCm * 0.5f));
	}
	if (LoadZone)
	{
		// Standing room off the tail, where a person would work.
		LoadZone->SetBoxExtent(FVector(90.f, W * 0.5f + 40.f, 110.f), /*bUpdateOverlaps=*/false);
		LoadZone->SetRelativeLocation(FVector(-L * 0.5f - 90.f, 0.f, 110.f));
	}
	if (HandoverPoint)
	{
		HandoverPoint->SetRelativeLocation(FVector(-L * 0.5f - 90.f, 0.f, 120.f));
	}
}

void AVehicleActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyBodyLayout();
}

int32 AVehicleActor::GetSlotCount() const
{
	FVehicleRow Fallback;
	const FVehicleRow* Row = GetRow();
	const FVehicleRow& V = Row ? *Row : Fallback;

	const float BedLen = V.BodyLengthM * 100.f * (1.f - CabLengthFraction);
	const float BedWidth = V.BodyWidthM * 100.f;
	const float StackHeight = FMath::Max(V.BodyHeightM * 100.f - BedTopCm, SlotSpacing.Z);

	const int32 Columns = FMath::Max(1, FMath::FloorToInt(BedLen / FMath::Max(1.f, SlotSpacing.X)));
	const int32 Rows = FMath::Max(1, FMath::FloorToInt(BedWidth / FMath::Max(1.f, SlotSpacing.Y)));
	const int32 Levels = FMath::Max(1, FMath::FloorToInt(StackHeight / FMath::Max(1.f, SlotSpacing.Z)));
	return Columns * Rows * Levels;
}

FTransform AVehicleActor::SlotTransform(int32 Index, const FVector& BoxSizeM) const
{
	FVehicleRow Fallback;
	const FVehicleRow* Row = GetRow();
	const FVehicleRow& V = Row ? *Row : Fallback;

	const float L = V.BodyLengthM * 100.f;
	const float W = V.BodyWidthM * 100.f;
	const float BedLen = L * (1.f - CabLengthFraction);

	const int32 Columns = FMath::Max(1, FMath::FloorToInt(BedLen / FMath::Max(1.f, SlotSpacing.X)));
	const int32 Rows = FMath::Max(1, FMath::FloorToInt(W / FMath::Max(1.f, SlotSpacing.Y)));

	// Fill the tail first, then across, then upwards: a truck loads from the
	// back door forwards, and that is what looks right while it fills.
	const int32 Column = Index % Columns;
	const int32 RowIndex = (Index / Columns) % Rows;
	const int32 Level = Index / (Columns * Rows);

	const FVector Local(
		-L * 0.5f + SlotSpacing.X * (Column + 0.5f),
		-W * 0.5f + SlotSpacing.Y * (RowIndex + 0.5f),
		BedTopCm + SlotSpacing.Z * Level + BoxSizeM.Z * 50.f);

	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(Local), BoxSizeM);
}

// --- Lifetime ----------------------------------------------------------------

UFleetSubsystem* AVehicleActor::GetFleet() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFleetSubsystem>() : nullptr;
}

void AVehicleActor::BeginPlay()
{
	Super::BeginPlay();

	ResolvedKey = VehicleKey.IsNone() ? GetFName() : VehicleKey;

	if (UFleetSubsystem* Fleet = GetFleet())
	{
		Fleet->RegisterVehicle(ResolvedKey, VehicleID);
		Fleet->OnCargoChanged.AddDynamic(this, &AVehicleActor::HandleCargoChanged);
	}

	if (LoadZone)
	{
		LoadZone->OnComponentBeginOverlap.AddDynamic(this, &AVehicleActor::HandleLoadZoneBegin);
		LoadZone->OnComponentEndOverlap.AddDynamic(this, &AVehicleActor::HandleLoadZoneEnd);
	}

	ApplyBodyLayout();   // the row is loadable now, unlike in the constructor
	RefreshCargoStack();
}

void AVehicleActor::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UFleetSubsystem* Fleet = GetFleet())
	{
		Fleet->OnCargoChanged.RemoveDynamic(this, &AVehicleActor::HandleCargoChanged);
	}
	if (LoadZone)
	{
		LoadZone->OnComponentBeginOverlap.RemoveDynamic(this, &AVehicleActor::HandleLoadZoneBegin);
		LoadZone->OnComponentEndOverlap.RemoveDynamic(this, &AVehicleActor::HandleLoadZoneEnd);
	}
	Super::EndPlay(Reason);
}

void AVehicleActor::HandleCargoChanged(FName ChangedKey)
{
	if (ChangedKey == ResolvedKey)
	{
		RefreshCargoStack();
	}
}

void AVehicleActor::HandleLoadZoneBegin(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(OtherActor))
	{
		Character->EnterLoadZone(this);
	}
}

void AVehicleActor::HandleLoadZoneEnd(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(OtherActor))
	{
		Character->LeaveLoadZone(this);
	}
}

// --- The visible load --------------------------------------------------------

void AVehicleActor::RefreshCargoStack()
{
	if (!CargoMesh)
	{
		return;
	}

	CargoMesh->ClearInstances();

	const UFleetSubsystem* Fleet = GetFleet();
	if (!Fleet)
	{
		return;
	}

	const TArray<FStoredContainer> Contents = Fleet->GetCargoContents(ResolvedKey);

	// Boxes still gliding in are already on the manifest. Hold their slots open
	// or the same box would be drawn twice, once flying and once landed.
	const int32 Drawn = FMath::Min(Contents.Num() - BoxesInFlight, GetSlotCount());

	for (int32 Index = 0; Index < Drawn; ++Index)
	{
		const FVector SizeM = AContainerActor::PlaceholderSizeMetres(Contents[Index].ContainerType);
		CargoMesh->AddInstance(SlotTransform(Index, SizeM), /*bWorldSpace=*/true);
	}
}

void AVehicleActor::HandleSlideFinished(AContainerActor* Container)
{
	BoxesInFlight = FMath::Max(0, BoxesInFlight - 1);

	if (Container)
	{
		Container->Destroy();   // from here on it is one ISM entry (GDD 10.1)
	}
	RefreshCargoStack();
}

// --- Loading and unloading ---------------------------------------------------

FText AVehicleActor::GetLoadPrompt(APawn* Interactor) const
{
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	const UFleetSubsystem* Fleet = GetFleet();
	if (!Character || !Fleet)
	{
		return FText::GetEmpty();
	}

	const FText Where = FText::Format(
		NSLOCTEXT("Deadline", "VehicleBU", "{0}  ({1} / {2} BU)"),
		FText::FromName(VehicleID),
		FText::AsNumber(FMath::RoundToInt(Fleet->GetUsedBU(ResolvedKey))),
		FText::AsNumber(FMath::RoundToInt(Fleet->GetCapacityBU(ResolvedKey))));

	if (const AContainerActor* Carried = Character->GetCarriedContainer())
	{
		FText Reason;
		if (!Fleet->CanLoad(ResolvedKey, Carried->ProductID, Carried->Containers,
			Carried->bLooseBox, Reason))
		{
			return Reason;
		}
		return FText::Format(
			NSLOCTEXT("Deadline", "VehicleLoad", "F - Load {1} x {0} into {2}"),
			FText::FromName(Carried->ProductID),
			FText::AsNumber(Carried->Containers), Where);
	}

	// Nothing in your hands: F has nothing to hand over. Taking is E.
	return FText::GetEmpty();
}

bool AVehicleActor::CanLoadFrom(APawn* Interactor) const
{
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	const UFleetSubsystem* Fleet = GetFleet();
	if (!Character || !Fleet)
	{
		return false;
	}

	if (const AContainerActor* Carried = Character->GetCarriedContainer())
	{
		FText Reason;
		return Fleet->CanLoad(ResolvedKey, Carried->ProductID, Carried->Containers,
			Carried->bLooseBox, Reason);
	}

	return false;
}



bool AVehicleActor::CanAddToLoad(const ADeadlinePlayerCharacter* Character,
	FName ProductID, bool bLoose) const
{
	const UFleetSubsystem* Fleet = GetFleet();
	const FVehicleCargo* Cargo = Fleet ? Fleet->GetAllCargo().Find(ResolvedKey) : nullptr;
	const FInventoryEntry* Entry = Cargo ? Cargo->Entries.Find(ProductID) : nullptr;
	if (!Entry || !Character)
	{
		return false;
	}

	const bool bWasRecorded = bLoose ? Entry->RecordedLooseBoxes > 0 : Entry->RecordedStock > 0;

	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (!Row)
	{
		return false;
	}

	const float VolumeBU = bLoose
		? FProductRow::ContainerVolumeBU(EContainerType::BoxM)
		: FProductRow::ContainerVolumeBU(Row->ContainerType);

	return Character->CanStackOnJack(ProductID, bLoose, bWasRecorded, VolumeBU);
}

bool AVehicleActor::CanReceiveByHand(const ADeadlinePlayerCharacter* Character,
	FName ProductID, bool bLoose) const
{
	// A loose box is a Box M whatever it came off, so it always goes in arms.
	if (!Character || bLoose)
	{
		return Character != nullptr;
	}

	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (!Row)
	{
		return false;
	}
	return AContainerActor::IsHandCarryable(Row->ContainerType) || Character->HasPalletJack();
}

bool AVehicleActor::TryLoadFrom(ADeadlinePlayerCharacter* Character)
{
	UFleetSubsystem* Fleet = GetFleet();
	AContainerActor* Carried = Character ? Character->GetCarriedContainer() : nullptr;
	if (!Fleet || !Carried)
	{
		return false;
	}

	// The box keeps its age going aboard. A hold is not a fridge that stops
	// time, and parking perishables in a van must not renew them.
	if (!Fleet->LoadAged(ResolvedKey, Carried->ProductID, Carried->Containers,
		Carried->bRecorded, Carried->bLooseBox, Carried->AcquiredAtMinute))
	{
		return false;
	}

	// The manifest already says the box is aboard; the actor now has half a
	// second to look like it agrees.
	Character->ReleaseCarried();

	const FVector SizeM = AContainerActor::PlaceholderSizeMetres(Carried->ContainerType);
	const int32 SlotIndex = FMath::Max(0, Fleet->GetCargoContents(ResolvedKey).Num() - 1);
	const FTransform Slot = SlotTransform(SlotIndex, SizeM);

	++BoxesInFlight;
	Carried->OnSlideFinished.BindUObject(this, &AVehicleActor::HandleSlideFinished);
	Carried->StartSlideTo(Slot.GetLocation(), Slot.Rotator(), SlideSeconds);

	RefreshCargoStack();
	return true;
}

bool AVehicleActor::TryUnloadTo(ADeadlinePlayerCharacter* Character)
{
	UFleetSubsystem* Fleet = GetFleet();
	if (!Fleet || !Character)
	{
		return false;
	}

	bool bLoose = false;
	const FName Product = Fleet->PickUnloadProduct(ResolvedKey, bLoose);
	if (Product.IsNone())
	{
		return false;
	}

	// Unloading builds a stack on the forks the same way loading one does, so
	// a pallet's worth comes off the bed in one walk rather than sixteen.
	if (Character->IsCarrying() && !CanAddToLoad(Character, Product, bLoose))
	{
		return false;
	}

	// Taking a box off is a physical move, not a sale: the manifest keeps its
	// white/grey shape, so a grey box comes back off grey.
	const FVehicleCargo* Cargo = Fleet->GetAllCargo().Find(ResolvedKey);
	const FInventoryEntry* Entry = Cargo ? Cargo->Entries.Find(Product) : nullptr;
	const bool bWasRecorded = Entry
		&& (bLoose ? Entry->RecordedLooseBoxes > 0 : Entry->RecordedStock > 0);

	// Oldest first, so this is the stamp the box comes off with.
	const double AcquiredAt = Fleet->GetOldestAcquiredMinute(ResolvedKey, Product, bLoose);

	if (!Fleet->Unload(ResolvedKey, Product, 1, bWasRecorded, bLoose))
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

	AContainerActor* Box = GetWorld()->SpawnActor<AContainerActor>(
		AContainerActor::StaticClass(), SpawnAt, Params);
	if (!Box)
	{
		Fleet->LoadAged(ResolvedKey, Product, 1, bWasRecorded, bLoose, AcquiredAt);   // put it back
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
	// Init stamps the box as new; it is not. Put the hold's age back on it.
	Box->AcquiredAtMinute = AcquiredAt;

	Character->PickUp(Box);
	return true;
}

// --- IInteractable: Seyahat Et (GDD 9.2) --------------------------------------

FName AVehicleActor::ResolveTakeProduct(const ADeadlinePlayerCharacter* Character,
	bool& bOutLoose) const
{
	bOutLoose = false;

	const UFleetSubsystem* Fleet = GetFleet();
	if (!Fleet || !Character || Character->GetLoadZoneVehicle() != this)
	{
		return NAME_None;
	}

	const FName Product = Fleet->PickUnloadProduct(ResolvedKey, bOutLoose);
	if (Product.IsNone() || !CanReceiveByHand(Character, Product, bOutLoose))
	{
		return NAME_None;
	}
	// Carrying something already is fine as long as it can go on the stack.
	if (Character->IsCarrying() && !CanAddToLoad(Character, Product, bOutLoose))
	{
		return NAME_None;
	}
	return Product;
}

FText AVehicleActor::GetInteractionPrompt_Implementation(APawn* Interactor) const
{
	const UGameInstance* GI = GetGameInstance();
	const UTravelSubsystem* Travel = GI ? GI->GetSubsystem<UTravelSubsystem>() : nullptr;
	if (!Travel)
	{
		return FText::GetEmpty();
	}

	// Standing at the tail, E handles cargo. Travel lives on M (GDD 3.3), so
	// nothing is lost by giving the back of the truck the more useful verb --
	// and with F meaning "hand over", taking needs a key of its own or a stack
	// can never be built out of a truck that still has room in it.
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	bool bLoose = false;
	if (const FName ToTake = ResolveTakeProduct(Character, bLoose); !ToTake.IsNone())
	{
		return FText::Format(
			NSLOCTEXT("Deadline", "VehicleTakeCargo", "E - Take {0} off {1}"),
			FText::FromName(ToTake), FText::FromName(ResolvedKey));
	}
	if (Travel->IsTravelling())
	{
		return NSLOCTEXT("Deadline", "VehicleOnRoad", "On the road");
	}

	const FVehicleRow* Row = GetRow();
	if (Row && Row->WarehouseOnly)
	{
		return NSLOCTEXT("Deadline", "VehicleYardOnly", "Yard vehicle - does not travel");
	}
	return NSLOCTEXT("Deadline", "VehicleTravel", "E - Travel");
}

bool AVehicleActor::CanInteract_Implementation(APawn* Interactor) const
{
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	bool bLoose = false;
	if (!ResolveTakeProduct(Character, bLoose).IsNone())
	{
		return true;
	}

	const UGameInstance* GI = GetGameInstance();
	const UTravelSubsystem* Travel = GI ? GI->GetSubsystem<UTravelSubsystem>() : nullptr;
	const FVehicleRow* Row = GetRow();
	return Travel && !Travel->IsTravelling() && Row && !Row->WarehouseOnly;
}

void AVehicleActor::Interact_Implementation(APawn* Interactor)
{
	if (ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor))
	{
		bool bLoose = false;
		if (!ResolveTakeProduct(Character, bLoose).IsNone())
		{
			TryUnloadTo(Character);
			return;
		}
	}

	// The controller owns the screen: it is the thing that swaps input modes.
	if (const APawn* Pawn = Cast<APawn>(Interactor))
	{
		if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(Pawn->GetController()))
		{
			PC->OpenTravelScreen(ResolvedKey);
		}
	}
}
