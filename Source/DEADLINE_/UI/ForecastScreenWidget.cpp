// Copyright DEADLINE. All Rights Reserved.

#include "UI/ForecastScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Core/DeadlinePlayerController.h"
#include "Core/DeadlineSettings.h"
#include "Core/TimeSubsystem.h"
#include "Economy/EconomySubsystem.h"
#include "Engine/GameInstance.h"
#include "UI/DeadlineUIPalette.h"
#include "UI/ForecastCardWidget.h"
#include "UI/ForecastFormat.h"

void UForecastScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UForecastScreenWidget::HandleCloseClicked);
	}
	if (ProductPrevButton) { ProductPrevButton->OnClicked.AddUniqueDynamic(this, &UForecastScreenWidget::HandleProductPrev); }
	if (ProductNextButton) { ProductNextButton->OnClicked.AddUniqueDynamic(this, &UForecastScreenWidget::HandleProductNext); }
	if (QtyMinusButton) { QtyMinusButton->OnClicked.AddUniqueDynamic(this, &UForecastScreenWidget::HandleQtyMinus); }
	if (QtyPlusButton) { QtyPlusButton->OnClicked.AddUniqueDynamic(this, &UForecastScreenWidget::HandleQtyPlus); }
	if (HoldMinusButton) { HoldMinusButton->OnClicked.AddUniqueDynamic(this, &UForecastScreenWidget::HandleHoldMinus); }
	if (HoldPlusButton) { HoldPlusButton->OnClicked.AddUniqueDynamic(this, &UForecastScreenWidget::HandleHoldPlus); }
	if (CommitButton) { CommitButton->OnClicked.AddUniqueDynamic(this, &UForecastScreenWidget::HandleCommitClicked); }
	if (UForecastSubsystem* Forecast = GetForecast())
	{
		Forecast->OnBoardChanged.AddUniqueDynamic(this, &UForecastScreenWidget::HandleBoardChanged);
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UNewsSubsystem* News = GI->GetSubsystem<UNewsSubsystem>())
		{
			News->OnBulletinPublished.AddUniqueDynamic(this, &UForecastScreenWidget::HandleBulletin);
		}
	}
	Refresh();
}

void UForecastScreenWidget::NativeDestruct()
{
	if (UForecastSubsystem* Forecast = GetForecast())
	{
		Forecast->OnBoardChanged.RemoveDynamic(this, &UForecastScreenWidget::HandleBoardChanged);
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UNewsSubsystem* News = GI->GetSubsystem<UNewsSubsystem>())
		{
			News->OnBulletinPublished.RemoveDynamic(this, &UForecastScreenWidget::HandleBulletin);
		}
	}
	Super::NativeDestruct();
}

// --- Refresh --------------------------------------------------------------------

void UForecastScreenWidget::Refresh()
{
	const UForecastSubsystem* Forecast = GetForecast();
	Board = Forecast ? Forecast->GetBoard() : TArray<FForecastEntry>();

	if (HeaderDayText)
	{
		const UGameInstance* GI = GetGameInstance();
		const UTimeSubsystem* Time = GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
		HeaderDayText->SetText(FText::Format(NSLOCTEXT("Deadline", "ForecastHeaderDay", "GÜN {0}"),
			FText::AsNumber(Time ? Time->GetDay() : 0)));
	}
	if (SummaryText)
	{
		const UGameInstance* GI = GetGameInstance();
		const UEconomySubsystem* Economy = GI ? GI->GetSubsystem<UEconomySubsystem>() : nullptr;
		const float Locked = Economy ? Economy->GetLockedFunds() : 0.f;
		const FText Exposure = ForecastFormat::Money(Forecast ? Forecast->GetTotalExposure() : 0.f);
		const FText Available = ForecastFormat::Money(Economy ? Economy->GetAvailableFunds() : 0.f);
		SummaryText->SetText(Locked > 0.5f
			? FText::Format(NSLOCTEXT("Deadline", "ForecastSummaryLocked", "Toplam maruziyet {0}   ·   Serbest para {1}   ·   Kilitli {2}"),
				Exposure, Available, ForecastFormat::Money(Locked))
			: FText::Format(NSLOCTEXT("Deadline", "ForecastSummary", "Toplam maruziyet {0}   ·   Kullanılabilir para {1}"),
				Exposure, Available));
	}

	if (CardList)
	{
		CardList->ClearChildren();
	}
	Cards.Reset();
	if (CardList && CardWidgetClass)
	{
		for (const FForecastEntry& Entry : Board)
		{
			UForecastCardWidget* Card = CreateWidget<UForecastCardWidget>(this, CardWidgetClass);
			if (!Card)
			{
				continue;
			}
			CardList->AddChild(Card);
			if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Card->Slot))
			{
				CardSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			}
			Card->SetEntry(Entry);
			Card->OnCardClicked.AddUniqueDynamic(this, &UForecastScreenWidget::HandleCardClicked);
			Cards.Add(Card);
		}
	}
	if (EmptyText)
	{
		EmptyText->SetVisibility(Board.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// Keep the selection across a refresh. Otherwise open on what matters most
	// to you -- the entry with the most money riding on it -- or the first.
	const bool bStillThere = Board.ContainsByPredicate([this](const FForecastEntry& E) { return E.EventID == SelectedEventID; });
	FName Pick = bStillThere ? SelectedEventID : NAME_None;
	if (Pick.IsNone() && Board.Num() > 0)
	{
		const FForecastEntry* Best = &Board[0];
		for (const FForecastEntry& E : Board)
		{
			if (E.Exposure > Best->Exposure)
			{
				Best = &E;
			}
		}
		Pick = Best->EventID;
	}
	SelectEntry(Pick);
}

void UForecastScreenWidget::SelectEntry(FName EventID)
{
	SelectedEventID = EventID;
	for (UForecastCardWidget* Card : Cards)
	{
		if (Card)
		{
			Card->SetSelected(Card->GetEventID() == EventID);
		}
	}
	ShowDetail(Board.FindByPredicate([EventID](const FForecastEntry& E) { return E.EventID == EventID; }));
}

// --- Detail ----------------------------------------------------------------------

void UForecastScreenWidget::ShowDetail(const FForecastEntry* Entry)
{
	if (DetailProductList)
	{
		DetailProductList->ClearChildren();
	}
	const ESlateVisibility ProductsVisibility = (Entry && Entry->Products.Num() > 0)
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (DetailProductsLabel)
	{
		DetailProductsLabel->SetVisibility(ProductsVisibility);
	}
	if (DetailHintFrame)
	{
		DetailHintFrame->SetVisibility(Entry ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!Entry)
	{
		const FText Nothing = NSLOCTEXT("Deadline", "ForecastNothing",
			"Şu an kulağına gelen bir şey yok. Sabah 08:00 bülteni yeni söylentiler getirir.");
		if (DetailTitleText) { DetailTitleText->SetText(NSLOCTEXT("Deadline", "ForecastQuiet", "Piyasa sakin")); }
		if (DetailStatusText) { DetailStatusText->SetText(FText::GetEmpty()); }
		if (DetailOddsText) { DetailOddsText->SetText(FText::GetEmpty()); }
		if (DetailBodyText) { DetailBodyText->SetText(Nothing); }
		if (DetailStakeText) { DetailStakeText->SetText(FText::GetEmpty()); }
		if (DetailHintText) { DetailHintText->SetText(FText::GetEmpty()); }
		ShowCommitPanel(nullptr);
		return;
	}

	const bool bSignal = Entry->Status == EForecastStatus::Signal;
	if (DetailTitleText)
	{
		DetailTitleText->SetText(FText::FromString(Entry->Name));
	}
	if (DetailStatusText)
	{
		DetailStatusText->SetText(bSignal ? NSLOCTEXT("Deadline", "ForecastIsRumour", "SÖYLENTİ — henüz olmadı")
			: NSLOCTEXT("Deadline", "ForecastIsActive", "OLUYOR — fiyatlara işliyor"));
		DetailStatusText->SetColorAndOpacity(FSlateColor(bSignal ? DeadlineUI::Warn : DeadlineUI::Accent));
	}
	if (DetailOddsText)
	{
		if (bSignal)
		{
			const FText When = Entry->DaysAway <= 1
				? NSLOCTEXT("Deadline", "ForecastOddsTomorrow", "yarın")
				: FText::Format(NSLOCTEXT("Deadline", "ForecastOddsDays", "{0} gün sonra (gün {1})"),
					FText::AsNumber(Entry->DaysAway), FText::AsNumber(Entry->Day));
			DetailOddsText->SetText(FText::Format(NSLOCTEXT("Deadline", "ForecastOdds", "%{0} olasılık · {1}"),
				FText::AsNumber(FMath::RoundToInt(Entry->Confidence * 100.f)), When));
			DetailOddsText->SetColorAndOpacity(FSlateColor(ForecastFormat::ConfidenceColour(Entry->Confidence)));
		}
		else
		{
			DetailOddsText->SetText(Entry->DaysAway <= 0
				? NSLOCTEXT("Deadline", "ForecastEndsToday", "Bugün bitiyor")
				: FText::Format(NSLOCTEXT("Deadline", "ForecastEndsIn", "{0} gün daha sürer (gün {1}'e kadar)"),
					FText::AsNumber(Entry->DaysAway), FText::AsNumber(Entry->Day)));
			DetailOddsText->SetColorAndOpacity(FSlateColor(DeadlineUI::Accent));
		}
	}
	if (DetailBodyText)
	{
		DetailBodyText->SetText(FText::FromString(Entry->Text));
	}
	if (DetailStakeText)
	{
		FText Stake;
		if (Entry->Exposure > 0.5f && Entry->ImpactMax > 0.f)
		{
			Stake = FText::Format(NSLOCTEXT("Deadline", "ForecastStake",
				"Elindeki mal {0}. {1}: +{2} ile +{3} arası."),
				ForecastFormat::Money(Entry->Exposure),
				bSignal ? NSLOCTEXT("Deadline", "ForecastIfHappens", "Olursa değeri artar") : NSLOCTEXT("Deadline", "ForecastWhileRuns", "Sürdükçe olası artış"),
				ForecastFormat::Money(Entry->GainIfHappensMin), ForecastFormat::Money(Entry->GainIfHappensMax));
		}
		else if (Entry->ImpactMax > 0.f)
		{
			Stake = FText::Format(bSignal
				? NSLOCTEXT("Deadline", "ForecastNoStakeDetail", "Bu ürünlerden elinde yok. Olursa fiyatları +%{0}–{1} yükselir.")
				: NSLOCTEXT("Deadline", "ForecastNoStakeActive", "Bu ürünlerden elinde yok. Olay sürdükçe fiyatları +%{0}–{1} yükselir."),
				FText::AsNumber(FMath::RoundToInt(Entry->ImpactMin * 100.f)), FText::AsNumber(FMath::RoundToInt(Entry->ImpactMax * 100.f)));
		}
		else
		{
			Stake = FText::Format(NSLOCTEXT("Deadline", "ForecastRouteDetail", "Mal fiyatlarına dokunmaz; seferler %{0} pahalanır."),
				FText::AsNumber(FMath::RoundToInt((Entry->RouteCostMultiplier - 1.f) * 100.f)));
		}
		DetailStakeText->SetText(Stake);
	}
	if (DetailHintText)
	{
		FText Hint;
		if (Entry->ImpactMax <= 0.f)
		{
			Hint = bSignal
				? NSLOCTEXT("Deadline", "ForecastHintRouteSignal", "Karar: yapacağın seferleri bu günden önceye çekmek mi, beklemek mi?")
				: NSLOCTEXT("Deadline", "ForecastHintRouteActive", "Karar: seferi şimdi mi yaparsın, bitmesini mi beklersin?");
		}
		else
		{
			Hint = bSignal
				? NSLOCTEXT("Deadline", "ForecastHintSignal",
					"Karar: şimdi alıp fiyat yükselince satmak. Söylenti yanlış çıkarsa fiyatlar normal seyrinde kalır; aldığın mal elinde kalır.")
				: NSLOCTEXT("Deadline", "ForecastHintActive",
					"Karar: ne zaman satacağın. Olay bitince fiyatlar normale döner; en iyi satış, bitmeden önce.");
		}
		DetailHintText->SetText(Hint);
	}

	for (const FForecastProductLine& Line : Entry->Products)
	{
		AddProductRow(Line, !bSignal);
	}
	ShowCommitPanel(Entry);
}

void UForecastScreenWidget::AddProductRow(const FForecastProductLine& Line, bool bActive)
{
	if (!DetailProductList || !WidgetTree)
	{
		return;
	}
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	DetailProductList->AddChild(Row);
	if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Row->Slot))
	{
		RowSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
	}

	// Rows take the font of the body text above them, so the build script
	// stays the one place that knows the typography.
	auto AddCell = [this, Row](const FString& Text, const FLinearColor& Colour, bool bFill)
	{
		UTextBlock* Cell = WidgetTree->ConstructWidget<UTextBlock>();
		Cell->SetText(FText::FromString(Text));
		if (DetailBodyText)
		{
			Cell->SetFont(DetailBodyText->GetFont());
		}
		Cell->SetColorAndOpacity(FSlateColor(Colour));
		Row->AddChild(Cell);
		if (UHorizontalBoxSlot* CellSlot = Cast<UHorizontalBoxSlot>(Cell->Slot))
		{
			CellSlot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
			CellSlot->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
			CellSlot->SetVerticalAlignment(VAlign_Center);
		}
	};

	AddCell(Line.Name, DeadlineUI::Body, true);
	AddCell(FString::Printf(TEXT("$%.2f"), Line.Price), DeadlineUI::Body, false);
	if (bActive)
	{
		AddCell(FString::Printf(TEXT("%+.0f%%"), Line.ChangePercent),
			Line.ChangePercent >= 0.f ? DeadlineUI::Profit : DeadlineUI::Loss, false);
	}
	AddCell(Line.HeldContainers > 0.f
			? FText::Format(NSLOCTEXT("Deadline", "ForecastHeldRow", "elinde {0} · {1}"),
				FText::AsNumber(FMath::RoundToInt(Line.HeldContainers)), ForecastFormat::Money(Line.HeldValue)).ToString()
			: NSLOCTEXT("Deadline", "ForecastHeldNone", "elinde yok").ToString(),
		Line.HeldContainers > 0.f ? DeadlineUI::Body : DeadlineUI::Muted, false);
}

// --- Commitment panel -------------------------------------------------------------

void UForecastScreenWidget::ShowCommitPanel(const FForecastEntry* Entry)
{
	// Only a signal that pushes a price can be committed to: a running event
	// is no longer a forecast, and a route event has no product to buy.
	const bool bCommittable = Entry && Entry->Status == EForecastStatus::Signal
		&& Entry->Products.Num() > 0 && Entry->ImpactMax > 0.f;
	if (CommitFrame)
	{
		CommitFrame->SetVisibility(bCommittable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!bCommittable)
	{
		return;
	}
	if (CommitForEventID != Entry->EventID)
	{
		CommitForEventID = Entry->EventID;
		CommitProductIndex = 0;
		CommitQty = 1;
		CommitHold = FMath::Clamp(UDeadlineSettings::Get().CommitHoldDaysDefault, 1,
			FMath::Max(1, UDeadlineSettings::Get().CommitHoldDaysMax));
	}
	RefreshCommitPanel();
}

void UForecastScreenWidget::RefreshCommitPanel()
{
	const FForecastEntry* Entry = Board.FindByPredicate([this](const FForecastEntry& E) { return E.EventID == CommitForEventID; });
	UForecastSubsystem* Forecast = GetForecast();
	const UGameInstance* GI = GetGameInstance();
	const UEconomySubsystem* Economy = GI ? GI->GetSubsystem<UEconomySubsystem>() : nullptr;
	if (!Entry || !Forecast || !Economy || Entry->Products.Num() == 0)
	{
		return;
	}

	// --- Already committed: progress and the way out --------------------------
	FForecastCommitment Existing;
	if (Entry->CommitmentID != INDEX_NONE && Forecast->GetCommitment(Entry->CommitmentID, Existing))
	{
		if (CommitControls) { CommitControls->SetVisibility(ESlateVisibility::Collapsed); }
		const FForecastProductLine* Line = Entry->Products.FindByPredicate([&Existing](const FForecastProductLine& L)
		{
			return L.ProductID == Existing.ProductID;
		});
		const FText Product = FText::FromString(Line ? Line->Name : Existing.ProductID.ToString());
		if (CommitStatusText)
		{
			CommitStatusText->SetText(FText::Format(NSLOCTEXT("Deadline", "CommitProgress",
				"{0}: {1} / {2} konteyner alındı"),
				Product, FText::AsNumber(Existing.BoughtContainers), FText::AsNumber(Existing.TargetContainers)));
			CommitStatusText->SetColorAndOpacity(FSlateColor(Existing.BoughtContainers >= Existing.TargetContainers
				? DeadlineUI::Profit : DeadlineUI::Body));
		}
		if (CommitBudgetText)
		{
			CommitBudgetText->SetText(FText::Format(NSLOCTEXT("Deadline", "CommitLockedInfo",
				"Kilitli kalan {0} — yalnızca {1} alımına harcanır, gün {2}'de serbest kalır. Değerlendirme: gün {3}."),
				ForecastFormat::Money(Existing.LockRemaining), Product,
				FText::AsNumber(Existing.ExpectedDay), FText::AsNumber(Existing.GetResolveDay())));
			CommitBudgetText->SetColorAndOpacity(FSlateColor(DeadlineUI::Muted));
		}
		if (CommitButtonText) { CommitButtonText->SetText(NSLOCTEXT("Deadline", "CommitCancel", "VAZGEÇ")); }
		if (CommitButton) { CommitButton->SetIsEnabled(true); }
		return;
	}

	// --- Choosing ---------------------------------------------------------------
	if (CommitControls) { CommitControls->SetVisibility(ESlateVisibility::Visible); }
	CommitProductIndex = FMath::Clamp(CommitProductIndex, 0, Entry->Products.Num() - 1);
	const FForecastProductLine& Line = Entry->Products[CommitProductIndex];
	const int32 MaxQty = FMath::Max(1, Forecast->GetMaxAffordable(Line.ProductID));
	CommitQty = FMath::Clamp(CommitQty, 1, MaxQty);
	const int32 MaxHold = FMath::Max(1, UDeadlineSettings::Get().CommitHoldDaysMax);
	CommitHold = FMath::Clamp(CommitHold, 1, MaxHold);

	const float UnitPrice = Economy->GetBuyPrice(Line.ProductID);
	const float Budget = UnitPrice * CommitQty;
	const ECommitRefusal Refusal = Forecast->CanCommit(Entry->EventID, Line.ProductID, CommitQty);

	if (CommitStatusText)
	{
		CommitStatusText->SetText(NSLOCTEXT("Deadline", "CommitPrompt",
			"Bu olursa fiyatı yükselir diyorsan: ürünü, miktarı ve bekleme süresini seç."));
		CommitStatusText->SetColorAndOpacity(FSlateColor(DeadlineUI::Muted));
	}
	if (CommitProductText)
	{
		CommitProductText->SetText(FText::Format(NSLOCTEXT("Deadline", "CommitProduct", "{0}  ·  {1}/konteyner  ({2}/{3})"),
			FText::FromString(Line.Name), ForecastFormat::Money(UnitPrice),
			FText::AsNumber(CommitProductIndex + 1), FText::AsNumber(Entry->Products.Num())));
	}
	if (CommitQtyText)
	{
		CommitQtyText->SetText(FText::AsNumber(CommitQty));
	}
	if (CommitHoldText)
	{
		CommitHoldText->SetText(FText::Format(NSLOCTEXT("Deadline", "CommitHoldDays", "{0} gün"), FText::AsNumber(CommitHold)));
	}
	if (QtyMinusButton) { QtyMinusButton->SetIsEnabled(CommitQty > 1); }
	if (QtyPlusButton) { QtyPlusButton->SetIsEnabled(CommitQty < MaxQty); }
	if (HoldMinusButton) { HoldMinusButton->SetIsEnabled(CommitHold > 1); }
	if (HoldPlusButton) { HoldPlusButton->SetIsEnabled(CommitHold < MaxHold); }
	const bool bManyProducts = Entry->Products.Num() > 1;
	if (ProductPrevButton) { ProductPrevButton->SetIsEnabled(bManyProducts); }
	if (ProductNextButton) { ProductNextButton->SetIsEnabled(bManyProducts); }

	if (CommitBudgetText)
	{
		FText Budgeted;
		FLinearColor Colour = DeadlineUI::Body;
		switch (Refusal)
		{
		case ECommitRefusal::AlreadyCommitted:
			Budgeted = NSLOCTEXT("Deadline", "CommitAlready", "Bu üründe zaten açık bir taahhüdün var.");
			Colour = DeadlineUI::Warn;
			break;
		case ECommitRefusal::NotEnoughFunds:
			Budgeted = FText::Format(NSLOCTEXT("Deadline", "CommitNoFunds", "Serbest paran yetmiyor: {0} gerekli, {1} var."),
				ForecastFormat::Money(Budget), ForecastFormat::Money(Economy->GetAvailableFunds()));
			Colour = DeadlineUI::Loss;
			break;
		default:
			Budgeted = FText::Format(NSLOCTEXT("Deadline", "CommitBudget",
				"{0} kilitlenir, gün {1}'e kadar. Değerlendirme: gün {2}."),
				ForecastFormat::Money(Budget), FText::AsNumber(Entry->Day), FText::AsNumber(Entry->Day + CommitHold));
			break;
		}
		CommitBudgetText->SetText(Budgeted);
		CommitBudgetText->SetColorAndOpacity(FSlateColor(Colour));
	}
	if (CommitButtonText) { CommitButtonText->SetText(NSLOCTEXT("Deadline", "CommitDo", "TAAHHÜT ET")); }
	if (CommitButton) { CommitButton->SetIsEnabled(Refusal == ECommitRefusal::None); }
}

void UForecastScreenWidget::StepProduct(int32 Delta)
{
	const FForecastEntry* Entry = Board.FindByPredicate([this](const FForecastEntry& E) { return E.EventID == CommitForEventID; });
	if (!Entry || Entry->Products.Num() == 0)
	{
		return;
	}
	const int32 Count = Entry->Products.Num();
	CommitProductIndex = ((CommitProductIndex + Delta) % Count + Count) % Count;
	RefreshCommitPanel();
}

void UForecastScreenWidget::HandleProductPrev() { StepProduct(-1); }
void UForecastScreenWidget::HandleProductNext() { StepProduct(1); }
void UForecastScreenWidget::HandleQtyMinus() { --CommitQty; RefreshCommitPanel(); }
void UForecastScreenWidget::HandleQtyPlus() { ++CommitQty; RefreshCommitPanel(); }
void UForecastScreenWidget::HandleHoldMinus() { --CommitHold; RefreshCommitPanel(); }
void UForecastScreenWidget::HandleHoldPlus() { ++CommitHold; RefreshCommitPanel(); }

void UForecastScreenWidget::HandleCommitClicked()
{
	UForecastSubsystem* Forecast = GetForecast();
	const FForecastEntry* Entry = Board.FindByPredicate([this](const FForecastEntry& E) { return E.EventID == CommitForEventID; });
	if (!Forecast || !Entry || Entry->Products.Num() == 0)
	{
		return;
	}
	// Either call succeeds into OnBoardChanged, which refreshes the screen.
	if (Entry->CommitmentID != INDEX_NONE)
	{
		Forecast->CancelCommitment(Entry->CommitmentID);
		return;
	}
	const FName Product = Entry->Products[FMath::Clamp(CommitProductIndex, 0, Entry->Products.Num() - 1)].ProductID;
	Forecast->Commit(Entry->EventID, Product, CommitQty, CommitHold);
}

// --- Events --------------------------------------------------------------------

void UForecastScreenWidget::HandleCardClicked(FName EventID)
{
	SelectEntry(EventID);
}

void UForecastScreenWidget::HandleBoardChanged()
{
	Refresh();
}

void UForecastScreenWidget::HandleBulletin(const FNewsBulletin& Bulletin)
{
	Refresh();
}

void UForecastScreenWidget::HandleCloseClicked()
{
	if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOwningPlayer()))
	{
		PC->CloseForecastScreen();
	}
}

UForecastSubsystem* UForecastScreenWidget::GetForecast() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UForecastSubsystem>() : nullptr;
}
