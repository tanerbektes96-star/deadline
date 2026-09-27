// Copyright DEADLINE. All Rights Reserved.
//
// The travel map (GDD 9.2 / 17): the places you can go, and what each trip
// would cost before you commit to it.
//
// All wiring is in C++ and every child is bound by name, so the Blueprint needs
// no graph. The exact names the Designer must use are in
// Assets/Arsiv/G005_HaritaEkrani_Yerlesim.md.
//
// Markers are not placed by hand. The screen reads DT_Destinations and anchors
// one marker per row at its MapX/MapY — the same two numbers UTravelSubsystem
// uses to price the route — so the picture and the fare can never disagree.
//
// Nothing polls: the sidebar is refilled when the selection changes, and the
// screen closes itself when the truck actually pulls away.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Travel/TravelSubsystem.h"
#include "TravelScreenWidget.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;
class UTravelMarkerWidget;
class UTravelRouteWidget;

UCLASS()
class DEADLINE__API UTravelScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Which destination the sidebar is quoting for. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Travel")
	void SelectDestination(FName DestinationID);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	FName GetSelectedDestination() const { return SelectedDestination; }

	/** Marker widget to spawn per destination. Set this in the Blueprint's
	    Class Defaults — it is the one thing the Designer cannot infer. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Travel")
	TSubclassOf<UTravelMarkerWidget> MarkerWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- Bound widgets. Exact names required. ------------------------------

	/** Holds one UTravelMarkerWidget per destination, anchored by MapX/MapY. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> MarkerCanvas;

	/** The dashed line. Stretch it over the same area as MarkerCanvas. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTravelRouteWidget> RouteView;

	/** Pin card: where you would be going. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DestinationNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DestinationTypeText;

	/** Truck card: which vehicle, and what it is carrying. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> VehicleNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> VehicleLoadText;

	/** Clock card: how long, and what time you arrive. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TravelTimeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ArrivalTimeText;

	/** Road card: how far, and what the fuel costs. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DistanceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> FuelText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	/** Optional: the screen still works without them. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CurrentLocationText;

	/** Says "rush hour" when the clock is charging you for it, so the extra
	    35% is explained rather than just felt (GDD 9.4). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TrafficText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmLabelText;

	/** Why the trip is refused, when it is. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION() void HandleConfirmClicked();
	UFUNCTION() void HandleCloseClicked();
	UFUNCTION() void HandleMarkerClicked(FName DestinationID);
	UFUNCTION() void HandleTravelStarted(const FTravelQuote& Quote);

	/** One marker per row of DT_Destinations. Only on open: the map does not
	    change while you are looking at it. */
	void BuildMarkers();

	/** Put the current quote into the four sidebar cards. */
	void RefreshQuote();

	/** Which truck this trip is for. Set by the vehicle you pressed E on. */
	FName ResolveVehicleKey() const;

	UTravelSubsystem* GetTravel() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTravelMarkerWidget>> Markers;

	FName SelectedDestination;
};
