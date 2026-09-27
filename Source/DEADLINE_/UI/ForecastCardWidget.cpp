// Copyright DEADLINE. All Rights Reserved.

#include "UI/ForecastCardWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "UI/DeadlineUIPalette.h"
#include "UI/ForecastFormat.h"

void UForecastCardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CardButton)
	{
		CardButton->OnClicked.AddUniqueDynamic(this, &UForecastCardWidget::HandleClicked);
	}
}

void UForecastCardWidget::HandleClicked()
{
	OnCardClicked.Broadcast(EventID);
}

void UForecastCardWidget::SetEntry(const FForecastEntry& Entry)
{
	EventID = Entry.EventID;
	const bool bSignal = Entry.Status == EForecastStatus::Signal;

	if (NameText)
	{
		NameText->SetText(FText::FromString(Entry.Name));
	}
	if (StatusText)
	{
		FText When;
		if (bSignal)
		{
			When = Entry.DaysAway <= 1
				? NSLOCTEXT("Deadline", "ForecastTomorrow", "SÖYLENTİ · yarın")
				: FText::Format(NSLOCTEXT("Deadline", "ForecastInDays", "SÖYLENTİ · {0} gün sonra"), FText::AsNumber(Entry.DaysAway));
		}
		else
		{
			When = Entry.DaysAway <= 0
				? NSLOCTEXT("Deadline", "ForecastLastDay", "SÜRÜYOR · bugün bitiyor")
				: FText::Format(NSLOCTEXT("Deadline", "ForecastUntil", "SÜRÜYOR · gün {0}'e kadar"), FText::AsNumber(Entry.Day));
		}
		StatusText->SetText(When);
		StatusText->SetColorAndOpacity(FSlateColor(bSignal ? DeadlineUI::Warn : DeadlineUI::Accent));
	}
	if (ConfidenceText)
	{
		// Active events are not a forecast any more: say so rather than "%100".
		ConfidenceText->SetText(bSignal
			? FText::Format(NSLOCTEXT("Deadline", "ForecastConfidence", "%{0}"), FText::AsNumber(FMath::RoundToInt(Entry.Confidence * 100.f)))
			: NSLOCTEXT("Deadline", "ForecastHappening", "OLUYOR"));
		ConfidenceText->SetColorAndOpacity(FSlateColor(bSignal ? ForecastFormat::ConfidenceColour(Entry.Confidence) : DeadlineUI::Accent));
	}
	if (ConfidenceBar)
	{
		ConfidenceBar->SetPercent(bSignal ? Entry.Confidence : 1.f);
		ConfidenceBar->SetFillColorAndOpacity(bSignal ? ForecastFormat::ConfidenceColour(Entry.Confidence) : DeadlineUI::Accent);
	}
	if (BodyText)
	{
		BodyText->SetText(FText::FromString(Entry.Text));
	}
	if (ImpactText)
	{
		TArray<FString> Parts;
		if (Entry.ImpactMax > 0.f)
		{
			Parts.Add(FString::Printf(TEXT("+%%%.0f–%.0f"), Entry.ImpactMin * 100.f, Entry.ImpactMax * 100.f));
		}
		if (Entry.Products.Num() > 0)
		{
			Parts.Add(FText::Format(NSLOCTEXT("Deadline", "ForecastProducts", "{0} ürün"), FText::AsNumber(Entry.Products.Num())).ToString());
		}
		if (Entry.RouteCostMultiplier > 1.f + KINDA_SMALL_NUMBER)
		{
			Parts.Add(FText::Format(NSLOCTEXT("Deadline", "ForecastRoutes", "rotalar +%{0}"),
				FText::AsNumber(FMath::RoundToInt((Entry.RouteCostMultiplier - 1.f) * 100.f))).ToString());
		}
		const FText Prefix = bSignal ? NSLOCTEXT("Deadline", "ForecastIf", "Olursa") : NSLOCTEXT("Deadline", "ForecastEffect", "Etkisi");
		ImpactText->SetText(FText::FromString(Prefix.ToString() + TEXT(": ") + FString::Join(Parts, TEXT(" · "))));
	}
	if (ExposureText)
	{
		if (Entry.Exposure > 0.5f)
		{
			ExposureText->SetText(FText::Format(NSLOCTEXT("Deadline", "ForecastExposure", "Maruziyet {0}"), ForecastFormat::Money(Entry.Exposure)));
			ExposureText->SetColorAndOpacity(FSlateColor(DeadlineUI::Body));
		}
		else
		{
			ExposureText->SetText(NSLOCTEXT("Deadline", "ForecastNoStake", "Elinde bu ürünlerden yok"));
			ExposureText->SetColorAndOpacity(FSlateColor(DeadlineUI::Muted));
		}
	}
	SetSelected(bSelected);
}

void UForecastCardWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	if (CardFrame)
	{
		CardFrame->SetBrushColor(bSelected ? DeadlineUI::PanelRaised : DeadlineUI::Panel);
	}
}
