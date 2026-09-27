// Copyright DEADLINE. All Rights Reserved.
//
// The GDD 9.5 fleet table and the manifest behind the loading flow.
//
// What this is really guarding: a truck is the one place where Box Units and
// kilograms both bite, where two of the vehicles refuse whole container types,
// and where the white/grey split has to survive a box leaving the warehouse,
// riding somewhere and coming back off. Get any of those wrong and the failure
// shows up as money, not as a crash.
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "D:\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.Fleet;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log

#include "CoreMinimal.h"
#include "Core/DeadlineSettings.h"
#include "Data/VehicleRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Fleet/FleetSubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// --- The table itself ---------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineVehicleDataTest,
	"Deadline.Fleet.VehicleData",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineVehicleDataTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = UDeadlineSettings::Get().VehicleTable.LoadSynchronous();
	if (!Table)
	{
		AddError(TEXT("DT_Vehicles is not set in Project Settings > Deadline, or failed to load."));
		return false;
	}

	// GDD 9.5 lists eight vehicles, V01 to V08.
	TestEqual(TEXT("Eight vehicles"), Table->GetRowMap().Num(), 8);

	// The capacities are the GDD's own numbers and must not drift with tuning.
	const TMap<FName, float> ExpectedCapacity = {
		{TEXT("V01"), 32.f},  {TEXT("V02"), 64.f},  {TEXT("V03"), 96.f},
		{TEXT("V04"), 160.f}, {TEXT("V05"), 320.f}, {TEXT("V06"), 128.f},
		{TEXT("V07"), 32.f},
	};

	for (const TPair<FName, float>& Pair : ExpectedCapacity)
	{
		const FVehicleRow* Row = Table->FindRow<FVehicleRow>(Pair.Key, TEXT("test"), false);
		if (!Row)
		{
			AddError(FString::Printf(TEXT("DT_Vehicles has no row '%s'."), *Pair.Key.ToString()));
			continue;
		}
		TestEqual(*FString::Printf(TEXT("%s capacity"), *Pair.Key.ToString()),
			Row->CapacityBU, Pair.Value);
		TestTrue(*FString::Printf(TEXT("%s has a speed"), *Pair.Key.ToString()), Row->SpeedKmh > 0.f);
		TestTrue(*FString::Printf(TEXT("%s has an axle limit"), *Pair.Key.ToString()),
			Row->MaxWeightKg > 0.f);
	}

	// The two restrictions the GDD spells out.
	const FVehicleRow* Flatbed = Table->FindRow<FVehicleRow>(TEXT("V03"), TEXT("test"), false);
	const FVehicleRow* Reefer = Table->FindRow<FVehicleRow>(TEXT("V06"), TEXT("test"), false);
	const FVehicleRow* Forklift = Table->FindRow<FVehicleRow>(TEXT("V08"), TEXT("test"), false);
	if (Flatbed) { TestFalse(TEXT("V03 open bed refuses secure cases"), Flatbed->AllowsSecure); }
	if (Reefer)  { TestTrue(TEXT("V06 reefer is cold only"), Reefer->ColdOnly); }
	if (Forklift){ TestTrue(TEXT("V08 forklift never travels"), Forklift->WarehouseOnly); }

	return true;
}

// --- The manifest -------------------------------------------------------------

namespace
{
	UFleetSubsystem* MakeFleet(UGameInstance*& OutGI)
	{
		OutGI = NewObject<UGameInstance>(GEngine);
		if (!OutGI)
		{
			return nullptr;
		}
		OutGI->InitializeStandalone();
		return OutGI->GetSubsystem<UFleetSubsystem>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineCargoTest,
	"Deadline.Fleet.Cargo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineCargoTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = nullptr;
	UFleetSubsystem* Fleet = MakeFleet(GI);
	if (!Fleet)
	{
		AddError(TEXT("Could not create a game instance with a fleet subsystem."));
		return false;
	}

	FText Reason;

	// --- Box Units cap the load --------------------------------------------
	// V01 holds 32 BU. S01 is a Box L at 2.0 BU, so sixteen fill it.
	Fleet->RegisterVehicle(TEXT("Test_Van"), TEXT("V01"));
	TestEqual(TEXT("Van capacity"), Fleet->GetCapacityBU(TEXT("Test_Van")), 32.f);

	TestTrue(TEXT("Sixteen cases fit"), Fleet->Load(TEXT("Test_Van"), TEXT("S01"), 16, true, false));
	TestEqual(TEXT("Van is full"), Fleet->GetUsedBU(TEXT("Test_Van")), 32.f);
	TestFalse(TEXT("Seventeenth is refused"),
		Fleet->CanLoad(TEXT("Test_Van"), TEXT("S01"), 1, false, Reason));
	TestFalse(TEXT("...and says why"), Reason.IsEmpty());

	// --- Kilograms cap it too ----------------------------------------------
	// I02 Rebar Bundle is 16 BU but 900 kg, so a 1200 kg van runs out of axle
	// long before it runs out of room. This is the check that a heavy pallet
	// cannot ride on a small van no matter how empty it looks.
	Fleet->RegisterVehicle(TEXT("Test_Van2"), TEXT("V01"));
	TestTrue(TEXT("One rebar bundle fits"),
		Fleet->Load(TEXT("Test_Van2"), TEXT("I02"), 1, true, false));
	TestTrue(TEXT("Room left in Box Units"), Fleet->GetFreeBU(TEXT("Test_Van2")) >= 16.f);
	TestFalse(TEXT("But not in kilograms"),
		Fleet->CanLoad(TEXT("Test_Van2"), TEXT("I02"), 1, false, Reason));

	// --- The GDD 9.5 carry restrictions ------------------------------------
	Fleet->RegisterVehicle(TEXT("Test_Flatbed"), TEXT("V03"));
	TestFalse(TEXT("Open bed refuses a secure case"),
		Fleet->CanLoad(TEXT("Test_Flatbed"), TEXT("E08"), 1, false, Reason));
	TestTrue(TEXT("...but takes ordinary boxes"),
		Fleet->CanLoad(TEXT("Test_Flatbed"), TEXT("S01"), 1, false, Reason));

	Fleet->RegisterVehicle(TEXT("Test_Reefer"), TEXT("V06"));
	TestTrue(TEXT("Reefer takes a cold tote"),
		Fleet->CanLoad(TEXT("Test_Reefer"), TEXT("S08"), 1, false, Reason));
	TestFalse(TEXT("Reefer refuses a dry box"),
		Fleet->CanLoad(TEXT("Test_Reefer"), TEXT("S01"), 1, false, Reason));

	// --- White and grey survive the ride -----------------------------------
	Fleet->RegisterVehicle(TEXT("Test_Grey"), TEXT("V02"));
	TestTrue(TEXT("A grey box goes aboard"),
		Fleet->Load(TEXT("Test_Grey"), TEXT("S01"), 1, /*bRecorded=*/false, false));
	{
		const FVehicleCargo* Cargo = Fleet->GetAllCargo().Find(TEXT("Test_Grey"));
		const FInventoryEntry* Entry = Cargo ? Cargo->Entries.Find(TEXT("S01")) : nullptr;
		TestTrue(TEXT("It is aboard"), Entry && Entry->PhysicalStock == 1);
		TestTrue(TEXT("And the books never saw it"), Entry && Entry->RecordedStock == 0);
	}

	// --- Loose boxes ride one at a time (GDD 10.2) -------------------------
	Fleet->RegisterVehicle(TEXT("Test_Loose"), TEXT("V01"));
	TestTrue(TEXT("A single box off a pallet goes aboard"),
		Fleet->Load(TEXT("Test_Loose"), TEXT("I01"), 1, true, /*bLoose=*/true));
	TestEqual(TEXT("It is worth one Box Unit, not sixteen"),
		Fleet->GetUsedBU(TEXT("Test_Loose")), 1.f);

	bool bLoose = false;
	TestEqual(TEXT("Unloading finds it"),
		Fleet->PickUnloadProduct(TEXT("Test_Loose"), bLoose), FName(TEXT("I01")));
	TestTrue(TEXT("...and knows it is loose"), bLoose);

	TestTrue(TEXT("It comes back off"),
		Fleet->Unload(TEXT("Test_Loose"), TEXT("I01"), 1, true, true));
	TestTrue(TEXT("Van is empty again"), Fleet->IsEmpty(TEXT("Test_Loose")));

	// --- Fleet-wide totals, for the valuation ------------------------------
	TestEqual(TEXT("S01 across the fleet"),
		Fleet->GetTotalWholeContainers(TEXT("S01")), 17);   // 16 in the van, 1 grey

	// --- The roster keeps its load across a re-register --------------------
	Fleet->RegisterVehicle(TEXT("Test_Van"), TEXT("V01"));
	TestEqual(TEXT("Coming back to the level does not empty the van"),
		Fleet->GetUsedBU(TEXT("Test_Van")), 32.f);

	GI->Shutdown();
	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
