// Copyright DEADLINE. All Rights Reserved.

#include "Actors/ContainerActor.h"

#include "Components/StaticMeshComponent.h"
#include "Core/DeadlineSettings.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Inventory/Condition.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Player/DeadlinePlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

AContainerActor::AContainerActor()
{
	// A box does no per-frame work except while it is sliding into a slot, and
	// it switches its own tick off the moment it lands (CLAUDE.md tick rule 6).
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(MeshComponent);

	// Physics is never enabled on a container (GDD 10.1).
	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);

	// Placeholder greybox cube; real meshes are Month 9 work.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMesh.Object);
	}
}

void AContainerActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyPlaceholderScale();
}

void AContainerActor::StartSlideTo(const FVector& TargetLocation,
	const FRotator& TargetRotation, float Duration)
{
	SlideFrom = GetActorLocation();
	SlideFromRotation = GetActorRotation();
	SlideTo = TargetLocation;
	SlideToRotation = TargetRotation;
	SlideDuration = FMath::Max(0.01f, Duration);
	SlideElapsed = 0.f;
	bSliding = true;

	// In the air it must not block the player or catch the focus trace.
	SetCarried(true);
	SetActorTickEnabled(true);
}

void AContainerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bSliding)
	{
		SetActorTickEnabled(false);
		return;
	}

	SlideElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(SlideElapsed / SlideDuration, 0.f, 1.f);

	// Ease out: a box slows into its slot rather than stopping dead.
	const float Eased = 1.f - FMath::Square(1.f - Alpha);
	SetActorLocationAndRotation(
		FMath::Lerp(SlideFrom, SlideTo, Eased),
		FMath::Lerp(SlideFromRotation, SlideToRotation, Eased));

	if (Alpha >= 1.f)
	{
		bSliding = false;
		SetActorTickEnabled(false);
		OnSlideFinished.ExecuteIfBound(this);
	}
}

float AContainerActor::CarrySpeedMultiplier(EContainerType Type)
{
	// GDD 3.2: S 95%, M 85%, L 70%. Hand-portable specials follow their size.
	switch (Type)
	{
	case EContainerType::BoxS:       return 0.95f;
	case EContainerType::BoxM:       return 0.85f;
	case EContainerType::BoxL:       return 0.70f;
	case EContainerType::ColdTote:   return 0.85f;
	case EContainerType::SecureCase: return 0.95f;
	case EContainerType::Crate:
	case EContainerType::Pallet:     return 0.f;   // forklift only
	}
	return 1.f;
}

double AContainerActor::NowMinute() const
{
	const UGameInstance* GI = GetGameInstance();
	const UTimeSubsystem* Time = GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
	return Time ? Time->GetTotalMinutes() : 0.0;
}

const FProductRow* AContainerActor::FindRow() const
{
	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	return Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
}

bool AContainerActor::IsHandCarryable(EContainerType Type)
{
	return Type != EContainerType::Crate && Type != EContainerType::Pallet;
}

void AContainerActor::InitFromProduct(FName InProductID, bool bInRecorded)
{
	ProductID = InProductID;
	bRecorded = bInRecorded;
	bLooseBox = false;

	// Brand new unless whoever made this box says otherwise straight after.
	AcquiredAtMinute = NowMinute();

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UProductCatalogSubsystem* Catalogue = GI->GetSubsystem<UProductCatalogSubsystem>())
		{
			if (const FProductRow* Row = Catalogue->FindProduct(InProductID))
			{
				ContainerType = Row->ContainerType;
			}
		}
	}

	ApplyPlaceholderScale();
}

void AContainerActor::InitAsLooseBox(FName InProductID, bool bInRecorded)
{
	ProductID = InProductID;
	bRecorded = bInRecorded;
	bLooseBox = true;
	AcquiredAtMinute = NowMinute();

	// A pallet breaks into 16 Box M stacks, so that is what you carry.
	ContainerType = EContainerType::BoxM;
	Containers = 1;

	ApplyPlaceholderScale();
}

void AContainerActor::AddToStack(int32 Count, double AcquiredAt)
{
	if (Count <= 0)
	{
		return;
	}

	Containers += Count;
	// Never younger. Same rule as FInventoryEntry::AddBatch, for the same
	// reason: dropping a fresh crate onto old ones would reset the old ones.
	AcquiredAtMinute = FMath::Min(AcquiredAtMinute, AcquiredAt);

	ApplyPlaceholderScale();
}

FVector AContainerActor::PlaceholderSizeMetres(EContainerType Type)
{
	// Engine cube is 100 cm, so metres double as the scale. Roadmap 2.2
	// greybox sizes.
	switch (Type)
	{
	case EContainerType::BoxS:       return FVector(0.3f);
	case EContainerType::BoxM:       return FVector(0.4f);
	case EContainerType::BoxL:       return FVector(0.6f);
	case EContainerType::ColdTote:   return FVector(0.4f);
	case EContainerType::SecureCase: return FVector(0.3f);
	case EContainerType::Crate:      return FVector(0.8f);
	case EContainerType::Pallet:     return FVector(1.2f, 0.8f, 1.4f);
	}
	return FVector(0.4f);
}

void AContainerActor::ApplyPlaceholderScale()
{
	if (!MeshComponent)
	{
		return;
	}

	if (Containers <= 1)
	{
		MeshComponent->SetWorldScale3D(PlaceholderSizeMetres(ContainerType));
		return;
	}

	// A stack on the forks is drawn as a pallet filling up rather than as one
	// box scaled sixteen times taller: the player needs to see how much more
	// will fit, and a six-metre column tells them nothing.
	const FVector Pallet = PlaceholderSizeMetres(EContainerType::Pallet);
	const float Fraction = FMath::Clamp(
		GetVolumeBU() / FProductRow::ContainerVolumeBU(EContainerType::Pallet), 0.f, 1.f);

	MeshComponent->SetWorldScale3D(FVector(Pallet.X, Pallet.Y,
		FMath::Max(0.2f, Pallet.Z * Fraction)));
}

float AContainerActor::GetVolumeBU() const
{
	return FProductRow::ContainerVolumeBU(ContainerType) * Containers;
}

void AContainerActor::SetCarried(bool bCarried)
{
	// A carried box must not block the player capsule or catch the focus trace.
	MeshComponent->SetCollisionEnabled(bCarried ? ECollisionEnabled::NoCollision
	                                            : ECollisionEnabled::QueryOnly);
}

// --- IInteractable -----------------------------------------------------------

float AContainerActor::GetAgeDays() const
{
	return static_cast<float>(DeadlineCondition::DaysBetween(AcquiredAtMinute, NowMinute()));
}

int32 AContainerActor::GetDaysUntilSpoilage() const
{
	const FProductRow* Row = FindRow();
	return Row ? DeadlineCondition::DaysUntilSpoilage(Row->ShelfLifeDays, AcquiredAtMinute, NowMinute())
	           : -1;
}

float AContainerActor::GetConditionMultiplier() const
{
	const FProductRow* Row = FindRow();
	if (!Row || Row->ObsolescencePerDay <= 0.f)
	{
		return 1.f;
	}
	return DeadlineCondition::ObsolescenceMultiplier(Row->ObsolescencePerDay,
		GetAgeDays(), UDeadlineSettings::Get().ObsolescenceFloor);
}

FText AContainerActor::GetInteractionPrompt_Implementation(APawn* Interactor) const
{
	if (!IsHandCarryable())
	{
		return NSLOCTEXT("Deadline", "ContainerNeedsForklift", "Needs a forklift");
	}

	// Say how long the goods have left while they are in reach. Spoilage that
	// only announces itself once the crate is gone is a tax, not a decision.
	const int32 DaysLeft = GetDaysUntilSpoilage();
	if (DaysLeft >= 0 && DaysLeft <= 7)
	{
		return FText::Format(
			NSLOCTEXT("Deadline", "ContainerPickUpDated", "E - Pick up  ({0} gun kaldi)"),
			FText::AsNumber(DaysLeft));
	}
	return NSLOCTEXT("Deadline", "ContainerPickUp", "E - Pick up");
}

bool AContainerActor::CanInteract_Implementation(APawn* Interactor) const
{
	if (!IsHandCarryable())
	{
		return false;
	}
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	return Character && !Character->IsCarrying();
}

void AContainerActor::Interact_Implementation(APawn* Interactor)
{
	if (ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor))
	{
		Character->PickUp(this);
	}
}
