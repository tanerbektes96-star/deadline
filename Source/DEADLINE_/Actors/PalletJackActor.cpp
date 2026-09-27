// Copyright DEADLINE. All Rights Reserved.

#include "Actors/PalletJackActor.h"

#include "Actors/ContainerActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Player/DeadlinePlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Greybox dimensions in metres. Shaped like a hand pallet truck rather than
	// scaled like one: a 60 cm cube is invisible in a warehouse made of grey
	// cubes, and the first thing anyone said about this was that they could not
	// find it. A thin upright handle over flat forks reads at a distance.
	constexpr float BodyLengthM = 0.18f;
	constexpr float BodyWidthM  = 0.18f;
	constexpr float BodyHeightM = 1.25f;

	/** The crossbar you push. Sticks out sideways so the silhouette is a T. */
	constexpr float HandleLengthM = 0.14f;
	constexpr float HandleWidthM  = 0.62f;
	constexpr float HandleHeightM = 0.12f;

	constexpr float ForkLengthM = 1.2f;
	constexpr float ForkWidthM  = 0.7f;
	constexpr float ForkHeightM = 0.1f;
}

APalletJackActor::APalletJackActor()
{
	// Nothing to do per frame. It is attached to the player or it is parked.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));

	// The handle end, where the player stands.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	BodyMesh->SetupAttachment(Root);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BodyMesh->SetRelativeScale3D(FVector(BodyLengthM, BodyWidthM, BodyHeightM));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, BodyHeightM * 50.f));

	// The forks, sticking out forwards at floor level.
	ForkMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Forks"));
	ForkMesh->SetupAttachment(Root);
	ForkMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForkMesh->SetRelativeScale3D(FVector(ForkLengthM, ForkWidthM, ForkHeightM));
	ForkMesh->SetRelativeLocation(FVector(
		(BodyLengthM * 0.5f + ForkLengthM * 0.5f) * 100.f, 0.f, ForkHeightM * 50.f));

	HandleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Handle"));
	HandleMesh->SetupAttachment(Root);
	HandleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HandleMesh->SetRelativeScale3D(FVector(HandleLengthM, HandleWidthM, HandleHeightM));
	HandleMesh->SetRelativeLocation(FVector(0.f, 0.f, BodyHeightM * 100.f));

	if (CubeMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CubeMesh.Object);
		ForkMesh->SetStaticMesh(CubeMesh.Object);
		HandleMesh->SetStaticMesh(CubeMesh.Object);
	}

	// A pallet rests on top of the forks, not inside them.
	LoadSocket = CreateDefaultSubobject<USceneComponent>(TEXT("LoadSocket"));
	LoadSocket->SetupAttachment(Root);
	LoadSocket->SetRelativeLocation(FVector(
		(BodyLengthM * 0.5f + ForkLengthM * 0.5f) * 100.f, 0.f, ForkHeightM * 100.f));

	LoadMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Load"));
	LoadMesh->SetupAttachment(LoadSocket);
	LoadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CubeMesh.Succeeded())
	{
		LoadMesh->SetStaticMesh(CubeMesh.Object);
	}
}

void APalletJackActor::RefreshLoadVisual(const AContainerActor* Load)
{
	if (!LoadMesh)
	{
		return;
	}

	LoadMesh->ClearInstances();
	if (!Load || Load->Containers <= 0)
	{
		return;
	}

	const FVector SizeM = AContainerActor::PlaceholderSizeMetres(Load->ContainerType);
	const FVector SizeCm = SizeM * 100.f;

	// Fill the fork bed before going up, the way anybody stacks a pallet.
	const int32 Columns = FMath::Max(1, FMath::FloorToInt(ForkLengthM * 100.f / SizeCm.X));
	const int32 Rows = FMath::Max(1, FMath::FloorToInt(ForkWidthM * 100.f / SizeCm.Y));

	for (int32 Index = 0; Index < Load->Containers; ++Index)
	{
		const int32 Column = Index % Columns;
		const int32 Row = (Index / Columns) % Rows;
		const int32 Level = Index / (Columns * Rows);

		const FVector Offset(
			(Column - (Columns - 1) * 0.5f) * SizeCm.X,
			(Row - (Rows - 1) * 0.5f) * SizeCm.Y,
			SizeCm.Z * (Level + 0.5f));

		LoadMesh->AddInstance(FTransform(FRotator::ZeroRotator, Offset, SizeM));
	}
}

bool APalletJackActor::Take(ADeadlinePlayerCharacter* Character)
{
	if (!Character || IsHeld())
	{
		return false;
	}

	Holder = Character;
	SetActorEnableCollision(false);
	return true;
}

bool APalletJackActor::Park(ADeadlinePlayerCharacter* Character)
{
	if (Holder != Character)
	{
		return false;
	}

	Holder = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorEnableCollision(true);

	// Sit it flat on the floor wherever it was let go, facing the way the
	// player was facing. No physics, so this is the whole of "put it down".
	FVector Where = GetActorLocation();
	Where.Z = Character ? Character->GetActorLocation().Z
		- Character->GetSimpleCollisionHalfHeight() : Where.Z;
	SetActorLocation(Where);
	SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	return true;
}

// --- Interaction --------------------------------------------------------------

FText APalletJackActor::GetInteractionPrompt_Implementation(APawn* Interactor) const
{
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	if (!Character)
	{
		return FText::GetEmpty();
	}

	if (Character->HasPalletJack())
	{
		return Character->IsCarrying()
			? NSLOCTEXT("Deadline", "JackLoaded", "Unload the pallet first")
			: NSLOCTEXT("Deadline", "JackPark", "E - Park the pallet jack");
	}

	// Both hands go on the handle.
	return Character->IsCarrying()
		? NSLOCTEXT("Deadline", "JackHandsFull", "Both hands needed for the jack")
		: NSLOCTEXT("Deadline", "JackTake", "E - Take the pallet jack");
}

bool APalletJackActor::CanInteract_Implementation(APawn* Interactor) const
{
	const ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	if (!Character)
	{
		return false;
	}
	// Taking it and parking it both need free hands, for the same reason.
	return !Character->IsCarrying()
		&& (Character->HasPalletJack() ? Character->GetPalletJack() == this : !IsHeld());
}

void APalletJackActor::Interact_Implementation(APawn* Interactor)
{
	ADeadlinePlayerCharacter* Character = Cast<ADeadlinePlayerCharacter>(Interactor);
	if (!Character)
	{
		return;
	}

	if (Character->GetPalletJack() == this)
	{
		Character->ReleasePalletJack();
	}
	else
	{
		Character->TakePalletJack(this);
	}
}
