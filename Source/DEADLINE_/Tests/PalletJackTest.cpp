// Copyright DEADLINE. All Rights Reserved.
//
// Empty pallets and what a person can actually lift (GDD 5.2 / 5.6).
//
// The rule being pinned: a loaded pallet is sixteen boxes and well over half a
// tonne, so nobody picks one up by hand. An empty pallet is wood and air, so
// they do. Everything about the pallet jack follows from that one difference,
// and if it ever stops holding, the jack becomes decoration and the pallet bay
// stops being a place you have to walk to.
//
// The other half is the wood itself. Opening a pallet used to make it vanish;
// now it stays behind as a spare, and closing one needs a spare back. Get that
// wrong in either direction and you either mint pallets out of nothing or
// strand sixteen boxes that can never be a pallet again.
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "D:\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.Inventory.Pallets;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log

#include "CoreMinimal.h"
#include "Actors/ContainerActor.h"
#include "Core/DeadlineSettings.h"
#include "Data/ProductRow.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventorySubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** I01 — a cement pallet. 16 BU, 1200 kg. Nobody is lifting this. */
	const FName PalletProduct(TEXT("I01"));

	/** G01 — a crate. GDD 5.2: crate goes on a forklift, never in arms. */
	const FName CrateProduct(TEXT("G01"));
}

// --- What arms can and cannot do ----------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineCarryRulesTest,
	"Deadline.Inventory.CarryRules",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineCarryRulesTest::RunTest(const FString& Parameters)
{
	// GDD 5.2's "nasıl taşınır" column, as code.
	TestTrue(TEXT("Box S by hand"), AContainerActor::IsHandCarryable(EContainerType::BoxS));
	TestTrue(TEXT("Box M by hand"), AContainerActor::IsHandCarryable(EContainerType::BoxM));
	TestTrue(TEXT("Box L by hand"), AContainerActor::IsHandCarryable(EContainerType::BoxL));
	TestTrue(TEXT("Cold tote by hand"), AContainerActor::IsHandCarryable(EContainerType::ColdTote));
	TestTrue(TEXT("Secure case by hand"), AContainerActor::IsHandCarryable(EContainerType::SecureCase));

	// The two that need the jack. This is the whole reason it exists.
	TestFalse(TEXT("A loaded pallet is not a hand carry"),
		AContainerActor::IsHandCarryable(EContainerType::Pallet));
	TestFalse(TEXT("Nor is a crate"),
		AContainerActor::IsHandCarryable(EContainerType::Crate));

	// And they are refused because of the weight, not by accident: a pallet is
	// sixteen times a box, which is what the BU table already says.
	TestEqual(TEXT("A pallet is sixteen boxes"),
		FProductRow::ContainerVolumeBU(EContainerType::Pallet)
			/ FProductRow::ContainerVolumeBU(EContainerType::BoxM), 16.f);

	return true;
}

// --- The wood ------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlinePalletsTest,
	"Deadline.Inventory.Pallets",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlinePalletsTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	if (!GI)
	{
		AddError(TEXT("Could not create a game instance."));
		return false;
	}
	GI->InitializeStandalone();

	UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>();
	if (!Inventory)
	{
		AddError(TEXT("No inventory subsystem."));
		return false;
	}

	const int32 Starting = UDeadlineSettings::Get().StartingEmptyPallets;
	TestEqual(TEXT("You start with a couple of spares"),
		Inventory->GetEmptyPallets(), Starting);

	// --- Opening leaves the wood behind ------------------------------------

	TestTrue(TEXT("A loaded pallet in"), Inventory->AddStock(PalletProduct, 1, true));
	TestTrue(TEXT("Open it"), Inventory->OpenPallet(PalletProduct, 1));
	TestEqual(TEXT("Sixteen boxes came off"),
		Inventory->GetLooseBoxes(PalletProduct), FInventoryEntry::BoxesPerPallet);
	TestEqual(TEXT("...and the pallet they came off is still here"),
		Inventory->GetEmptyPallets(), Starting + 1);

	// --- Closing uses one up ------------------------------------------------

	TestTrue(TEXT("Close it again"), Inventory->ClosePallet(PalletProduct, 1));
	TestEqual(TEXT("The spare went back under the boxes"),
		Inventory->GetEmptyPallets(), Starting);
	TestEqual(TEXT("And the pallet is whole"),
		Inventory->GetPhysicalStock(PalletProduct), 1);

	// --- Sixteen boxes and nothing to stack them on -------------------------
	// This is the failure GDD 5.6 describes: run out of pallets and the goods
	// are still yours, they just cannot be made into a pallet.

	TestTrue(TEXT("Open it once more"), Inventory->OpenPallet(PalletProduct, 1));
	TestTrue(TEXT("Take every spare away"),
		Inventory->RemoveEmptyPallets(Inventory->GetEmptyPallets()));
	TestEqual(TEXT("None left"), Inventory->GetEmptyPallets(), 0);

	TestFalse(TEXT("Cannot rebuild a pallet without a pallet"),
		Inventory->ClosePallet(PalletProduct, 1));
	TestEqual(TEXT("And the boxes are untouched by the attempt"),
		Inventory->GetLooseBoxes(PalletProduct), FInventoryEntry::BoxesPerPallet);

	Inventory->AddEmptyPallets(1);
	TestTrue(TEXT("With one, it works"), Inventory->ClosePallet(PalletProduct, 1));
	TestEqual(TEXT("Used up again"), Inventory->GetEmptyPallets(), 0);

	// --- The count cannot go negative or be conjured ------------------------

	TestFalse(TEXT("Cannot take pallets you do not have"),
		Inventory->RemoveEmptyPallets(1));
	TestFalse(TEXT("Nor a negative number of them"),
		Inventory->RemoveEmptyPallets(-3));
	Inventory->AddEmptyPallets(-5);
	TestEqual(TEXT("Adding a negative count does nothing"),
		Inventory->GetEmptyPallets(), 0);

	// --- Empty pallets take no room ----------------------------------------
	// They stack flat against a wall. If they took Box Units, a warehouse full
	// of crates could refuse the pallet you need to unload the next delivery.

	const float BayUsedBefore = Inventory->GetClassUsedBU(EStorageClass::PalletBay);
	Inventory->AddEmptyPallets(20);
	TestEqual(TEXT("Twenty spares cost no Box Units"),
		Inventory->GetClassUsedBU(EStorageClass::PalletBay), BayUsedBefore, 0.01f);

	// --- A crate is bay stock that never comes apart ------------------------

	TestTrue(TEXT("A crate in the bay"), Inventory->AddStock(CrateProduct, 1, true));
	TestFalse(TEXT("A crate is not a pallet and does not open"),
		Inventory->OpenPallet(CrateProduct, 1));
	TestEqual(TEXT("Opening a crate produced no wood"),
		Inventory->GetEmptyPallets(), 20);

	// --- A new game puts the spares back ------------------------------------

	Inventory->ResetAll();
	TestEqual(TEXT("A new game starts with the same couple of spares"),
		Inventory->GetEmptyPallets(), Starting);

	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
