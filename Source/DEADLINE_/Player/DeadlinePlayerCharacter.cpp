// Copyright DEADLINE. All Rights Reserved.

#include "Player/DeadlinePlayerCharacter.h"

#include "Core/DeadlinePlayerController.h"

#include "Actors/ContainerActor.h"
#include "Actors/PalletJackActor.h"
#include "Actors/StorageZoneActor.h"
#include "Actors/VehicleActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/DeadlineSettings.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Player/Interactable.h"
#include "TimerManager.h"

ADeadlinePlayerCharacter::ADeadlinePlayerCharacter()
{
	// No per-frame work of our own; movement is handled by the movement component.
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// A carried box fills the lower part of the view on purpose (GDD 3.1).
	CarrySocket = CreateDefaultSubobject<USceneComponent>(TEXT("CarrySocket"));
	CarrySocket->SetupAttachment(FirstPersonCamera);
	CarrySocket->SetRelativeLocation(FVector(60.f, 0.f, -30.f));

	// The jack rides on the capsule, not the camera: it is a thing on the floor
	// you are pushing, and it should not pitch up when you look up.
	JackSocket = CreateDefaultSubobject<USceneComponent>(TEXT("JackSocket"));
	JackSocket->SetupAttachment(GetCapsuleComponent());
	JackSocket->SetRelativeLocation(FVector(70.f, 0.f, -88.f));

	bUseControllerRotationYaw = true;
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = false;
		Movement->JumpZVelocity = 0.f;   // no jumping in a warehouse sim
		Movement->AirControl = 0.f;
	}
}

void ADeadlinePlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	const UDeadlineSettings& Settings = UDeadlineSettings::Get();
	SetFieldOfView(Settings.DefaultFOV);
	RefreshMovementSpeed();

	GetWorldTimerManager().SetTimer(
		FocusTimer, this, &ADeadlinePlayerCharacter::UpdateFocus,
		FocusTraceInterval, /*bLoop=*/true);

	// PawnClientRestart and BeginPlay can both run before the player input
	// object exists, so poll briefly until the context actually sticks.
	if (!ApplyInputMappings())
	{
		GetWorldTimerManager().SetTimer(
			InputRetryTimer, this, &ADeadlinePlayerCharacter::RetryInputMappings,
			0.05f, /*bLoop=*/true);
	}
}

void ADeadlinePlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	ApplyInputMappings();
}

bool ADeadlinePlayerCharacter::ApplyInputMappings()
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return false;
	}

	UEnhancedInputLocalPlayerSubsystem* Input =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	if (!Input)
	{
		return false;
	}

	if (!DefaultMappingContext)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Deadline] DefaultMappingContext is not set on %s. Assign IMC_Default on the pawn Blueprint."),
			*GetClass()->GetName());
		return false;
	}

	// Without bForceImmediately the rebuild is only flagged, and the flagged
	// path never reached us: the context was registered but the player input
	// kept an empty mapping table, so no action ever triggered.
	FModifyContextOptions Options;
	Options.bForceImmediately = true;
	Input->AddMappingContext(DefaultMappingContext, 0, Options);

	// AddMappingContext silently does nothing when the player input object does
	// not exist yet, so confirm the result instead of assuming it worked.
	if (!Input->HasMappingContext(DefaultMappingContext))
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[Deadline] Input mappings applied (%s) after %d attempt(s)."),
		*DefaultMappingContext->GetName(), InputMappingAttempts + 1);
	return true;
}

void ADeadlinePlayerCharacter::RetryInputMappings()
{
	if (ApplyInputMappings() || ++InputMappingAttempts > 40)
	{
		GetWorldTimerManager().ClearTimer(InputRetryTimer);
	}
}

void ADeadlinePlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("[Deadline] Enhanced Input is required on the player character."));
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Deadline] Binding input: Move=%s Look=%s Sprint=%s Interact=%s Drop=%s"),
		MoveAction ? TEXT("ok") : TEXT("NULL"),
		LookAction ? TEXT("ok") : TEXT("NULL"),
		SprintAction ? TEXT("ok") : TEXT("NULL"),
		InteractAction ? TEXT("ok") : TEXT("NULL"),
		DropAction ? TEXT("ok") : TEXT("NULL"));

	ApplyInputMappings();

	if (MoveAction)
	{
		Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADeadlinePlayerCharacter::Input_Move);
	}
	if (LookAction)
	{
		Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADeadlinePlayerCharacter::Input_Look);
	}
	if (SprintAction)
	{
		Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ADeadlinePlayerCharacter::Input_SprintStart);
		Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ADeadlinePlayerCharacter::Input_SprintStop);
	}
	if (InteractAction)
	{
		Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ADeadlinePlayerCharacter::Input_Interact);
	}
	if (DropAction)
	{
		Input->BindAction(DropAction, ETriggerEvent::Started, this, &ADeadlinePlayerCharacter::Input_Drop);
	}
	if (MapAction)
	{
		Input->BindAction(MapAction, ETriggerEvent::Started, this, &ADeadlinePlayerCharacter::Input_ToggleMap);
	}
	if (MarketAction)
	{
		Input->BindAction(MarketAction, ETriggerEvent::Started, this, &ADeadlinePlayerCharacter::Input_ToggleMarket);
	}
	if (LoadAction)
	{
		Input->BindAction(LoadAction, ETriggerEvent::Started, this, &ADeadlinePlayerCharacter::Input_Load);
	}
}

void ADeadlinePlayerCharacter::Input_ToggleMap(const FInputActionValue& Value)
{
	ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// The map is a map: it opens wherever you are standing, not only with a
	// hand on a truck. Passing the vehicle you happen to be at keeps the
	// screen quoting the right truck when you are at one.
	if (PC->IsTravelScreenOpen())
	{
		PC->CloseTravelScreen();
	}
	else
	{
		PC->OpenTravelScreen(LoadZoneVehicle ? LoadZoneVehicle->GetVehicleKey() : NAME_None);
	}
}

void ADeadlinePlayerCharacter::Input_ToggleMarket(const FInputActionValue& Value)
{
	// The controller owns the screen: it is the thing that has to swap input
	// modes, and it outlives the pawn.
	if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetController()))
	{
		PC->ToggleMarketScreen();
	}
}

// --- Input --------------------------------------------------------------------

void ADeadlinePlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (Controller && !Axis.IsNearlyZero())
	{
		const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X), Axis.Y);
		AddMovementInput(FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y), Axis.X);
	}
}

void ADeadlinePlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void ADeadlinePlayerCharacter::Input_SprintStart(const FInputActionValue& Value)
{
	bWantsToSprint = true;
	RefreshMovementSpeed();
}

void ADeadlinePlayerCharacter::Input_SprintStop(const FInputActionValue& Value)
{
	bWantsToSprint = false;
	RefreshMovementSpeed();
}

void ADeadlinePlayerCharacter::Input_Interact(const FInputActionValue& Value)
{
	if (FocusedActor && CanInteractWithFocus())
	{
		IInteractable::Execute_Interact(FocusedActor, this);
		UpdateFocus();   // the world just changed under the crosshair
	}
}

void ADeadlinePlayerCharacter::Input_Load(const FInputActionValue& Value)
{
	// F hands over, in both places it works. Taking is E, at the truck bed as
	// well as at a rack: one key cannot do both directions when the goods
	// match, because there is no way to say which you meant.
	if (LoadZoneVehicle && IsCarrying())
	{
		LoadZoneVehicle->TryLoadFrom(this);
		return;
	}
	if (LoadZoneVehicle)
	{
		return;
	}

	// GDD 3.3: "F - Yükleme bölgesine ver (kutuyu araca/rafa aktar)". The rack
	// half of that sentence matters now that E has to mean "take one more onto
	// the forks": with one key doing both, filling a pallet at a rack that
	// still has stock would be impossible to stop.
	if (AStorageZoneActor* Zone = Cast<AStorageZoneActor>(FocusedActor))
	{
		Zone->TryStoreFrom(this);
		UpdateFocus();
	}
}

// --- Loading zones ------------------------------------------------------------

void ADeadlinePlayerCharacter::EnterLoadZone(AVehicleActor* Vehicle)
{
	if (Vehicle)
	{
		LoadZoneVehicle = Vehicle;
	}
}

void ADeadlinePlayerCharacter::LeaveLoadZone(AVehicleActor* Vehicle)
{
	// Only the zone you actually left clears the pointer: walking out of one
	// truck's zone straight into the next must not blank the new one.
	if (LoadZoneVehicle == Vehicle)
	{
		LoadZoneVehicle = nullptr;
	}
}

FText ADeadlinePlayerCharacter::GetLoadPrompt() const
{
	ADeadlinePlayerCharacter* Self = const_cast<ADeadlinePlayerCharacter*>(this);
	if (LoadZoneVehicle)
	{
		return LoadZoneVehicle->GetLoadPrompt(Self);
	}
	// A rack is a place you hand things to as well (GDD 3.3), so it gets the
	// same line under the crosshair the truck bed does.
	if (const AStorageZoneActor* Zone = Cast<AStorageZoneActor>(FocusedActor); Zone && IsCarrying())
	{
		return Zone->GetStorePrompt(Self);
	}
	return FText::GetEmpty();
}

bool ADeadlinePlayerCharacter::CanUseLoadZone() const
{
	ADeadlinePlayerCharacter* Self = const_cast<ADeadlinePlayerCharacter*>(this);
	if (LoadZoneVehicle)
	{
		return LoadZoneVehicle->CanLoadFrom(Self);
	}
	const AStorageZoneActor* Zone = Cast<AStorageZoneActor>(FocusedActor);
	return Zone && IsCarrying() && !Zone->GetStorePrompt(Self).IsEmpty();
}

void ADeadlinePlayerCharacter::Input_Drop(const FInputActionValue& Value)
{
	// Q is "put down what you are holding" (GDD 3.3), and the jack is
	// something you are holding. It also cannot be parked any other way: you
	// push it in front of you below the camera, so the interaction trace can
	// never look at it.
	if (DropCarried())
	{
		return;
	}
	ReleasePalletJack();
}

// --- Movement -----------------------------------------------------------------

void ADeadlinePlayerCharacter::RefreshMovementSpeed()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const UDeadlineSettings& Settings = UDeadlineSettings::Get();

	// You cannot run with a box in your hands, or behind a jack (GDD 3.2).
	const bool bCanSprint = bWantsToSprint && !IsCarrying() && !HasPalletJack();
	float Speed = bCanSprint ? Settings.SprintSpeed : Settings.WalkSpeed;

	if (CarriedContainer && CarriedContainer->IsHandCarryable())
	{
		Speed *= CarriedContainer->GetCarrySpeedMultiplier();
	}
	if (HasPalletJack())
	{
		// The jack sets the pace, loaded or not: what slows you is pushing the
		// thing, and a pallet on it does not change that much.
		Speed *= Settings.PalletJackSpeedMultiplier;
	}

	Movement->MaxWalkSpeed = Speed;
}

void ADeadlinePlayerCharacter::SetFieldOfView(float NewFOV)
{
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetFieldOfView(FMath::Clamp(NewFOV, 70.f, 100.f));
	}
}

// --- Carrying -----------------------------------------------------------------

bool ADeadlinePlayerCharacter::CanCarry(const AContainerActor* Container) const
{
	if (!Container)
	{
		return false;
	}
	// An empty pallet is wood and air: awkward, but a person carries one.
	if (Container->IsHandCarryable())
	{
		return true;
	}
	// Everything else on the forks, and only one thing at a time.
	return HasPalletJack();
}

bool ADeadlinePlayerCharacter::PickUp(AContainerActor* Container)
{
	if (IsCarrying() || !CanCarry(Container))
	{
		return false;
	}

	CarriedContainer = Container;
	Container->SetCarried(true);

	// Both hands are on the handle. Anything you pick up while pushing the jack
	// goes on the forks -- a box appearing in your arms while you steer a
	// pallet truck is exactly the nonsense this tool exists to remove.
	USceneComponent* Socket = PalletJack ? PalletJack->GetLoadSocket()
	                                     : ToRawPtr(CarrySocket);
	Container->AttachToComponent(Socket, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	// On the forks the jack draws the boxes, so the actor's own mesh would be
	// a second copy of the same goods sitting inside the first.
	Container->SetActorHiddenInGame(PalletJack != nullptr);

	RefreshJackLoad();
	RefreshMovementSpeed();
	OnCarryChanged.Broadcast(CarriedContainer);
	return true;
}

void ADeadlinePlayerCharacter::RefreshJackLoad()
{
	if (PalletJack)
	{
		PalletJack->RefreshLoadVisual(CarriedContainer);
	}
}

bool ADeadlinePlayerCharacter::TakePalletJack(APalletJackActor* Jack)
{
	if (!Jack || PalletJack || IsCarrying() || !Jack->Take(this))
	{
		return false;
	}

	PalletJack = Jack;
	Jack->AttachToComponent(JackSocket, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	RefreshJackLoad();
	RefreshMovementSpeed();
	OnCarryChanged.Broadcast(CarriedContainer);
	return true;
}

float ADeadlinePlayerCharacter::GetJackFreeBU() const
{
	if (!HasPalletJack())
	{
		return 0.f;
	}
	const float Capacity = UDeadlineSettings::Get().PalletJackCapacityBU;
	return FMath::Max(0.f, Capacity - (CarriedContainer ? CarriedContainer->GetVolumeBU() : 0.f));
}

bool ADeadlinePlayerCharacter::CanStackOnJack(FName ProductID, bool bLoose, bool bRecorded,
	float VolumeBU) const
{
	if (!HasPalletJack() || !CarriedContainer)
	{
		return false;
	}
	if (CarriedContainer->ProductID != ProductID
		|| CarriedContainer->bLooseBox != bLoose
		|| CarriedContainer->bRecorded != bRecorded)
	{
		return false;
	}
	return VolumeBU <= GetJackFreeBU() + KINDA_SMALL_NUMBER;
}

bool ADeadlinePlayerCharacter::ReleasePalletJack()
{
	// Parking a loaded jack would leave a pallet standing in the middle of the
	// floor with no ledger entry pointing at it. Take the pallet off first.
	if (!PalletJack || IsCarrying())
	{
		return false;
	}

	PalletJack->RefreshLoadVisual(nullptr);
	PalletJack->Park(this);
	PalletJack = nullptr;

	RefreshMovementSpeed();
	OnCarryChanged.Broadcast(CarriedContainer);
	return true;
}

bool ADeadlinePlayerCharacter::DropCarried()
{
	if (!CarriedContainer)
	{
		return false;
	}

	AContainerActor* Container = ReleaseCarried();

	// Place it on the floor just ahead, no physics toss (GDD 10.1).
	const FVector Ahead = GetActorLocation() + GetActorForwardVector() * 120.f;
	const FVector Down = Ahead - FVector(0.f, 0.f, 200.f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Container);

	const FVector Target = GetWorld()->LineTraceSingleByChannel(Hit, Ahead, Down, ECC_Visibility, Params)
		? Hit.ImpactPoint
		: Ahead;

	Container->SetActorLocation(Target);
	Container->SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	return true;
}

AContainerActor* ADeadlinePlayerCharacter::ReleaseCarried()
{
	AContainerActor* Container = CarriedContainer;
	if (!Container)
	{
		return nullptr;
	}

	Container->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Container->SetCarried(false);
	CarriedContainer = nullptr;
	RefreshJackLoad();
	Container->SetActorHiddenInGame(false);

	RefreshMovementSpeed();
	OnCarryChanged.Broadcast(nullptr);
	return Container;
}

// --- Interaction focus ---------------------------------------------------------

void ADeadlinePlayerCharacter::UpdateFocus()
{
	AActor* NewFocus = nullptr;

	if (FirstPersonCamera)
	{
		const float Distance = UDeadlineSettings::Get().InteractionDistance;
		const FVector Start = FirstPersonCamera->GetComponentLocation();
		const FVector End = Start + FirstPersonCamera->GetForwardVector() * Distance;

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		if (CarriedContainer)
		{
			Params.AddIgnoredActor(CarriedContainer);
		}

		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && HitActor->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
			{
				NewFocus = HitActor;
			}
		}
	}

	if (NewFocus != FocusedActor)
	{
		FocusedActor = NewFocus;
		OnFocusChanged.Broadcast(FocusedActor);
	}
}

FText ADeadlinePlayerCharacter::GetFocusPrompt() const
{
	if (!FocusedActor)
	{
		return FText::GetEmpty();
	}
	return IInteractable::Execute_GetInteractionPrompt(FocusedActor, const_cast<ADeadlinePlayerCharacter*>(this));
}

bool ADeadlinePlayerCharacter::CanInteractWithFocus() const
{
	if (!FocusedActor)
	{
		return false;
	}
	return IInteractable::Execute_CanInteract(FocusedActor, const_cast<ADeadlinePlayerCharacter*>(this));
}
