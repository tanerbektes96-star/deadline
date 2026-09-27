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

#endif
