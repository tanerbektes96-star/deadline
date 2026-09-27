// Copyright DEADLINE. All Rights Reserved.

#include "UI/CommitmentResultWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Core/DeadlinePlayerController.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "Events/EventSubsystem.h"
#include "UI/DeadlineUIPalette.h"
#include "UI/ForecastFormat.h"

namespace
{
	FText Percent(float Fraction)
	{
		return FText::AsNumber(FMath::RoundToInt(Fraction * 100.f));
	}

	/** "+$1.240" / "−$380": the sign outside the currency, as people say it. */
	FText SignedMoney(float Value)
	{
		return FText::Format(NSLOCTEXT("Deadline", "SignedMoney", "{0}{1}"),
			FText::FromString(Value < 0.f ? TEXT("−") : TEXT("+")), ForecastFormat::Money(FMath::Abs(Value)));
	}
}

void UCommitmentResultWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UCommitmentResultWidget::HandleCloseClicked);
	}
}

void UCommitmentResultWidget::SetCommitment(const FForecastCommitment& C, int32 QueueLeft)
{
	const UGameInstance* GI = GetGameInstance();
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	const UEventSubsystem* Events = GI ? GI->GetSubsystem<UEventSubsystem>() : nullptr;
	const FString Product = Catalogue ? Catalogue->GetDisplayName(C.ProductID) : C.ProductID.ToString();
	const FString Event = Events ? Events->GetEventName(C.EventID) : C.EventID.ToString();

	const bool bProfit = C.Result > 0.5f;
	const bool bLoss = C.Result < -0.5f;
	const FLinearColor Tone = bProfit ? DeadlineUI::Profit : (bLoss ? DeadlineUI::Loss : DeadlineUI::Muted);

	if (ResultFrame)
	{
		ResultFrame->SetBrushColor(Tone);
	}
	if (VerdictText)
	{
		VerdictText->SetText(C.bEventHappened
			? NSLOCTEXT("Deadline", "ResultRight", "TAHMİN TUTTU")
			: NSLOCTEXT("Deadline", "ResultWrong", "TAHMİN TUTMADI"));
		VerdictText->SetColorAndOpacity(FSlateColor(C.bEventHappened ? DeadlineUI::Profit : DeadlineUI::Loss));
	}
	if (TitleText)
	{
		TitleText->SetText(FText::Format(NSLOCTEXT("Deadline", "ResultTitle", "{0}  ·  {1}"),
			FText::FromString(Event), FText::FromString(Product)));
	}
	if (OddsText)
	{
		FText What;
		if (!C.bEventHappened)
		{
			What = FText::Format(NSLOCTEXT("Deadline", "ResultDidNot", "{0}. gün beklenen olay gerçekleşmedi"),
				FText::AsNumber(C.ExpectedDay));
		}
		else if (C.EventEndDay != INDEX_NONE)
		{
			// EndDay is the first day it is over; say the last day it ran, and
			// say "will run" if it outlives the judgement.
			const FText LastDay = FText::AsNumber(C.EventEndDay - 1);
			What = C.EventEndDay > C.GetResolveDay()
				? FText::Format(NSLOCTEXT("Deadline", "ResultRunning", "olay {0}. gün başladı, {1}. güne kadar sürecek"),
					FText::AsNumber(C.ExpectedDay), LastDay)
				: FText::Format(NSLOCTEXT("Deadline", "ResultRan", "olay {0}. gün başladı, {1}. günden sonra bitti"),
					FText::AsNumber(C.ExpectedDay), LastDay);
		}
		OddsText->SetText(FText::Format(NSLOCTEXT("Deadline", "ResultOdds", "Kaynak %{0} demişti  ·  {1}"),
			Percent(C.Confidence), What));
	}
	if (OutcomeText)
	{
		FText Outcome;
		if (C.BoughtContainers == 0)
		{
			Outcome = NSLOCTEXT("Deadline", "ResultNoTrade", "İŞLEM YOK");
		}
		else if (bProfit || bLoss)
		{
			Outcome = FText::Format(bProfit ? NSLOCTEXT("Deadline", "ResultGain", "KAZANÇ  {0}")
				: NSLOCTEXT("Deadline", "ResultLoss", "KAYIP  {0}"), SignedMoney(C.Result));
		}
		else
		{
			Outcome = NSLOCTEXT("Deadline", "ResultEven", "BAŞABAŞ");
		}
		OutcomeText->SetText(Outcome);
		OutcomeText->SetColorAndOpacity(FSlateColor(Tone));
	}
	if (LedgerText)
	{
		const int32 Held = FMath::Max(0, C.BoughtContainers - C.SoldContainers);
		const float HeldValue = C.Result - C.Proceeds + C.Spent;
		FText Ledger = FText::Format(NSLOCTEXT("Deadline", "ResultBought", "Alınan:  {0} / {1} konteyner  ·  {2} ödendi"),
			FText::AsNumber(C.BoughtContainers), FText::AsNumber(C.TargetContainers), ForecastFormat::Money(C.Spent));
		if (C.SoldContainers > 0)
		{
			Ledger = FText::Format(NSLOCTEXT("Deadline", "ResultSoldLine", "{0}\nSatılan:  {1}  ·  {2} geldi"),
				Ledger, FText::AsNumber(C.SoldContainers), ForecastFormat::Money(C.Proceeds));
		}
		if (Held > 0)
		{
			Ledger = FText::Format(NSLOCTEXT("Deadline", "ResultHeldLine", "{0}\nElde kalan:  {1}  ·  bugün {2} eder"),
				Ledger, FText::AsNumber(Held), ForecastFormat::Money(HeldValue));
		}
		LedgerText->SetText(Ledger);
	}
	if (LessonTitleText)
	{
		LessonTitleText->SetText(LessonTitle(C));
	}
	if (LessonText)
	{
		LessonText->SetText(LessonBody(C, Product));
	}
	if (QueueText)
	{
		QueueText->SetText(QueueLeft > 0
			? FText::Format(NSLOCTEXT("Deadline", "ResultQueue", "{0} sonuç daha var"), FText::AsNumber(QueueLeft))
			: FText::GetEmpty());
	}
}

FText UCommitmentResultWidget::LessonTitle(const FForecastCommitment& C)
{
	switch (C.Lesson)
	{
	case ECommitmentLesson::GoodCall:
	case ECommitmentLesson::LuckyWin:
		return NSLOCTEXT("Deadline", "LessonWhyWon", "NEDEN KAZANDIN");
	case ECommitmentLesson::MissedIt:
		return NSLOCTEXT("Deadline", "LessonMissed", "NE KAÇTI");
	case ECommitmentLesson::FalseRumourSpared:
		return NSLOCTEXT("Deadline", "LessonSpared", "NE OLDU");
	default:
		return NSLOCTEXT("Deadline", "LessonWhyWrong", "NEDEN YANILDIM");
	}
}

FText UCommitmentResultWidget::LessonBody(const FForecastCommitment& C, const FString& ProductName)
{
	const FText Conf = Percent(C.Confidence);
	const FText Rise = Percent(C.PeakRise);
	const FText BreakEven = Percent(C.BreakEvenRise);
	const FText PeakDay = FText::AsNumber(C.PeakDay);
	const FText Best = SignedMoney(C.BestResult);

	switch (C.Lesson)
	{
	case ECommitmentLesson::GoodCall:
	{
		FText Body = FText::Format(NSLOCTEXT("Deadline", "LessonGoodCall",
			"Kaynağa güvendin ve olay geldi. Fiyat en fazla +%{0} çıktı; alıp satmanın gerektirdiği +%{1}'i geçti."),
			Rise, BreakEven);
		if (C.BestResult > C.Result * 1.25f && C.BestResult - C.Result > 50.f)
		{
			Body = FText::Format(NSLOCTEXT("Deadline", "LessonGoodCallPeak", "{0} Zirvede, {1}. gün satsaydın sonuç {2} olurdu."),
				Body, PeakDay, Best);
		}
		return Body;
	}
	case ECommitmentLesson::MissedIt:
		return C.BestResult > 0.f
			? FText::Format(NSLOCTEXT("Deadline", "LessonMissedIt",
				"Tahmin doğruydu ama hiç {0} alımı yapmadın. Taahhüt ettiğin {1} konteyneri alıp {2}. gün satsaydın sonuç {3} olurdu. Kilitli bütçe, alım yapılmadıkça sadece bekler."),
				FText::FromString(ProductName), FText::AsNumber(C.TargetContainers), PeakDay, Best)
			: FText::Format(NSLOCTEXT("Deadline", "LessonMissedNothing",
				"Tahmin doğruydu ama fiyat en fazla +%{0} çıktı; alıp satmak en az +%{1} ister. Almaman bu kez doğruydu."),
				Rise, BreakEven);
	case ECommitmentLesson::EatenBySpread:
		return FText::Format(NSLOCTEXT("Deadline", "LessonSpread",
			"Olay geldi ama fiyat en fazla +%{0} çıktı. Alışta aracı payı, satışta alıcı indirimi var: alıp satmak en az +%{1} artış ister. Etkisi bu eşiğin altında kalabilecek olaylarda mal bağlama."),
			Rise, BreakEven);
	case ECommitmentLesson::HeldTooLong:
	{
		FText Body = FText::Format(NSLOCTEXT("Deadline", "LessonTiming",
			"Olay geldi ve fiyat yetecek kadar çıktı (+%{0}), ama doğru günde satmadın. En iyi satış {1}. gündü: o gün satsaydın sonuç {2} olurdu."),
			Rise, PeakDay, Best);
		if (C.EventEndDay != INDEX_NONE && C.GetResolveDay() >= C.EventEndDay)
		{
			Body = FText::Format(NSLOCTEXT("Deadline", "LessonTimingEnded",
				"{0} Olay {1}. günden sonra bitti ve fiyat geri çekildi: olay bitmeden sat."),
				Body, FText::AsNumber(C.EventEndDay - 1));
		}
		return Body;
	}
	case ECommitmentLesson::FalseRumour:
		return FText::Format(NSLOCTEXT("Deadline", "LessonFalse",
			"Söylenti çıkmadı. Kaynak %{0} demişti: böyle on söylentiden yaklaşık {1} tanesi boşa çıkar. Karar kötü değildi, sonuç kötü oldu. Güveni düşük söylentilere daha az para bağla."),
			Conf, FText::AsNumber(FMath::Clamp(FMath::RoundToInt((1.f - C.Confidence) * 10.f), 1, 9)));
	case ECommitmentLesson::FalseRumourSpared:
		return FText::Format(NSLOCTEXT("Deadline", "LessonSpared",
			"Söylenti boşa çıktı (kaynak %{0} demişti). Mal almadığın için kaybın yok; kilitli bütçe geri döndü."),
			Conf);
	case ECommitmentLesson::LuckyWin:
		return NSLOCTEXT("Deadline", "LessonLucky",
			"Söylenti boşa çıktı ama fiyat kendi seyrinde yine de lehine gitti. Bu şans: tekrarlanacağına güvenme.");
	default:
		return FText::GetEmpty();
	}
}

void UCommitmentResultWidget::HandleCloseClicked()
{
	if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOwningPlayer()))
	{
		PC->CloseResultCard();
	}
}
