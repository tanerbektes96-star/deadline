// Copyright DEADLINE. All Rights Reserved.

#include "UI/MarketRowWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/DeadlineUIPalette.h"

namespace
{
	/** Risk read as a gradient: calm teal through blue and amber to red. */
	FLinearColor RiskBandColour(ERiskBand Band)
	{
		switch (Band)
		{
		case ERiskBand::Stable:   return DeadlineUI::Profit;
		case ERiskBand::Reactive: return DeadlineUI::Accent;
		case ERiskBand::Volatile: return DeadlineUI::Warn;
		case ERiskBand::Crisis:   return DeadlineUI::Loss;
		}
		return DeadlineUI::Line;
	}

	FText RiskBandLabel(ERiskBand Band)
	{
		switch (Band)
		{
		case ERiskBand::Stable:   return NSLOCTEXT("Deadline", "BandStable",   "Durağan");
		case ERiskBand::Reactive: return NSLOCTEXT("Deadline", "BandReactive", "Tepkili");
		case ERiskBand::Volatile: return NSLOCTEXT("Deadline", "BandVolatile", "Oynak");
		case ERiskBand::Crisis:   return NSLOCTEXT("Deadline", "BandCrisis",   "Kriz");
		}
		return FText::GetEmpty();
	}
}

void UMarketRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddUniqueDynamic(this, &UMarketRowWidget::HandleSelectClicked);
	}
}

void UMarketRowWidget::HandleSelectClicked()
{
	OnRowClicked.Broadcast(ProductID);
}

void UMarketRowWidget::SetRow(const FMarketQuote& Quote, const FString& DisplayName, int32 ContainersHeld)
{
	ProductID = Quote.ProductID;

	if (ProductNameText)
	{
		// The product ID (S01, H06) is a data key, not something a player
		// should have to read. It still identifies rows in the log and in the
		// Dl_* cheats, which is where it belongs.
		ProductNameText->SetText(FText::FromString(DisplayName));
	}
	if (PriceText)
	{
		PriceText->SetText(FText::FromString(FString::Printf(TEXT("$%.2f"), Quote.Price)));
	}
	if (MultipleText)
	{
		MultipleText->SetText(FText::FromString(FString::Printf(TEXT("x%.2f"), Quote.Multiple)));
	}
	if (ChangeText)
	{
		// The sign carries the direction, so the colour is decoration rather
		// than the only signal (UI prompts 1.2, colour blindness). Deliberately
		// plain ASCII: arrowhead glyphs render as empty boxes in fonts that do
		// not carry U+25B2/U+25BC, which is most UI fonts including Roboto.
		ChangeText->SetText(FText::FromString(
			FString::Printf(TEXT("%+.1f%%"), Quote.DayChangePercent)));
		ChangeText->SetColorAndOpacity(FSlateColor(
			Quote.Trend == EPriceTrend::Rising  ? DeadlineUI::Profit :
			Quote.Trend == EPriceTrend::Falling ? DeadlineUI::Loss   : DeadlineUI::Muted));
	}
	if (HeldText)
	{
		// "3 kutu" rather than a bare 3: on the second line, next to the risk
		// band, a lone number reads as part of the band label.
		HeldText->SetText(ContainersHeld > 0
			? FText::Format(NSLOCTEXT("Deadline", "RowHeld", "{0} kutu"), FText::AsNumber(ContainersHeld))
			: FText::GetEmpty());
		HeldText->SetColorAndOpacity(FSlateColor(ContainersHeld > 0 ? DeadlineUI::Body : DeadlineUI::Muted));
	}
	if (RiskBandText)
	{
		RiskBandText->SetText(RiskBandLabel(Quote.RiskBand));
	}
	if (BandSwatch)
	{
		BandSwatch->SetBrushColor(RiskBandColour(Quote.RiskBand));
	}
}

void UMarketRowWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	if (ProductNameText)
	{
		// Raised-panel text vs body text. No shadow, no border change
		// (DEADLINE_UI_Prompts 1.4 says separate layers by tone, not shadow).
		ProductNameText->SetColorAndOpacity(FSlateColor(bSelected ? DeadlineUI::Profit : DeadlineUI::Body));
	}
}
