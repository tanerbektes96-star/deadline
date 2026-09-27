// Copyright DEADLINE. All Rights Reserved.
//
// The warehouse rules from GDD 11 and the pallet split from GDD 10.2.
//
// Two things this catches that a play-through would not. First, capacity is
// per storage class: the bug this guards against is a full cold zone quietly
// borrowing space from the empty racks, which would make the whole "what do I
// keep and what do I sell?" pressure disappear. Second, opening and closing a
// pallet has to conserve both ledgers — if a grey pallet came back white, the
// Month 7 inspection would be looking at the wrong number and nobody would
// notice until then.
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "D:\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.Inventory;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log

#include "CoreMinimal.h"
#include "Data/ProductRow.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventorySubsystem.h"
#include "Inventory/StorageClass.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --- The GDD 11 / 5.2 tables, no engine needed --------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineStorageClassTest,
	"Deadline.Inventory.StorageClasses",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineStorageClassTest::RunTest(const FString& Parameters)
{
	// GDD 11: rack 12, pallet bay 16, cold zone 24, caged rack 8.
	TestEqual(TEXT("Rack BU"), FStorageClassRules::BUPerUnit(EStorageClass::Rack), 12.f);
	TestEqual(TEXT("Pallet bay BU"), FStorageClassRules::BUPerUnit(EStorageClass::PalletBay), 16.f);
	TestEqual(TEXT("Cold zone BU"), FStorageClassRules::BUPerUnit(EStorageClass::ColdZone), 24.f);
	TestEqual(TEXT("Secure rack BU"), FStorageClassRules::BUPerUnit(EStorageClass::SecureRack), 8.f);

	// GDD 11 "what it takes" column. Every container type has exactly one home,
	// which is what lets the ledger skip storing a location per box.
	TestEqual(TEXT("BoxS"), FStorageClassRules::ClassForContainer(EContainerType::BoxS), EStorageClass::Rack);
	TestEqual(TEXT("BoxM"), FStorageClassRules::ClassForContainer(EContainerType::BoxM), EStorageClass::Rack);
	TestEqual(TEXT("BoxL"), FStorageClassRules::ClassForContainer(EContainerType::BoxL), EStorageClass::Rack);
	TestEqual(TEXT("Pallet"), FStorageClassRules::ClassForContainer(EContainerType::Pallet), EStorageClass::PalletBay);
	TestEqual(TEXT("Crate"), FStorageClassRules::ClassForContainer(EContainerType::Crate), EStorageClass::PalletBay);
	TestEqual(TEXT("ColdTote"), FStorageClassRules::ClassForContainer(EContainerType::ColdTote), EStorageClass::ColdZone);
	TestEqual(TEXT("SecureCase"), FStorageClassRules::ClassForContainer(EContainerType::SecureCase), EStorageClass::SecureRack);

	// A rack refusing a cold tote is the whole point of the class system.
	TestFalse(TEXT("Rack refuses a cold tote"),
		FStorageClassRules::Accepts(EStorageClass::Rack, EContainerType::ColdTote));
	TestFalse(TEXT("Cold zone refuses a secure case"),
		FStorageClassRules::Accepts(EStorageClass::ColdZone, EContainerType::SecureCase));

	// One pallet is 16 boxes of 1 BU: breaking it down must not create volume.
	TestEqual(TEXT("Pallet BU"), FProductRow::ContainerVolumeBU(EContainerType::Pallet), 16.f);
	TestEqual(TEXT("Boxes per pallet"), FInventoryEntry::BoxesPerPallet, 16);
	TestEqual(TEXT("A loose box is one BU"),
		FProductRow::ContainerVolumeBU(EContainerType::Pallet) / FInventoryEntry::BoxesPerPallet,
		FProductRow::ContainerVolumeBU(EContainerType::BoxM));

	return true;
}

// --- The ledger itself, on a standalone game instance -------------------------

namespace
{
	/** Spin up just enough game for the subsystems to exist. */
	UInventorySubsystem* MakeInventory(UGameInstance*& OutGI)
	{
		OutGI = NewObject<UGameInstance>(GEngine);
		if (!OutGI)
		{
			return nullptr;
		}
		OutGI->InitializeStandalone();
		return OutGI->GetSubsystem<UInventorySubsystem>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineWarehouseTest,
	"Deadline.Inventory.Warehouse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineWarehouseTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = nullptr;
	UInventorySubsystem* Inventory = MakeInventory(GI);
	if (!Inventory)
	{
		AddError(TEXT("Could not create a game instance with an inventory subsystem."));
		return false;
	}

	// The four defaults still add up to the 240 BU the warehouse had in Month 1.
	TestEqual(TEXT("Total starting capacity"), Inventory->GetCapacityBU(), 240.f);

	// --- A class fills on its own ------------------------------------------
	// E08 is a Secure Case: 0.5 BU each into 4 cages of 8 BU = 64 of them.
	const int32 CageCapacity = 64;
	TestTrue(TEXT("Secure rack takes a full load"),
		Inventory->AddStock(TEXT("E08"), CageCapacity, /*bRecorded=*/true));
	TestFalse(TEXT("A full secure rack refuses one more, empty racks or not"),
		Inventory->AddStock(TEXT("E08"), 1, true));
	TestTrue(TEXT("The racks are still empty and still take boxes"),
		Inventory->AddStock(TEXT("S01"), 1, true));   // BoxL, 2 BU
	TestEqual(TEXT("Secure rack used"),
		Inventory->GetClassUsedBU(EStorageClass::SecureRack), 32.f);
	TestEqual(TEXT("Rack used"), Inventory->GetClassUsedBU(EStorageClass::Rack), 2.f);

	// A cold tote does not fit on a rack even with the racks nearly empty.
	TestEqual(TEXT("Cold goods go to the cold zone"),
		Inventory->GetStorageClassFor(TEXT("S08")), EStorageClass::ColdZone);

	Inventory->ResetAll();

	// --- Opening a pallet (GDD 10.2) ---------------------------------------
	// I01 Cement Pallet, 16 BU, white.
	TestTrue(TEXT("Two pallets in"), Inventory->AddStock(TEXT("I01"), 2, true));
	const float BeforeBU = Inventory->GetUsedBU();

	TestTrue(TEXT("Open one"), Inventory->OpenPallet(TEXT("I01"), 1));
	TestEqual(TEXT("One pallet left"), Inventory->GetPhysicalStock(TEXT("I01")), 1);
	TestEqual(TEXT("Sixteen loose boxes"), Inventory->GetLooseBoxes(TEXT("I01")), 16);
	TestEqual(TEXT("Volume is conserved"), Inventory->GetUsedBU(), BeforeBU);
	TestEqual(TEXT("The boxes moved to the racks"),
		Inventory->GetClassUsedBU(EStorageClass::Rack), 16.f);
	TestEqual(TEXT("The bay gave up its space"),
		Inventory->GetClassUsedBU(EStorageClass::PalletBay), 16.f);
	TestTrue(TEXT("A white pallet stays white"),
		FMath::IsNearlyZero(Inventory->GetDiscrepancy(TEXT("I01"))));

	// Carrying one box off the racks, then putting it back.
	TestTrue(TEXT("Take one box"), Inventory->RemoveLooseBoxes(TEXT("I01"), 1, true));
	TestEqual(TEXT("Fifteen left"), Inventory->GetLooseBoxes(TEXT("I01")), 15);
	TestFalse(TEXT("Fifteen boxes do not close a pallet"),
		Inventory->ClosePallet(TEXT("I01"), 1));
	TestTrue(TEXT("Put it back"), Inventory->AddLooseBoxes(TEXT("I01"), 1, true));

	TestTrue(TEXT("Close it again"), Inventory->ClosePallet(TEXT("I01"), 1));
	TestEqual(TEXT("Back to two pallets"), Inventory->GetPhysicalStock(TEXT("I01")), 2);
	TestEqual(TEXT("No loose boxes left"), Inventory->GetLooseBoxes(TEXT("I01")), 0);
	TestEqual(TEXT("Recorded came back whole"), Inventory->GetRecordedStock(TEXT("I01")), 2);

	Inventory->ResetAll();

	// --- A grey pallet stays grey through the split ------------------------
	TestTrue(TEXT("One grey pallet"), Inventory->AddStock(TEXT("I01"), 1, /*bRecorded=*/false));
	TestEqual(TEXT("Grey before"), Inventory->GetDiscrepancy(TEXT("I01")), 1.f);
	TestTrue(TEXT("Open it"), Inventory->OpenPallet(TEXT("I01"), 1));
	TestEqual(TEXT("Still one pallet of unexplained goods"),
		Inventory->GetDiscrepancy(TEXT("I01")), 1.f);
	TestEqual(TEXT("Nothing on the books"), Inventory->GetRecordedLooseBoxes(TEXT("I01")), 0);

	Inventory->ResetAll();

	// --- A sale can reach goods that were broken down ----------------------
	TestTrue(TEXT("One pallet"), Inventory->AddStock(TEXT("I01"), 1, true));
	TestTrue(TEXT("Open it"), Inventory->OpenPallet(TEXT("I01"), 1));
	TestEqual(TEXT("No whole containers"), Inventory->GetPhysicalStock(TEXT("I01")), 0);
	TestTrue(TEXT("Selling gathers the boxes back up"),
		Inventory->RemoveStock(TEXT("I01"), 1, true));
	TestEqual(TEXT("Warehouse is empty"), Inventory->GetUsedBU(), 0.f);
	TestTrue(TEXT("And the books are clean"),
		FMath::IsNearlyZero(Inventory->GetTotalDiscrepancy()));

	// --- Only pallets come apart -------------------------------------------
	TestTrue(TEXT("A crate in the bay"), Inventory->AddStock(TEXT("G01"), 1, true));
	TestFalse(TEXT("Crates do not open"), Inventory->OpenPallet(TEXT("G01"), 1));
	TestFalse(TEXT("Neither do boxes"), Inventory->OpenPallet(TEXT("S01"), 1));

	// --- Resizing a class ---------------------------------------------------
	Inventory->SetUnitCount(EStorageClass::ColdZone, 0);
	TestEqual(TEXT("No cold zone, no cold capacity"),
		Inventory->GetClassCapacityBU(EStorageClass::ColdZone), 0.f);
	TestFalse(TEXT("Frozen goods have nowhere to go"),
		Inventory->AddStock(TEXT("S08"), 1, true));

	GI->Shutdown();
	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
