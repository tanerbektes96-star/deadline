// Copyright DEADLINE. All Rights Reserved.

#include "UI/TravelMarkerWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/DeadlineUIPalette.h"

void UTravelMarkerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MarkerButton)
	{
		MarkerButton->OnClicked.AddUniqueDynamic(this, &UTravelMarkerWidget::HandleClicked);
	}
}

FLinearColor UTravelMarkerWidget::CategoryColour(FName Category)
{
	// The six supply channels of GDD 6.1 boil down to four kinds of place.
	if (Category == TEXT("Home"))     { return DeadlineUI::Body; }
	if (Category == TEXT("Supplier")) { return DeadlineUI::Accent; }
	if (Category == TEXT("Buyer"))    { return DeadlineUI::Profit; }
	if (Category == TEXT("Port"))     { return DeadlineUI::Warn; }
	if (Category == TEXT("Grey"))     { return DeadlineUI::Loss; }
	return DeadlineUI::Muted;
}

void UTravelMarkerWidget::SetDestination(FName InDestinationID, const FDestinationRow& Row,
	bool bInIsCurrent)
{
	DestinationID = InDestinationID;
	bIsCurrent = bInIsCurrent;

	if (MarkerLabel)
	{
		MarkerLabel->SetText(FText::FromString(Row.NameTR.IsEmpty() ? Row.NameEN : Row.NameTR));
	}
	if (MarkerDot)
	{
		MarkerDot->SetBrushColor(CategoryColour(Row.Category));
	}

	ApplyVisualState();
}

void UTravelMarkerWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	ApplyVisualState();
}

void UTravelMarkerWidget::ApplyVisualState()
{
	if (!MarkerRing)
	{
		return;
	}

	// Three states, and the ring alone tells them apart: the place you are
	// standing in is ringed off-white, the one you are considering is ringed
	// amber, everything else has no ring at all. Selection is never signalled
	// by colour alone -- the sidebar names whatever is selected.
	if (bSelected)
	{
		MarkerRing->SetBrushColor(DeadlineUI::Warn);
		MarkerRing->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else if (bIsCurrent)
	{
		MarkerRing->SetBrushColor(DeadlineUI::Body);
		MarkerRing->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		MarkerRing->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UTravelMarkerWidget::HandleClicked()
{
	OnMarkerClicked.Broadcast(DestinationID);
}
