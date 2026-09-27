// Copyright DEADLINE. All Rights Reserved.

#include "UI/TravelScreenWidget.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Core/DeadlinePlayerController.h"
#include "Core/TimeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Fleet/FleetSubsystem.h"
#include "UI/DeadlineUIPalette.h"
#include "UI/TravelMarkerWidget.h"
#include "UI/TravelRouteWidget.h"

namespace
{
	template <typename T>
	T* GetSub(const UWidget* Widget)
	{
		const UGameInstance* GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<T>() : nullptr;
	}

	/** "14:25", from a minute-of-day that may run past midnight. */
	FString ClockAt(double MinuteOfDay)
	{
		const int32 Wrapped = FMath::RoundToInt(FMath::Fmod(MinuteOfDay, 1440.0));
		return FString::Printf(TEXT("%02d:%02d"), Wrapped / 60, Wrapped % 60);
	}
}

UTravelSubsystem* UTravelScreenWidget::GetTravel() const
{
	return GetSub<UTravelSubsystem>(this);
}

FName UTravelScreenWidget::ResolveVehicleKey() const
{
	// The truck you pressed E on. Falling back to the first one in the fleet
	// keeps Dl_OpenMap useful before there is a second vehicle to confuse it.
	if (const ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOwningPlayer()))
	{
		if (!PC->GetTravelVehicleKey().IsNone())
		{
			return PC->GetTravelVehicleKey();
		}
	}
	const UFleetSubsystem* Fleet = GetSub<UFleetSubsystem>(this);
	const TArray<FName> Keys = Fleet ? Fleet->GetVehicleKeys() : TArray<FName>();
	return Keys.Num() > 0 ? Keys[0] : NAME_None;
}

void UTravelScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddUniqueDynamic(this, &UTravelScreenWidget::HandleConfirmClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UTravelScreenWidget::HandleCloseClicked);
	}
	if (TitleText)
	{
		TitleText->SetText(NSLOCTEXT("Deadline", "TravelTitle", "HARITA"));
	}
	if (ConfirmLabelText)
	{
		ConfirmLabelText->SetText(NSLOCTEXT("Deadline", "TravelGo", "SEYAHAT ET"));
	}

	if (UTravelSubsystem* Travel = GetTravel())
	{
		Travel->OnTravelStarted.AddUniqueDynamic(this, &UTravelScreenWidget::HandleTravelStarted);
	}

	BuildMarkers();

	// Nothing is preselected. Picking one for the player put the amber ring on
	// wherever they had just driven in from, which read as "you are going back
	// there" -- worse than an empty sidebar, and the empty state says
	// "Bir hedef seç" rather than showing a fabricated zero-kilometre trip.
	SelectDestination(NAME_None);
}

void UTravelScreenWidget::NativeDestruct()
{
	if (UTravelSubsystem* Travel = GetTravel())
	{
		Travel->OnTravelStarted.RemoveDynamic(this, &UTravelScreenWidget::HandleTravelStarted);
	}
	Super::NativeDestruct();
}

// --- Markers ------------------------------------------------------------------

void UTravelScreenWidget::BuildMarkers()
{
	if (!MarkerCanvas || !MarkerWidgetClass)
	{
		if (!MarkerWidgetClass)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Deadline] WBP_TravelScreen has no MarkerWidgetClass set; the map will be empty."));
		}
		return;
	}

	MarkerCanvas->ClearChildren();
	Markers.Reset();

	const UTravelSubsystem* Travel = GetTravel();
	if (!Travel)
	{
		return;
	}

	const FName Here = Travel->GetCurrentLocation();
	for (const FName& ID : Travel->GetDestinationIDs())
	{
		FDestinationRow Row;
		if (!Travel->GetDestination(ID, Row))
		{
			continue;
		}

		UTravelMarkerWidget* Marker = CreateWidget<UTravelMarkerWidget>(this, MarkerWidgetClass);
		if (!Marker)
		{
			continue;
		}
		Marker->SetDestination(ID, Row, /*bIsCurrent=*/ID == Here);
		Marker->OnMarkerClicked.AddUniqueDynamic(this, &UTravelScreenWidget::HandleMarkerClicked);

		// Anchored, not positioned: the marker lands on the same relative spot
		// whatever size the map panel ends up.
		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(MarkerCanvas->AddChild(Marker)))
		{
			MarkerSlot->SetAnchors(FAnchors(Row.MapX, Row.MapY, Row.MapX, Row.MapY));
			MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			MarkerSlot->SetPosition(FVector2D::ZeroVector);
			MarkerSlot->SetAutoSize(true);
		}
		Markers.Add(Marker);
	}
}

void UTravelScreenWidget::SelectDestination(FName DestinationID)
{
	SelectedDestination = DestinationID;

	for (UTravelMarkerWidget* Marker : Markers)
	{
		if (Marker)
		{
			Marker->SetSelected(Marker->GetDestinationID() == DestinationID);
		}
	}

	RefreshQuote();
}

// --- The sidebar --------------------------------------------------------------

void UTravelScreenWidget::RefreshQuote()
{
	const UTravelSubsystem* Travel = GetTravel();
	const UFleetSubsystem* Fleet = GetSub<UFleetSubsystem>(this);
	const UTimeSubsystem* Time = GetSub<UTimeSubsystem>(this);
	if (!Travel || !Fleet)
	{
		return;
	}

	const FName VehicleKey = ResolveVehicleKey();
	const FTravelQuote Quote = Travel->GetQuote(VehicleKey, SelectedDestination);

	if (CurrentLocationText)
	{
		FDestinationRow Here;
		CurrentLocationText->SetText(Travel->GetDestination(Travel->GetCurrentLocation(), Here)
			? FText::FromString(Here.NameTR)
			: FText::GetEmpty());
	}

	// --- Pin card ---
	FDestinationRow To;
	const bool bHaveTo = Travel->GetDestination(SelectedDestination, To);
	if (DestinationNameText)
	{
		DestinationNameText->SetText(bHaveTo ? FText::FromString(To.NameTR)
		                                     : NSLOCTEXT("Deadline", "TravelPick", "Bir hedef seç"));
	}
	if (DestinationTypeText)
	{
		DestinationTypeText->SetText(bHaveTo ? FText::FromName(To.Category) : FText::GetEmpty());
	}

	// --- Truck card ---
	const FName VehicleID = Fleet->GetVehicleID(VehicleKey);
	FVehicleRow Vehicle;
	const bool bHaveVehicle = Fleet->GetVehicleRow(VehicleID, Vehicle);
	if (VehicleNameText)
	{
		VehicleNameText->SetText(bHaveVehicle ? FText::FromString(Vehicle.NameTR)
		                                      : NSLOCTEXT("Deadline", "TravelNoVan", "Araç yok"));
	}
	if (VehicleLoadText)
	{
		VehicleLoadText->SetText(FText::FromString(FString::Printf(TEXT("%.1f / %.0f BU   %.0f kg"),
			Fleet->GetUsedBU(VehicleKey), Fleet->GetCapacityBU(VehicleKey),
			Fleet->GetWeightKg(VehicleKey))));
	}

	// --- Clock card ---
	// With nothing picked the cards show dashes, not zeroes: "0 dk, 0 km, $0"
	// looks like a free trip rather than like no trip at all.
	const FText Dash = NSLOCTEXT("Deadline", "TravelDash", "—");

	if (TravelTimeText)
	{
		TravelTimeText->SetText(bHaveTo
			? FText::FromString(FString::Printf(TEXT("%.0f dk"), Quote.TravelMinutes))
			: Dash);
	}
	if (ArrivalTimeText)
	{
		ArrivalTimeText->SetText(bHaveTo && Time
			? FText::FromString(FString::Printf(TEXT("varış %s"),
				*ClockAt(Time->GetMinuteOfDay() + Quote.TravelMinutes)))
			: FText::GetEmpty());
	}
	if (TrafficText && !bHaveTo)
	{
		TrafficText->SetText(FText::GetEmpty());
	}
	else if (TrafficText)
	{
		// Spelled out, not just priced in: a trip that is 35% longer because it
		// is half past eight should say so.
		const bool bRush = Quote.TrafficMultiplier > 1.f;
		TrafficText->SetText(bRush
			? FText::FromString(FString::Printf(TEXT("yoğun saat  x%.2f"), Quote.TrafficMultiplier))
			: NSLOCTEXT("Deadline", "TravelClearRoad", "yol açık"));
		TrafficText->SetColorAndOpacity(FSlateColor(bRush ? DeadlineUI::Warn : DeadlineUI::Muted));
	}

	// --- Road card ---
	if (DistanceText)
	{
		DistanceText->SetText(bHaveTo
			? FText::FromString(FString::Printf(TEXT("%.1f km"), Quote.DistanceKm))
			: Dash);
	}
	if (FuelText)
	{
		FuelText->SetText(bHaveTo
			? FText::FromString(FString::Printf(TEXT("%.1f L   $%.0f"), Quote.FuelLitres, Quote.FuelCost))
			: FText::GetEmpty());
	}

	// --- Confirm ---
	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(Quote.bPossible);
	}
	if (StatusText)
	{
		StatusText->SetText(bHaveTo ? Quote.Reason : FText::GetEmpty());
		StatusText->SetColorAndOpacity(FSlateColor(DeadlineUI::Warn));
	}

	// --- Route ---
	FDestinationRow From;
	if (RouteView && bHaveTo && Travel->GetDestination(Travel->GetCurrentLocation(), From))
	{
		RouteView->SetRoute(FVector2D(From.MapX, From.MapY), FVector2D(To.MapX, To.MapY));
	}
	else if (RouteView)
	{
		RouteView->ClearRoute();
	}
}

// --- Input --------------------------------------------------------------------

void UTravelScreenWidget::HandleMarkerClicked(FName DestinationID)
{
	SelectDestination(DestinationID);
}

void UTravelScreenWidget::HandleConfirmClicked()
{
	UTravelSubsystem* Travel = GetTravel();
	if (!Travel)
	{
		return;
	}

	// The screen does not close itself here: OnTravelStarted does, so the map
	// also goes away when something else sends the truck off.
	if (!Travel->BeginTravel(ResolveVehicleKey(), SelectedDestination))
	{
		RefreshQuote();   // the reason will have changed
	}
}

void UTravelScreenWidget::HandleTravelStarted(const FTravelQuote& Quote)
{
	HandleCloseClicked();
}

void UTravelScreenWidget::HandleCloseClicked()
{
	if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOwningPlayer()))
	{
		PC->CloseTravelScreen();
	}
}
