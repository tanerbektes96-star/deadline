// Copyright DEADLINE. All Rights Reserved.

#include "Actors/DestinationActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ADestinationActor::ADestinationActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	PadMesh->SetupAttachment(Root);
	// You stand on it when you arrive, so it has to hold you up.
	PadMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PadMesh->SetCollisionResponseToAllChannels(ECR_Block);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		PadMesh->SetStaticMesh(CubeMesh.Object);
	}
	// A flat 8 x 5 m apron: big enough to park the largest van on.
	PadMesh->SetRelativeScale3D(FVector(8.f, 5.f, 0.08f));
	PadMesh->SetRelativeLocation(FVector(0.f, 0.f, 4.f));

	VehicleParkPoint = CreateDefaultSubobject<USceneComponent>(TEXT("VehiclePark"));
	VehicleParkPoint->SetupAttachment(Root);

	PlayerArrivalPoint = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerArrival"));
	PlayerArrivalPoint->SetupAttachment(Root);
	// Off the tail of where the truck will sit, which is where you would be
	// standing anyway to open the back.
	PlayerArrivalPoint->SetRelativeLocation(FVector(-380.f, 0.f, 95.f));
}

FVector ADestinationActor::GetVehicleParkLocation() const
{
	return VehicleParkPoint ? VehicleParkPoint->GetComponentLocation() : GetActorLocation();
}

FRotator ADestinationActor::GetVehicleParkRotation() const
{
	return VehicleParkPoint ? VehicleParkPoint->GetComponentRotation() : GetActorRotation();
}

FVector ADestinationActor::GetPlayerArrivalLocation() const
{
	return PlayerArrivalPoint ? PlayerArrivalPoint->GetComponentLocation() : GetActorLocation();
}
