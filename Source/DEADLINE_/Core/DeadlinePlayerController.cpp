// Copyright DEADLINE. All Rights Reserved.

#include "Core/DeadlinePlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Core/DeadlineCheatManager.h"
#include "Core/DeadlineSettings.h"
#include "Engine/GameInstance.h"
#include "Forecast/ForecastSubsystem.h"
#include "Travel/TravelSubsystem.h"
#include "UI/CommitmentResultWidget.h"

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
		if (UForecastSubsystem* Forecast = GI->GetSubsystem<UForecastSubsystem>())
		{
			Forecast->OnCommitmentResolved.AddUniqueDynamic(this, &ADeadlinePlayerController::HandleCommitmentResolved);
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
		if (UForecastSubsystem* Forecast = GI->GetSubsystem<UForecastSubsystem>())
		{
			Forecast->OnCommitmentResolved.RemoveDynamic(this, &ADeadlinePlayerController::HandleCommitmentResolved);
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

bool ADeadlinePlayerController::IsForecastScreenOpen() const
{
	return ForecastScreen != nullptr && ForecastScreen->IsInViewport();
}

void ADeadlinePlayerController::OpenForecastScreen()
{
	OpenScreen(UDeadlineSettings::Get().ForecastScreenWidget, TEXT("Forecast Screen Widget"), ForecastScreen);
}

void ADeadlinePlayerController::CloseForecastScreen()
{
	CloseScreen(ForecastScreen);
}

void ADeadlinePlayerController::ToggleForecastScreen()
{
	if (IsForecastScreenOpen())
	{
		CloseForecastScreen();
	}
	else
	{
		OpenForecastScreen();
	}
}

// --- Result cards -----------------------------------------------------------------

void ADeadlinePlayerController::HandleCommitmentResolved(const FForecastCommitment& Commitment)
{
	ShowResultCard(Commitment.ID);
}

bool ADeadlinePlayerController::IsResultCardOpen() const
{
	return ResultCard != nullptr && ResultCard->IsInViewport();
}

void ADeadlinePlayerController::ShowResultCard(int32 CommitmentID)
{
	ResultQueue.AddUnique(CommitmentID);
	ShowNextResult();
}

void ADeadlinePlayerController::ShowNextResult()
{
	const UGameInstance* GI = GetGameInstance();
	const UForecastSubsystem* Forecast = GI ? GI->GetSubsystem<UForecastSubsystem>() : nullptr;
	while (!IsResultCardOpen() && ResultQueue.Num() > 0 && Forecast)
	{
		FForecastCommitment Commitment;
		const bool bFound = Forecast->GetCommitment(ResultQueue[0], Commitment);
		ResultQueue.RemoveAt(0);
		if (!bFound || Commitment.State != ECommitmentState::Resolved)
		{
			continue;
		}
		OpenScreen(UDeadlineSettings::Get().CommitmentResultWidget, TEXT("Commitment Result Widget"), ResultCard);
		if (UCommitmentResultWidget* Card = Cast<UCommitmentResultWidget>(ResultCard))
		{
			Card->SetCommitment(Commitment, ResultQueue.Num());
		}
		UE_LOG(LogTemp, Log, TEXT("[Deadline] Result card: commitment #%d, %s, %+.0f."),
			Commitment.ID, *UEnum::GetValueAsString(Commitment.Lesson), Commitment.Result);
		return;
	}
}

void ADeadlinePlayerController::CloseResultCard()
{
	CloseScreen(ResultCard);
	if (ResultQueue.Num() > 0)
	{
		ShowNextResult();
		return;
	}
	// The card may have come up over another screen: hand that one the mouse
	// back rather than dropping the player into walking mode under it.
	for (UUserWidget* Under : { MarketScreen.Get(), TravelScreen.Get(), ForecastScreen.Get() })
	{
		if (Under && Under->IsInViewport())
		{
			bShowMouseCursor = true;
			FInputModeGameAndUI Mode;
			Mode.SetWidgetToFocus(Under->TakeWidget());
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Mode.SetHideCursorDuringCapture(false);
			SetInputMode(Mode);
			break;
		}
	}
}
