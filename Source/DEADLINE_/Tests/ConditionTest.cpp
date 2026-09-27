// Copyright DEADLINE. All Rights Reserved.
//
// Spoilage and obsolescence (GDD 5.1).
//
// What this is really guarding: the ways goods could get younger. Every one of
// them is a hole that makes shelf life free, and every one of them is a normal
// thing a player does -- pick a box up, put it back, break a pallet down, load
// a van, save and reload. A single missed stamp and S09 (six-day shelf life)
// never spoils again, which nobody would notice until the cold zone stopped
// being a constraint and the whole GDD 11 pressure quietly evaporated.
//
// It also pins the two rules apart. Spoilage destroys goods; obsolescence only
// makes them cheaper. Conflating them is the easiest mistake here, and it would
// either delete electronics or make food free.
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "D:\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.Inventory.Condition;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log

#include "CoreMinimal.h"
#include "Core/DeadlineSettings.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Data/ProductRow.h"
#include "Economy/EconomySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Fleet/FleetSubsystem.h"
#include "Inventory/Condition.h"
#include "Inventory/InventorySubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr double DayMinutes = DeadlineCondition::MinutesPerDay;

	/** S09 — a cold tote with a six-day shelf life. The tightest in the game. */
	const FName Perishable(TEXT("S09"));

	/** E05 — the fastest-obsoleting product in DT_Products, and it never rots. */
	const FName Obsoleting(TEXT("E05"));

	/** I01 — the only pallet product that spoils, so it tests both at once. */
	const FName SpoilingPallet(TEXT("I01"));
}

// --- The arithmetic, with no engine at all ------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineConditionMathTest,
	"Deadline.Inventory.ConditionMath",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineConditionMathTest::RunTest(const FString& Parameters)
{
	// A product with no obsolescence never loses a cent, however long it sits.
	TestEqual(TEXT("No obsolescence, no loss"),
		DeadlineCondition::ObsolescenceMultiplier(0.f, 500.0, 0.25f), 1.f);

	// Percent per day, compounding. 0.6%/day for 30 days keeps 0.994^30.
	const float Kept30 = DeadlineCondition::ObsolescenceMultiplier(0.6f, 30.0, 0.f);
	TestEqual(TEXT("0.6%/day over 30 days"), Kept30,
		static_cast<float>(FMath::Pow(0.994, 30.0)), 1e-5f);
	TestTrue(TEXT("A month costs a graphics card real money, not all of it"),
		Kept30 > 0.80f && Kept30 < 0.86f);

	// Read as a raw fraction instead of a percentage, 0.6 would mean 60% a day
	// and this number would be about 0.000002. That is the reading this pins.
	TestTrue(TEXT("Not read as a fraction"), Kept30 > 0.5f);

	// The floor holds however long you wait: old stock is cheap, never free.
	TestEqual(TEXT("Floor holds"),
		DeadlineCondition::ObsolescenceMultiplier(0.6f, 100000.0, 0.25f), 0.25f, 1e-4f);

	// Spoilage is a cliff, not a slope, and 0 means never.
	TestFalse(TEXT("Fresh"), DeadlineCondition::HasSpoiled(6, 0.0, 5.9 * DayMinutes));
	TestTrue(TEXT("Spoiled on the day"), DeadlineCondition::HasSpoiled(6, 0.0, 6.0 * DayMinutes));
	TestFalse(TEXT("Zero shelf life never spoils"),
		DeadlineCondition::HasSpoiled(0, 0.0, 100000.0 * DayMinutes));

	// -1 means "these goods do not spoil", which is a different answer from
	// "plenty of time" and the UI shows it differently.
	TestEqual(TEXT("Never-spoils reports -1"),
		DeadlineCondition::DaysUntilSpoilage(0, 0.0, 5.0 * DayMinutes), -1);
	TestEqual(TEXT("Two days left"),
		DeadlineCondition::DaysUntilSpoilage(6, 0.0, 4.0 * DayMinutes), 2);
	TestEqual(TEXT("Never negative"),
		DeadlineCondition::DaysUntilSpoilage(6, 0.0, 99.0 * DayMinutes), 0);

	return true;
}

// --- The ledger, on a standalone game instance --------------------------------

namespace
{
	UGameInstance* MakeGame()
	{
		UGameInstance* GI = NewObject<UGameInstance>(GEngine);
		if (GI)
		{
			GI->InitializeStandalone();
		}
		return GI;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineConditionTest,
	"Deadline.Inventory.Condition",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineConditionTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = MakeGame();
	UInventorySubsystem* Inventory = GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
	UFleetSubsystem* Fleet = GI ? GI->GetSubsystem<UFleetSubsystem>() : nullptr;
	const UProductCatalogSubsystem* Catalogue = GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
	if (!Inventory || !Fleet || !Catalogue)
	{
		AddError(TEXT("Could not create a game instance with the subsystems."));
		return false;
	}

	// The catalogue has to actually say what this test assumes about it.
	const FProductRow* Cold = Catalogue->FindProduct(Perishable);
	const FProductRow* Chip = Catalogue->FindProduct(Obsoleting);
	if (!Cold || !Chip)
	{
		AddError(TEXT("DT_Products is missing S09 or E05."));
		return false;
	}
	TestTrue(TEXT("S09 spoils"), Cold->ShelfLifeDays > 0);
	TestEqual(TEXT("S09 does not obsolete"), Cold->ObsolescencePerDay, 0.f);
	TestEqual(TEXT("E05 never spoils"), Chip->ShelfLifeDays, 0);
	TestTrue(TEXT("E05 obsoletes"), Chip->ObsolescencePerDay > 0.f);

	// --- Spoilage destroys goods -------------------------------------------

	TestTrue(TEXT("Cold totes in"), Inventory->AddStock(Perishable, 4, /*bRecorded=*/true));
	TestEqual(TEXT("Fresh on arrival"), Inventory->GetDaysUntilSpoilage(Perishable),
		Cold->ShelfLifeDays);

	// One day short of the shelf life: still there, still on the books.
	Inventory->AgeAllStockByDays(static_cast<float>(Cold->ShelfLifeDays) - 1.f);
	TestEqual(TEXT("Still on the shelf the day before"),
		Inventory->GetPhysicalStock(Perishable), 4);
	TestEqual(TEXT("With one day left on it"), Inventory->GetDaysUntilSpoilage(Perishable), 1);

	Inventory->AgeAllStockByDays(1.f);
	TestEqual(TEXT("Gone the day it expires"), Inventory->GetPhysicalStock(Perishable), 0);
	TestEqual(TEXT("And off the books with it"),
		Inventory->GetRecordedStock(Perishable), 0);

	// --- Grey goods rot without paperwork, and the discrepancy survives -----

	TestTrue(TEXT("Two white"), Inventory->AddStock(Perishable, 2, /*bRecorded=*/true));
	TestTrue(TEXT("Two grey"), Inventory->AddStock(Perishable, 2, /*bRecorded=*/false));
	TestEqual(TEXT("Two containers unaccounted for"),
		Inventory->GetDiscrepancy(Perishable), 2.f, 0.01f);

	Inventory->AgeAllStockByDays(static_cast<float>(Cold->ShelfLifeDays));
	TestEqual(TEXT("The lot rots"), Inventory->GetPhysicalStock(Perishable), 0);
	TestEqual(TEXT("Nothing left on the books either"),
		Inventory->GetRecordedStock(Perishable), 0);
	TestEqual(TEXT("And no phantom discrepancy behind it"),
		Inventory->GetDiscrepancy(Perishable), 0.f, 0.01f);

	// --- Obsolescence takes value, not goods --------------------------------

	TestTrue(TEXT("Secure cases in"), Inventory->AddStock(Obsoleting, 4, true));
	TestEqual(TEXT("New stock is worth full price"),
		Inventory->GetConditionMultiplier(Obsoleting), 1.f, 1e-4f);

	Inventory->AgeAllStockByDays(60.f);
	TestEqual(TEXT("Nothing spoiled -- E05 has no shelf life"),
		Inventory->GetPhysicalStock(Obsoleting), 4);

	const float After60 = Inventory->GetConditionMultiplier(Obsoleting);
	TestTrue(TEXT("Two months cost real value"), After60 < 0.95f);
	TestTrue(TEXT("But not everything"),
		After60 >= UDeadlineSettings::Get().ObsolescenceFloor);

	// A fresh crate landing on old ones must not rejuvenate the stack. This is
	// the exact failure a single average-age number would produce.
	TestTrue(TEXT("One fresh case in"), Inventory->AddStock(Obsoleting, 1, true));
	const float Mixed = Inventory->GetConditionMultiplier(Obsoleting);
	TestTrue(TEXT("Fresh stock lifts the average a little"), Mixed > After60);
	TestTrue(TEXT("...but nowhere near back to new"), Mixed < 1.f);

	// --- Handling goods does not renew them ---------------------------------
	// Every one of these is something a player does without thinking, and each
	// is a way age could be washed off.

	Inventory->ResetAll();
	TestTrue(TEXT("A pallet in"), Inventory->AddStock(SpoilingPallet, 1, true));
	Inventory->AgeAllStockByDays(50.f);
	const int32 LeftBefore = Inventory->GetDaysUntilSpoilage(SpoilingPallet);

	// Break it into 16 boxes (GDD 10.2) and gather it back up.
	TestTrue(TEXT("Open the pallet"), Inventory->OpenPallet(SpoilingPallet, 1));
	TestEqual(TEXT("Opening a pallet does not refresh it"),
		Inventory->GetDaysUntilSpoilage(SpoilingPallet), LeftBefore);
	TestTrue(TEXT("Close it again"), Inventory->ClosePallet(SpoilingPallet, 1));
	TestEqual(TEXT("Nor does closing it"),
		Inventory->GetDaysUntilSpoilage(SpoilingPallet), LeftBefore);

	// Take it off the shelf and put it straight back, the way carrying a box does.
	const double CarriedStamp = Inventory->GetOldestAcquiredMinute(SpoilingPallet, /*bLoose=*/false);
	TestTrue(TEXT("Off the shelf"), Inventory->RemoveStock(SpoilingPallet, 1, true));
	TestTrue(TEXT("And back on"),
		Inventory->AddStockAged(SpoilingPallet, 1, true, CarriedStamp));
	TestEqual(TEXT("Carrying a box does not refresh it"),
		Inventory->GetDaysUntilSpoilage(SpoilingPallet), LeftBefore);

	// Ride it around in a truck. A hold is not a fridge that stops time.
	Fleet->RegisterVehicle(TEXT("TestVan"), TEXT("V01"));
	const double LoadStamp = Inventory->GetOldestAcquiredMinute(SpoilingPallet, false);
	TestTrue(TEXT("Off the shelf again"), Inventory->RemoveStock(SpoilingPallet, 1, true));
	TestTrue(TEXT("Aboard"),
		Fleet->LoadAged(TEXT("TestVan"), SpoilingPallet, 1, true, /*bLoose=*/false, LoadStamp));
	TestEqual(TEXT("The hold keeps the stamp"),
		Fleet->GetOldestAcquiredMinute(TEXT("TestVan"), SpoilingPallet, false), LoadStamp, 1e-6);
	TestTrue(TEXT("Back off"), Fleet->Unload(TEXT("TestVan"), SpoilingPallet, 1, true, false));
	TestTrue(TEXT("Back on the shelf"),
		Inventory->AddStockAged(SpoilingPallet, 1, true, LoadStamp));
	TestEqual(TEXT("A round trip in the van does not refresh it"),
		Inventory->GetDaysUntilSpoilage(SpoilingPallet), LeftBefore);

	// --- Oldest goods leave first -------------------------------------------
	// If the newest went first, the old stock would sit there rotting and the
	// player would never see a batch expire at all.

	Inventory->ResetAll();
	const double Now = 0.0;
	TestTrue(TEXT("An old crate"),
		Inventory->AddStockAged(Obsoleting, 1, true, Now - 40.0 * DayMinutes));
	TestTrue(TEXT("And a new one"),
		Inventory->AddStockAged(Obsoleting, 1, true, Now));

	const double OldestBefore = Inventory->GetOldestAcquiredMinute(Obsoleting, false);
	TestTrue(TEXT("Sell one"), Inventory->RemoveStock(Obsoleting, 1, true));
	TestTrue(TEXT("The one that left was the old one"),
		Inventory->GetOldestAcquiredMinute(Obsoleting, false) > OldestBefore);

	// --- A save with no ages loads as fresh, not as ruined ------------------

	FInventoryEntry Legacy;
	Legacy.PhysicalStock = 3;
	Legacy.RecordedStock = 3;
	Inventory->RestoreEntry(Perishable, Legacy);
	TestEqual(TEXT("A v4 save keeps its stock"),
		Inventory->GetPhysicalStock(Perishable), 3);
	TestEqual(TEXT("...and starts its clock now rather than rotting on sight"),
		Inventory->GetDaysUntilSpoilage(Perishable), Cold->ShelfLifeDays);

	return true;
}

// --- What the loss does to the money ------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineSpoilageEconomyTest,
	"Deadline.Economy.Spoilage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineSpoilageEconomyTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = MakeGame();
	UInventorySubsystem* Inventory = GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
	UEconomySubsystem* Economy = GI ? GI->GetSubsystem<UEconomySubsystem>() : nullptr;
	if (!Inventory || !Economy)
	{
		AddError(TEXT("Could not create a game instance with the subsystems."));
		return false;
	}

	// Buy perishables, then let them rot. The money spent has to show up as a
	// realised loss, not vanish along with the goods.
	TestTrue(TEXT("Buy four"),
		Economy->TryBuy(Perishable, 4, ETradeLedger::White, EPaymentMethod::Bank));

	const float Spent = -Economy->GetTransactionsFor(Perishable)[0].Amount;
	TestTrue(TEXT("It cost something"), Spent > 0.f);

	const int32 ShelfLife = Inventory->GetDaysUntilSpoilage(Perishable);
	Inventory->AgeAllStockByDays(static_cast<float>(ShelfLife) + 1.f);

	TestEqual(TEXT("The goods are gone"), Inventory->GetPhysicalStock(Perishable), 0);

	const FProductPnL PnL = Economy->GetProductPnL(Perishable);
	TestEqual(TEXT("Nothing held"), PnL.ContainersHeld, 0);
	TestEqual(TEXT("The whole purchase is a realised loss"),
		PnL.RealisedProfit, -Spent, FMath::Max(0.01f, Spent * 0.001f));
	TestEqual(TEXT("And nothing is left to be worth anything"), PnL.MarketValue, 0.f, 0.01f);

	// Obsolescence, by contrast, only moves the price. The goods stay put.
	TestTrue(TEXT("Buy a chip"),
		Economy->TryBuy(Obsoleting, 1, ETradeLedger::White, EPaymentMethod::Bank));
	const float Quote = Economy->GetSellPrice(Obsoleting);

	Inventory->AgeAllStockByDays(90.f);
	TestEqual(TEXT("Still on the shelf"), Inventory->GetPhysicalStock(Obsoleting), 1);
	TestEqual(TEXT("The market quote itself has not moved"),
		Economy->GetSellPrice(Obsoleting), Quote, 0.01f);
	TestTrue(TEXT("But your stock fetches less than the quote"),
		Economy->GetSellPriceForHeldStock(Obsoleting) < Quote);

	// And selling it actually pays the lower number, not the quote.
	const float BankBefore = Economy->GetBank();
	const float Expected = Economy->GetSellPriceForHeldStock(Obsoleting);
	TestTrue(TEXT("Sell it"),
		Economy->TrySell(Obsoleting, 1, ETradeLedger::White, EPaymentMethod::Bank));
	TestEqual(TEXT("Paid the aged price"),
		Economy->GetBank() - BankBefore, Expected, FMath::Max(0.01f, Expected * 0.001f));

	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
