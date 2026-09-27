// Copyright DEADLINE. All Rights Reserved.
//
// The forecast board (GDD 17). What the board promises the player:
//
//   - it lists exactly today's open signals and the events running;
//   - exposure is the value of what you hold in the products at stake, and
//     the gain range is that value times the event's range;
//   - the range is the table's, never the calendar's rolled number, so the
//     board cannot give a forecast away.
//
// Also guards the per-product stock valuation it relies on: the total the HUD
// shows and the per-product figures the board shows must agree.
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "C:\Users\PC\Desktop\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.Forecast;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log

#include "CoreMinimal.h"
#include "Core/DeadlineSettings.h"
#include "Core/SaveSubsystem.h"
#include "Core/TimeSubsystem.h"
#include "Data/EventRow.h"
#include "Economy/EconomySubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Events/EventSubsystem.h"
#include "Forecast/ForecastSubsystem.h"
#include "Inventory/InventorySubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineForecastBoardTest,
	"Deadline.Forecast.Board",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineForecastBoardTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->InitializeStandalone();
	USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
	UTimeSubsystem* Time = GI->GetSubsystem<UTimeSubsystem>();
	UEventSubsystem* Events = GI->GetSubsystem<UEventSubsystem>();
	UForecastSubsystem* Forecast = GI->GetSubsystem<UForecastSubsystem>();
	UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>();
	UEconomySubsystem* Economy = GI->GetSubsystem<UEconomySubsystem>();
	const UDataTable* Table = UDeadlineSettings::Get().EventTable.LoadSynchronous();
	if (!Save || !Time || !Events || !Forecast || !Inventory || !Economy || !Table)
	{
		AddError(TEXT("Missing subsystems or DT_Events"));
		return false;
	}
	Save->StartNewGame(2024);

	// Walk forward to the first day with a price signal on the board.
	const FForecastEntry* Target = nullptr;
	TArray<FForecastEntry> Board;
	for (int32 Step = 0; Step < 40 && !Target; ++Step)
	{
		Board = Forecast->GetBoard();
		Target = Board.FindByPredicate([](const FForecastEntry& E)
		{
			return E.Status == EForecastStatus::Signal && E.Products.Num() > 0;
		});
		if (!Target)
		{
			Time->AdvanceMinutes(UTimeSubsystem::MinutesPerDay);
		}
	}
	if (!TestNotNull(TEXT("A price signal appears within 40 days"), Target))
	{
		return false;
	}
	const int32 Today = Time->GetDay();

	// Exactly today's signals and running events.
	TestEqual(TEXT("Board = signals + active events"),
		Board.Num(), Events->GetSignals(Today).Num() + Events->GetActiveEvents(Today).Num());

	// The range is the table's.
	const FEventRow* Row = Table->FindRow<FEventRow>(Target->EventID, TEXT("test"), false);
	if (TestNotNull(TEXT("Target event row"), Row))
	{
		TestEqual(TEXT("ImpactMin is the table's"), Target->ImpactMin, Row->ImpactMin);
		TestEqual(TEXT("ImpactMax is the table's"), Target->ImpactMax, Row->ImpactMax);
	}
	TestTrue(TEXT("Signal carries a confidence below certainty"), Target->Confidence > 0.f && Target->Confidence < 1.f);
	TestEqual(TEXT("DaysAway matches the day"), Target->DaysAway, Target->Day - Today);
	TestEqual(TEXT("No stock, no exposure"), Target->Exposure, 0.f);

	// Buy into one of the products at stake: exposure follows.
	const FName Product = Target->Products[0].ProductID;
	const FName EventID = Target->EventID;
	TestTrue(TEXT("Stock added"), Inventory->AddStock(Product, 3, true));

	const TArray<FForecastEntry> After = Forecast->GetBoard();
	const FForecastEntry* Updated = After.FindByPredicate([EventID](const FForecastEntry& E) { return E.EventID == EventID; });
	if (TestNotNull(TEXT("Entry still on the board"), Updated))
	{
		const float Value = Economy->GetStockValueOf(Product);
		TestTrue(TEXT("Held stock has value"), Value > 0.f);
		TestEqual(TEXT("Exposure = value of what you hold"), Updated->Exposure, Value, 0.01f);
		TestEqual(TEXT("Gain low = exposure x ImpactMin"), Updated->GainIfHappensMin, Value * Updated->ImpactMin, 0.01f);
		TestEqual(TEXT("Gain high = exposure x ImpactMax"), Updated->GainIfHappensMax, Value * Updated->ImpactMax, 0.01f);
		TestEqual(TEXT("Product you hold is listed first"), Updated->Products[0].ProductID, Product);
		TestEqual(TEXT("Held containers"), Updated->Products[0].HeldContainers, 3.f, 0.001f);
	}
	TestTrue(TEXT("Total exposure counts it"), Forecast->GetTotalExposure() >= Economy->GetStockValueOf(Product) - 0.01f);

	// A second product, then the refactored total must equal the per-product sum.
	TestTrue(TEXT("Other stock added"), Inventory->AddStock(TEXT("S01"), 2, true));
	const float Sum = Economy->GetStockValueOf(Product) + (Product == TEXT("S01") ? 0.f : Economy->GetStockValueOf(TEXT("S01")));
	TestEqual(TEXT("GetStockValue = sum of GetStockValueOf"), Economy->GetStockValue(), Sum, 0.01f);

	GI->Shutdown();
	return true;
}

// Commitments: what the lock promises and how the judgement is counted.
//
//   - the budget is locked: the free funds drop by it, a second commitment
//     on the same product is refused, and more than the free funds is refused;
//   - buying the product draws on its own lock; selling is counted up to what
//     was bought for it;
//   - the lock comes back on the expected day, the verdict comes HoldDays
//     later and matches what the calendar actually did;
//   - result = proceeds + today's value of the unsold rest - spent;
//   - a save and a load put an open commitment and its lock back.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineForecastCommitmentTest,
	"Deadline.Forecast.Commitment",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineForecastCommitmentTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->InitializeStandalone();
	USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>();
	UTimeSubsystem* Time = GI->GetSubsystem<UTimeSubsystem>();
	UEventSubsystem* Events = GI->GetSubsystem<UEventSubsystem>();
	UForecastSubsystem* Forecast = GI->GetSubsystem<UForecastSubsystem>();
	UEconomySubsystem* Economy = GI->GetSubsystem<UEconomySubsystem>();
	if (!Save || !Time || !Events || !Forecast || !Economy)
	{
		AddError(TEXT("Missing subsystems"));
		return false;
	}
	Save->StartNewGame(2024);

	// Day 0 always has a signal (EventCalendar never-quiet rule); walk on to
	// one that pushes prices and is at least two days off, so there is a day
	// to buy on before it lands.
	FForecastEntry Target;
	bool bFound = false;
	for (int32 Step = 0; Step < 40 && !bFound; ++Step)
	{
		for (const FForecastEntry& E : Forecast->GetBoard())
		{
			if (E.Status == EForecastStatus::Signal && E.Products.Num() > 0 && E.ImpactMax > 0.f && E.DaysAway >= 2)
			{
				Target = E;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			Time->AdvanceMinutes(UTimeSubsystem::MinutesPerDay);
		}
	}
	if (!TestTrue(TEXT("A price signal two or more days off appears within 40 days"), bFound))
	{
		return false;
	}
	const FName Product = Target.Products[0].ProductID;
	const float Price = Economy->GetBuyPrice(Product);
	const float FreeBefore = Economy->GetAvailableFunds();

	// --- Refusals and the lock --------------------------------------------------
	TestEqual(TEXT("Not a product it pushes"), Forecast->CanCommit(Target.EventID, TEXT("NOPE"), 1), ECommitRefusal::NotOnBoard);
	TestEqual(TEXT("Zero containers"), Forecast->CanCommit(Target.EventID, Product, 0), ECommitRefusal::BadAmount);
	TestEqual(TEXT("More than the free funds"),
		Forecast->CanCommit(Target.EventID, Product, FMath::CeilToInt(FreeBefore / Price) + 1), ECommitRefusal::NotEnoughFunds);

	const int32 Qty = FMath::Min(5, Forecast->GetMaxAffordable(Product));
	if (!TestTrue(TEXT("Can afford at least 3"), Qty >= 3))
	{
		return false;
	}
	const int32 ID = Forecast->Commit(Target.EventID, Product, Qty, 2);
	if (!TestTrue(TEXT("Committed"), ID != INDEX_NONE))
	{
		return false;
	}
	TestEqual(TEXT("Locked = qty x price"), Economy->GetLockedFundsFor(Product), Price * Qty, 0.01f);
	TestEqual(TEXT("Free funds drop by the lock"), Economy->GetAvailableFunds(), FreeBefore - Price * Qty, 0.01f);
	TestEqual(TEXT("Total funds untouched"), Economy->GetTotalFunds(), FreeBefore, 0.01f);
	TestEqual(TEXT("Second commitment on the product refused"),
		Forecast->CanCommit(Target.EventID, Product, 1), ECommitRefusal::AlreadyCommitted);
	const TArray<FForecastEntry> Board = Forecast->GetBoard();
	const FForecastEntry* Marked = Board.FindByPredicate([&Target](const FForecastEntry& E) { return E.EventID == Target.EventID; });
	TestTrue(TEXT("Board marks the entry"), Marked && Marked->CommitmentID == ID);

	// --- Buying draws on the lock, selling is counted ------------------------------
	const float LockBefore = Economy->GetLockedFundsFor(Product);
	if (!TestTrue(TEXT("Bought 3 against the lock"), Economy->TryBuy(Product, 3, ETradeLedger::White, EPaymentMethod::Bank)))
	{
		return false;
	}
	FForecastCommitment C;
	Forecast->GetCommitment(ID, C);
	TestEqual(TEXT("Bought counted"), C.BoughtContainers, 3);
	TestEqual(TEXT("Lock shrinks by the cost"), Economy->GetLockedFundsFor(Product), FMath::Max(0.f, LockBefore - C.Spent), 0.01f);
	TestEqual(TEXT("Commitment mirrors the lock"), C.LockRemaining, Economy->GetLockedFundsFor(Product), 0.01f);
	TestTrue(TEXT("Sold one"), Economy->TrySell(Product, 1, ETradeLedger::White, EPaymentMethod::Bank));
	Forecast->GetCommitment(ID, C);
	TestEqual(TEXT("Sale counted"), C.SoldContainers, 1);
	TestTrue(TEXT("Proceeds recorded"), C.Proceeds > 0.f);

	// --- Save and load put it back ----------------------------------------------------
	const FString Slot = TEXT("DeadlineAutomationCommitment");
	if (TestTrue(TEXT("Saved"), Save->SaveGame(Slot)))
	{
		const float LockSaved = Economy->GetLockedFundsFor(Product);
		Save->StartNewGame(99);
		TestEqual(TEXT("New game clears commitments"), Forecast->GetCommitments().Num(), 0);
		TestEqual(TEXT("New game clears locks"), Economy->GetLockedFunds(), 0.f);
		TestTrue(TEXT("Loaded"), Save->LoadGame(Slot));
		FForecastCommitment Loaded;
		TestTrue(TEXT("Commitment back"), Forecast->GetCommitment(ID, Loaded) && Loaded.State == ECommitmentState::Open);
		TestEqual(TEXT("Lock back"), Economy->GetLockedFundsFor(Product), LockSaved, 0.01f);
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
	}

	// --- The day: lock released, then the verdict ----------------------------------------
	while (Time->GetDay() < Target.Day)
	{
		Time->AdvanceMinutes(UTimeSubsystem::MinutesPerDay);
	}
	Forecast->GetCommitment(ID, C);
	TestEqual(TEXT("Holding after the expected day"), C.State, ECommitmentState::Holding);
	TestEqual(TEXT("Lock released"), Economy->GetLockedFunds(), 0.f);

	FForecastCommitment Resolved;
	while (Time->GetDay() < Target.Day + 2)
	{
		Time->AdvanceMinutes(UTimeSubsystem::MinutesPerDay);
	}
	Forecast->GetCommitment(ID, Resolved);
	if (TestTrue(TEXT("Judged on the resolve day"), Resolved.State == ECommitmentState::Resolved))
	{
		bool bTruth = false;
		for (const FScheduledEvent& E : Events->GetCalendar(0, Target.Day))
		{
			if (E.EventID == Target.EventID && E.StartDay == Target.Day)
			{
				bTruth = E.bHappens;
			}
		}
		TestEqual(TEXT("Verdict matches the calendar"), Resolved.bEventHappened, bTruth);
		const float Expected = Resolved.Proceeds + Economy->GetSellPrice(Product) * (Resolved.BoughtContainers - Resolved.SoldContainers) - Resolved.Spent;
		TestEqual(TEXT("Result = proceeds + rest - spent"), Resolved.Result, Expected, 0.01f);
	}

	// --- Cancel ----------------------------------------------------------------------------
	for (int32 Step = 0; Step < 40; ++Step)
	{
		const TArray<FForecastEntry> Now = Forecast->GetBoard();
		const FForecastEntry* Open = Now.FindByPredicate([](const FForecastEntry& E)
		{
			return E.Status == EForecastStatus::Signal && E.Products.Num() > 0 && E.ImpactMax > 0.f;
		});
		if (Open)
		{
			const int32 Other = Forecast->Commit(Open->EventID, Open->Products[0].ProductID, 1, 1);
			TestTrue(TEXT("Second commitment made"), Other != INDEX_NONE);
			TestTrue(TEXT("Cancelled"), Forecast->CancelCommitment(Other));
			TestEqual(TEXT("Cancel releases the lock"), Economy->GetLockedFunds(), 0.f);
			FForecastCommitment Gone;
			Forecast->GetCommitment(Other, Gone);
			TestEqual(TEXT("State cancelled"), Gone.State, ECommitmentState::Cancelled);
			break;
		}
		Time->AdvanceMinutes(UTimeSubsystem::MinutesPerDay);
	}

	GI->Shutdown();
	return true;
}

#endif
