// Copyright DEADLINE. All Rights Reserved.

#include "UI/NotebookScreenWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/DeadlinePlayerController.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "Events/EventSubsystem.h"
#include "UI/CommitmentResultWidget.h"
#include "UI/DeadlineUIPalette.h"
#include "UI/ForecastFormat.h"
#include "UI/NotebookRowWidget.h"

void UNotebookScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UNotebookScreenWidget::HandleCloseClicked);
	}
	if (BoardTabButton)
	{
		BoardTabButton->OnClicked.AddUniqueDynamic(this, &UNotebookScreenWidget::HandleBoardTabClicked);
	}
	if (UForecastSubsystem* Forecast = GetForecast())
	{
		Forecast->OnBoardChanged.AddUniqueDynamic(this, &UNotebookScreenWidget::HandleBoardChanged);
		Forecast->OnCommitmentResolved.AddUniqueDynamic(this, &UNotebookScreenWidget::HandleCommitmentResolved);
	}
	Refresh();
}

void UNotebookScreenWidget::NativeDestruct()
{
	if (UForecastSubsystem* Forecast = GetForecast())
	{
		Forecast->OnBoardChanged.RemoveDynamic(this, &UNotebookScreenWidget::HandleBoardChanged);
		Forecast->OnCommitmentResolved.RemoveDynamic(this, &UNotebookScreenWidget::HandleCommitmentResolved);
	}
	Super::NativeDestruct();
}

// --- Refresh ----------------------------------------------------------------------

void UNotebookScreenWidget::Refresh()
{
	const UForecastSubsystem* Forecast = GetForecast();

	Entries.Reset();
	if (Forecast)
	{
		for (const FForecastCommitment& C : Forecast->GetCommitments())
		{
			if (C.State != ECommitmentState::Cancelled)
			{
				Entries.Add(C);
			}
		}
	}
	// Newest first: IDs only grow.
	Entries.Sort([](const FForecastCommitment& A, const FForecastCommitment& B) { return A.ID > B.ID; });

	if (HeaderDayText)
	{
		const UGameInstance* GI = GetGameInstance();
		const UTimeSubsystem* Time = GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
		HeaderDayText->SetText(FText::Format(NSLOCTEXT("Deadline", "NotebookHeaderDay", "GÜN {0}"),
			FText::AsNumber(Time ? Time->GetDay() : 0)));
	}
	ShowSummary(Forecast ? Forecast->GetNotebookSummary() : FNotebookSummary());

	if (RowList)
	{
		RowList->ClearChildren();
	}
	Rows.Reset();
	if (RowList && RowWidgetClass)
	{
		for (const FForecastCommitment& C : Entries)
		{
			UNotebookRowWidget* Row = CreateWidget<UNotebookRowWidget>(this, RowWidgetClass);
			if (!Row)
			{
				continue;
			}
			RowList->AddChild(Row);
			if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Row->Slot))
			{
				RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			}
			Row->SetCommitment(C, EventName(C.EventID), ProductName(C.ProductID));
			Row->OnRowClicked.AddUniqueDynamic(this, &UNotebookScreenWidget::HandleRowClicked);
			Rows.Add(Row);
		}
	}
	if (EmptyText)
	{
		EmptyText->SetVisibility(Entries.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Keep the selection across a refresh; otherwise open on the newest.
	const bool bStillThere = Entries.ContainsByPredicate([this](const FForecastCommitment& C) { return C.ID == SelectedID; });
	SelectCommitment(bStillThere ? SelectedID : (Entries.Num() > 0 ? Entries[0].ID : INDEX_NONE));
}

void UNotebookScreenWidget::SelectCommitment(int32 CommitmentID)
{
	SelectedID = CommitmentID;
	for (UNotebookRowWidget* Row : Rows)
	{
		if (Row)
		{
			Row->SetSelected(Row->GetCommitmentID() == CommitmentID);
		}
	}
	ShowDetail(Entries.FindByPredicate([CommitmentID](const FForecastCommitment& C) { return C.ID == CommitmentID; }));
}

// --- Summary ----------------------------------------------------------------------

void UNotebookScreenWidget::ShowSummary(const FNotebookSummary& S)
{
	if (SummaryText)
	{
		FText Summary;
		if (S.Judged == 0)
		{
			Summary = NSLOCTEXT("Deadline", "NotebookSummaryNone", "Henüz sonuçlanan tahmin yok");
		}
		else
		{
			Summary = FText::Format(NSLOCTEXT("Deadline", "NotebookSummary",
				"{0} tahmin sonuçlandı  ·  {1} tanesi tuttu  ·  kaynaklar ortalama %{2} demişti"),
				FText::AsNumber(S.Judged), FText::AsNumber(S.Happened), ForecastFormat::Percent(S.AverageConfidence));
			if (S.Traded > 0)
			{
				Summary = FText::Format(NSLOCTEXT("Deadline", "NotebookSummaryTrades", "{0}  ·  {1} / {2} işlem kârlı"),
					Summary, FText::AsNumber(S.Profitable), FText::AsNumber(S.Traded));
			}
		}
		SummaryText->SetText(Summary);
	}
	if (TotalText)
	{
		const bool bAny = S.Traded > 0;
		TotalText->SetText(bAny
			? FText::Format(NSLOCTEXT("Deadline", "NotebookTotal", "TOPLAM  {0}"), ForecastFormat::SignedMoney(S.TotalResult))
			: FText::GetEmpty());
		TotalText->SetColorAndOpacity(FSlateColor(S.TotalResult > 0.5f ? DeadlineUI::Profit
			: (S.TotalResult < -0.5f ? DeadlineUI::Loss : DeadlineUI::Muted)));
	}

	// One line of advice at most. A repeated costly lesson beats the
	// confidence check: it names a thing to do differently.
	FText Pattern = HabitLine(S.RepeatLesson, S.RepeatCount);
	if (Pattern.IsEmpty() && S.Judged >= 3)
	{
		const float HitRate = static_cast<float>(S.Happened) / S.Judged;
		if (S.AverageConfidence - HitRate > 0.15f)
		{
			Pattern = FText::Format(NSLOCTEXT("Deadline", "NotebookOverTrust",
				"Kaynaklar ortalama %{0} dedi ama tutma oranın %{1}. Söylentilere biraz daha az para bağla."),
				ForecastFormat::Percent(S.AverageConfidence), ForecastFormat::Percent(HitRate));
		}
	}
	if (PatternText)
	{
		PatternText->SetText(Pattern);
	}
	if (PatternFrame)
	{
		PatternFrame->SetVisibility(Pattern.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

FText UNotebookScreenWidget::HabitLine(ECommitmentLesson Lesson, int32 Count)
{
	if (Count < 2)
	{
		return FText::GetEmpty();
	}
	const FText N = FText::AsNumber(Count);
	switch (Lesson)
	{
	case ECommitmentLesson::EatenBySpread:
		return FText::Format(NSLOCTEXT("Deadline", "HabitSpread",
			"{0} kez olay geldi ama fiyat artışı alım-satım farkını karşılamadı. Mal bağlamadan önce olayın etkisini başabaş eşiğiyle karşılaştır."), N);
	case ECommitmentLesson::HeldTooLong:
		return FText::Format(NSLOCTEXT("Deadline", "HabitHeld",
			"{0} kez zirveyi kaçırıp geç sattın. Olay sürerken sat; bitince fiyat geri çekilir."), N);
	case ECommitmentLesson::FalseRumour:
		return FText::Format(NSLOCTEXT("Deadline", "HabitFalse",
			"{0} kez boşa çıkan söylentiye para bağladın. Güveni düşük söylentilerde miktarı azalt."), N);
	case ECommitmentLesson::MissedIt:
		return FText::Format(NSLOCTEXT("Deadline", "HabitMissed",
			"{0} kez tahmin tuttu ama mal almadın. Taahhüt bütçeyi kilitler; kârı getiren, alımı yapmaktır."), N);
	default:
		return FText::GetEmpty();
	}
}

// --- Detail -------------------------------------------------------------------------

void UNotebookScreenWidget::ShowDetail(const FForecastCommitment* C)
{
	if (!C)
	{
		if (DetailVerdictText) { DetailVerdictText->SetText(FText::GetEmpty()); }
		if (DetailTitleText) { DetailTitleText->SetText(NSLOCTEXT("Deadline", "NotebookEmptyTitle", "Not defteri boş")); }
		if (DetailOddsText) { DetailOddsText->SetText(FText::GetEmpty()); }
		if (DetailOutcomeText) { DetailOutcomeText->SetText(FText::GetEmpty()); }
		if (DetailLedgerText)
		{
			DetailLedgerText->SetText(NSLOCTEXT("Deadline", "NotebookEmptyBody",
				"Tahmin panosunda bir söylentiye taahhüt ettiğinde burada görünür. Sonuçlandığında ne olduğu ve neden öyle olduğu da buraya yazılır."));
		}
		if (DetailLessonFrame) { DetailLessonFrame->SetVisibility(ESlateVisibility::Collapsed); }
		return;
	}

	const FString Product = ProductName(C->ProductID);
	if (DetailTitleText)
	{
		DetailTitleText->SetText(FText::Format(NSLOCTEXT("Deadline", "ResultTitle", "{0}  ·  {1}"),
			FText::FromString(EventName(C->EventID)), FText::FromString(Product)));
	}

	if (C->State != ECommitmentState::Resolved)
	{
		// Live: what was promised and how far along it is. No verdict yet.
		if (DetailVerdictText)
		{
			DetailVerdictText->SetText(NSLOCTEXT("Deadline", "NotebookLive", "SÜRÜYOR"));
			DetailVerdictText->SetColorAndOpacity(FSlateColor(DeadlineUI::Accent));
		}
		if (DetailOddsText)
		{
			DetailOddsText->SetText(FText::Format(NSLOCTEXT("Deadline", "NotebookLiveOdds",
				"Kaynak %{0} demişti  ·  olay {1}. gün bekleniyor  ·  değerlendirme {2}. gün"),
				ForecastFormat::Percent(C->Confidence), FText::AsNumber(C->ExpectedDay), FText::AsNumber(C->GetResolveDay())));
		}
		if (DetailOutcomeText)
		{
			DetailOutcomeText->SetText(FText::FromString(TEXT("—")));
			DetailOutcomeText->SetColorAndOpacity(FSlateColor(DeadlineUI::Muted));
		}
		if (DetailLedgerText)
		{
			FText Ledger = FText::Format(NSLOCTEXT("Deadline", "NotebookLiveBought",
				"Alınan:  {0} / {1} konteyner  ·  {2} ödendi"),
				FText::AsNumber(C->BoughtContainers), FText::AsNumber(C->TargetContainers), ForecastFormat::Money(C->Spent));
			if (C->State == ECommitmentState::Open && C->LockRemaining > 0.5f)
			{
				Ledger = FText::Format(NSLOCTEXT("Deadline", "NotebookLiveLock",
					"{0}\nKilitli kalan:  {1}  ·  {2}. gün serbest kalır"),
					Ledger, ForecastFormat::Money(C->LockRemaining), FText::AsNumber(C->ExpectedDay));
			}
			DetailLedgerText->SetText(Ledger);
		}
		if (DetailLessonFrame) { DetailLessonFrame->SetVisibility(ESlateVisibility::Collapsed); }
		return;
	}

	if (DetailVerdictText)
	{
		DetailVerdictText->SetText(UCommitmentResultWidget::Verdict(*C));
		DetailVerdictText->SetColorAndOpacity(FSlateColor(C->bEventHappened ? DeadlineUI::Profit : DeadlineUI::Loss));
	}
	if (DetailOddsText)
	{
		DetailOddsText->SetText(UCommitmentResultWidget::Odds(*C));
	}
	if (DetailOutcomeText)
	{
		DetailOutcomeText->SetText(UCommitmentResultWidget::Outcome(*C));
		DetailOutcomeText->SetColorAndOpacity(FSlateColor(UCommitmentResultWidget::OutcomeTone(*C)));
	}
	if (DetailLedgerText)
	{
		DetailLedgerText->SetText(UCommitmentResultWidget::Ledger(*C));
	}
	if (DetailLessonFrame) { DetailLessonFrame->SetVisibility(ESlateVisibility::HitTestInvisible); }
	if (DetailLessonTitleText) { DetailLessonTitleText->SetText(UCommitmentResultWidget::LessonTitle(*C)); }
	if (DetailLessonText) { DetailLessonText->SetText(UCommitmentResultWidget::LessonBody(*C, Product)); }
}

// --- Names ----------------------------------------------------------------------------

FString UNotebookScreenWidget::EventName(FName EventID) const
{
	const UGameInstance* GI = GetGameInstance();
	const UEventSubsystem* Events = GI ? GI->GetSubsystem<UEventSubsystem>() : nullptr;
	return Events ? Events->GetEventName(EventID) : EventID.ToString();
}

FString UNotebookScreenWidget::ProductName(FName ProductID) const
{
	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	return Catalogue ? Catalogue->GetDisplayName(ProductID) : ProductID.ToString();
}

// --- Events -----------------------------------------------------------------------------

void UNotebookScreenWidget::HandleRowClicked(int32 CommitmentID)
{
	SelectCommitment(CommitmentID);
}

void UNotebookScreenWidget::HandleBoardChanged()
{
	Refresh();
}

void UNotebookScreenWidget::HandleCommitmentResolved(const FForecastCommitment& Commitment)
{
	Refresh();
}

void UNotebookScreenWidget::HandleCloseClicked()
{
	if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOwningPlayer()))
	{
		PC->CloseNotebookScreen();
	}
}

void UNotebookScreenWidget::HandleBoardTabClicked()
{
	if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOwningPlayer()))
	{
		PC->OpenForecastScreen();
	}
}

UForecastSubsystem* UNotebookScreenWidget::GetForecast() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UForecastSubsystem>() : nullptr;
}
