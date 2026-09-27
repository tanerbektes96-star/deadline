// Copyright DEADLINE. All Rights Reserved.

#include "UI/NotebookRowWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/CommitmentResultWidget.h"
#include "UI/DeadlineUIPalette.h"
#include "UI/ForecastFormat.h"

void UNotebookRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (RowButton)
	{
		RowButton->OnClicked.AddUniqueDynamic(this, &UNotebookRowWidget::HandleClicked);
	}
}

void UNotebookRowWidget::HandleClicked()
{
	OnRowClicked.Broadcast(CommitmentID);
}

void UNotebookRowWidget::SetCommitment(const FForecastCommitment& C, const FString& EventName, const FString& ProductName)
{
	CommitmentID = C.ID;
	const bool bJudged = C.State == ECommitmentState::Resolved;
	const FLinearColor Tone = bJudged ? UCommitmentResultWidget::OutcomeTone(C) : DeadlineUI::Accent;

	if (DayText)
	{
		// The day the forecast was about: the one thing every row shares.
		DayText->SetText(FText::Format(NSLOCTEXT("Deadline", "NotebookRowDay", "GÜN {0}"), FText::AsNumber(C.ExpectedDay)));
	}
	if (NameText)
	{
		NameText->SetText(FText::Format(NSLOCTEXT("Deadline", "NotebookRowName", "{0}  ·  {1}"),
			FText::FromString(EventName), FText::FromString(ProductName)));
	}
	if (StatusText)
	{
		FText Status;
		FLinearColor Colour = DeadlineUI::Muted;
		if (!bJudged)
		{
			Status = FText::Format(NSLOCTEXT("Deadline", "NotebookRowLive", "SÜRÜYOR  ·  {0}. gün değerlendirilir"),
				FText::AsNumber(C.GetResolveDay()));
			Colour = DeadlineUI::Accent;
		}
		else
		{
			Status = FText::Format(C.bEventHappened
					? NSLOCTEXT("Deadline", "NotebookRowRight", "TUTTU  ·  kaynak %{0}")
					: NSLOCTEXT("Deadline", "NotebookRowWrong", "TUTMADI  ·  kaynak %{0}"),
				ForecastFormat::Percent(C.Confidence));
			Colour = C.bEventHappened ? DeadlineUI::Profit : DeadlineUI::Loss;
		}
		StatusText->SetText(Status);
		StatusText->SetColorAndOpacity(FSlateColor(Colour));
	}
	if (ResultText)
	{
		FText Result;
		if (!bJudged)
		{
			Result = FText::FromString(TEXT("—"));
		}
		else if (C.BoughtContainers == 0)
		{
			Result = NSLOCTEXT("Deadline", "NotebookRowNoTrade", "işlem yok");
		}
		else
		{
			Result = ForecastFormat::SignedMoney(C.Result);
		}
		ResultText->SetText(Result);
		ResultText->SetColorAndOpacity(FSlateColor(bJudged && C.BoughtContainers > 0 ? Tone : DeadlineUI::Muted));
	}
	if (ToneStrip)
	{
		ToneStrip->SetBrushColor(Tone);
	}
	SetSelected(bSelected);
}

void UNotebookRowWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	if (RowFrame)
	{
		RowFrame->SetBrushColor(bSelected ? DeadlineUI::PanelRaised : DeadlineUI::Panel);
	}
}
