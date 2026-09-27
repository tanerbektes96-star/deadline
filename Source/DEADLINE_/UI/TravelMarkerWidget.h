// Copyright DEADLINE. All Rights Reserved.
//
// One destination pin on the travel map.
//
// Every child is bound by name (CLAUDE.md UMG pattern), so the Blueprint needs
// no graph: put widgets with these exact names in the Designer and the engine
// wires them up. A missing name is a Blueprint compile error, which is the
// point — it fails loudly instead of silently showing nothing.
//
// The marker is placed by the screen, not by the Designer: its anchor comes
// from the destination's MapX/MapY, the same two numbers that price the route.
// So moving a place on the map moves it in the fare as well, and the two can
// never disagree.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Data/DestinationRow.h"
#include "TravelMarkerWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTravelMarkerClicked, FName, DestinationID);

UCLASS()
class DEADLINE__API UTravelMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fill the marker in. Called by UTravelScreenWidget, not by Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Travel")
	void SetDestination(FName InDestinationID, const FDestinationRow& Row, bool bInIsCurrent);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	FName GetDestinationID() const { return DestinationID; }

	UFUNCTION(BlueprintCallable, Category = "Deadline|Travel")
	void SetSelected(bool bInSelected);

	/** Category colour for the dot. Never the only signal: the label spells the
	    place out and the sidebar names its type (UI prompts 1.2). */
	static FLinearColor CategoryColour(FName Category);

	FOnTravelMarkerClicked OnMarkerClicked;

protected:
	virtual void NativeConstruct() override;

	// --- Bound widgets. Name them exactly this in the Designer. ------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> MarkerButton;

	/** The coloured dot. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MarkerDot;

	/** Drawn around the selected marker, and around where you are standing. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> MarkerRing;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MarkerLabel;

private:
	UFUNCTION()
	void HandleClicked();

	void ApplyVisualState();

	FName DestinationID;
	bool bSelected = false;

	/** True for the place the player is standing in right now. */
	bool bIsCurrent = false;
};
