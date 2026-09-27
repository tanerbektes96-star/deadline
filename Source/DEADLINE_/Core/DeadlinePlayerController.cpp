// Copyright DEADLINE. All Rights Reserved.

#include "Core/DeadlinePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Core/DeadlineCheatManager.h"
#include "Core/DeadlineSettings.h"
#include "Engine/GameInstance.h"
#include "Travel/TravelSubsystem.h"

ADeadlinePlayerController::ADeadlinePlayerController()
{
	CheatClass = UDeadlineCheatManager::StaticClass();
}

void ADeadlinePlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UTravelSubsystem* Travel = GI->GetSubsystem<UTravelSubsystem>())
		{
			Travel->OnTravelStarted.AddUniqueDynamic(this, &ADeadlinePlayerController::HandleTravelStarted);
			Travel->OnTravelFinished.AddUniqueDynamic(this, &ADeadlinePlayerController::HandleTravelFinished);
		}
	}
}

void ADeadlinePlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UTravelSubsystem* Travel = GI->GetSubsystem<UTravelSubsystem>())
		{
			Travel->OnTravelStarted.RemoveDynamic(this, &ADeadlinePlayerController::HandleTravelStarted);
			Travel->OnTravelFinished.RemoveDynamic(this, &ADeadlinePlayerController::HandleTravelFinished);
		}
	}
	Super::EndPlay(Reason);
}

void ADeadlinePlayerController::HandleTravelStarted(const FTravelQuote& Quote)
{
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);
}

void ADeadlinePlayerController::HandleTravelFinished(FName ArrivedAt)
{
	// Symmetric: these are counters, not flags.
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
}

// --- Screens ------------------------------------------------------------------

void ADeadlinePlayerController::OpenScreen(const TSoftClassPtr<UUserWidget>& ScreenClass,
	const TCHAR* SettingName, TObjectPtr<UUserWidget>& OutScreen)
{
	if (OutScreen && OutScreen->IsInViewport())
	{
		return;
	}

	UClass* Loaded = ScreenClass.LoadSynchronous();
	if (!Loaded)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Deadline] No %s set. Project Settings > Game > Deadline, or run the ")
			TEXT("month's setup script in Content/Deadline/Core."), SettingName);
		return;
	}

	// Rebuilt rather than kept hidden: a screen fills itself in NativeConstruct,
	// so a fresh one is always current.
	OutScreen = CreateWidget<UUserWidget>(this, Loaded);
	if (!OutScreen)
	{
		return;
	}

	OutScreen->AddToViewport();

	bShowMouseCursor = true;
	// GameAndUI, not UIOnly: the clock keeps running while a screen is up, and
	// walking away from a counter mid-trade should still be possible.
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(OutScreen->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

void ADeadlinePlayerController::CloseScreen(TObjectPtr<UUserWidget>& Screen)
{
	if (Screen)
	{
		Screen->RemoveFromParent();
		Screen = nullptr;
	}

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

bool ADeadlinePlayerController::IsMarketScreenOpen() const
{
	return MarketScreen != nullptr && MarketScreen->IsInViewport();
}

void ADeadlinePlayerController::OpenMarketScreen()
{
	OpenScreen(UDeadlineSettings::Get().MarketScreenWidget, TEXT("Market Screen Widget"), MarketScreen);
}

void ADeadlinePlayerController::CloseMarketScreen()
{
	CloseScreen(MarketScreen);
}

void ADeadlinePlayerController::ToggleMarketScreen()
{
	if (IsMarketScreenOpen())
	{
		CloseMarketScreen();
	}
	else
	{
		OpenMarketScreen();
	}
}

bool ADeadlinePlayerController::IsTravelScreenOpen() const
{
	return TravelScreen != nullptr && TravelScreen->IsInViewport();
}

void ADeadlinePlayerController::OpenTravelScreen(FName VehicleKey)
{
	// Set before the widget is built: it reads this in NativeConstruct.
	TravelVehicleKey = VehicleKey;
	OpenScreen(UDeadlineSettings::Get().TravelScreenWidget, TEXT("Travel Screen Widget"), TravelScreen);
}

void ADeadlinePlayerController::CloseTravelScreen()
{
	CloseScreen(TravelScreen);
}
