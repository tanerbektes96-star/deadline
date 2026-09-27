// Copyright DEADLINE. All Rights Reserved.
//
// Route cost (GDD 9.4). The trip is the only part of logistics the player never
// watches happen, so if the arithmetic drifts nobody notices until the money is
// wrong. These check the four things that actually move a quote: distance from
// the map itself, vehicle speed, load weight, and the refusals.
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "D:\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.Travel;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log

#include "CoreMinimal.h"
#include "Core/DeadlineSettings.h"
#include "Data/DestinationRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Fleet/FleetSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Travel/TravelSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineDestinationDataTest,
	"Deadline.Travel.DestinationData",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineDestinationDataTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = UDeadlineSettings::Get().DestinationTable.LoadSynchronous();
	if (!Table)
	{
		AddError(TEXT("DT_Destinations is not set in Project Settings > Deadline, or failed to load."));
		return false;
	}

	TestTrue(TEXT("There are places to go"), Table->GetRowMap().Num() >= 2);

	// Exactly one home, or the run has nowhere to start.
	int32 HomeCount = 0;
	for (const FName& ID : Table->GetRowNames())
	{
		const FDestinationRow* Row = Table->FindRow<FDestinationRow>(ID, TEXT("test"), false);
		if (!Row)
		{
			continue;
		}
		HomeCount += Row->IsHome ? 1 : 0;

		// Markers are normalised over the map image; anything outside it would
		// draw off-screen and price the route wrong at the same time.
		TestTrue(*FString::Printf(TEXT("%s MapX in range"), *ID.ToString()),
			Row->MapX >= 0.f && Row->MapX <= 1.f);
		TestTrue(*FString::Printf(TEXT("%s MapY in range"), *ID.ToString()),
			Row->MapY >= 0.f && Row->MapY <= 1.f);
		TestFalse(*FString::Printf(TEXT("%s has a name"), *ID.ToString()), Row->NameEN.IsEmpty());
	}
	TestEqual(TEXT("Exactly one home"), HomeCount, 1);

	return true;
}

// --- Quotes -------------------------------------------------------------------

namespace
{
	UGameInstance* MakeInstance()
	{
		UGameInstance* GI = NewObject<UGameInstance>(GEngine);
		if (GI)
		{
			GI->InitializeStandalone();
		}
		return GI;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineRouteCostTest,
	"Deadline.Travel.RouteCost",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineRouteCostTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = MakeInstance();
	UTravelSubsystem* Travel = GI ? GI->GetSubsystem<UTravelSubsystem>() : nullptr;
	UFleetSubsystem* Fleet = GI ? GI->GetSubsystem<UFleetSubsystem>() : nullptr;
	if (!Travel || !Fleet)
	{
		AddError(TEXT("Could not create a game instance with the travel and fleet subsystems."));
		return false;
	}

	const UDeadlineSettings& Settings = UDeadlineSettings::Get();

	// A run starts at whichever row calls itself home.
	const FName Home = Travel->GetCurrentLocation();
	TestFalse(TEXT("There is a starting place"), Home.IsNone());

	// Pick any other destination to price.
	FName Away = NAME_None;
	for (const FName& ID : Travel->GetDestinationIDs())
	{
		if (ID != Home)
		{
			Away = ID;
			break;
		}
	}
	if (Away.IsNone())
	{
		AddError(TEXT("DT_Destinations has only one row; nothing to travel to."));
		return false;
	}

	// --- Distance comes from the map, not from a road network --------------
	FDestinationRow From, To;
	Travel->GetDestination(Home, From);
	Travel->GetDestination(Away, To);
	const float Expected = FVector2D(To.MapX - From.MapX, To.MapY - From.MapY).Size()
		* Settings.CityScaleKm;
	TestEqual(TEXT("Distance is marker separation scaled"),
		Travel->GetDistanceKm(Home, Away), Expected, 0.01f);
	TestEqual(TEXT("Distance is symmetric"),
		Travel->GetDistanceKm(Away, Home), Expected, 0.01f);

	Fleet->RegisterVehicle(TEXT("Test_Truck"), TEXT("V01"));

	const FTravelQuote Empty = Travel->GetQuote(TEXT("Test_Truck"), Away);
	TestTrue(TEXT("An empty van can make the trip"), Empty.bPossible);
	TestEqual(TEXT("Quote distance matches"), Empty.DistanceKm, Expected, 0.01f);
	TestTrue(TEXT("It takes some time"), Empty.TravelMinutes > 0.f);
	TestTrue(TEXT("It burns some fuel"), Empty.FuelLitres > 0.f);
	TestEqual(TEXT("Fuel costs what fuel costs"),
		Empty.FuelCost, Empty.FuelLitres * Settings.FuelPricePerLitre, 0.01f);

	// --- A loaded truck burns more (GDD 9.4) -------------------------------
	// I02 Rebar Bundle is 900 kg in a 1200 kg van: three quarters loaded.
	TestTrue(TEXT("Load it"), Fleet->Load(TEXT("Test_Truck"), TEXT("I02"), 1, true, false));
	const FTravelQuote Laden = Travel->GetQuote(TEXT("Test_Truck"), Away);

	TestTrue(TEXT("Loaded is heavier than empty"), Laden.LoadFraction > 0.f);
	TestTrue(TEXT("Loaded burns more fuel"), Laden.FuelLitres > Empty.FuelLitres);
	TestEqual(TEXT("Exactly the GDD 9.4 penalty"),
		Laden.FuelLitres,
		Empty.FuelLitres * (1.f + Settings.LoadedFuelPenalty * Laden.LoadFraction),
		0.01f);
	// Weight changes the fuel, never the clock.
	TestEqual(TEXT("Same trip takes the same time"),
		Laden.TravelMinutes, Empty.TravelMinutes, 0.01f);

	// --- A faster vehicle arrives sooner -----------------------------------
	Fleet->RegisterVehicle(TEXT("Test_Pickup"), TEXT("V07"));   // 96 km/h vs 62
	const FTravelQuote Quick = Travel->GetQuote(TEXT("Test_Pickup"), Away);
	TestTrue(TEXT("The pickup is quicker"), Quick.TravelMinutes < Empty.TravelMinutes);

	// --- Refusals ----------------------------------------------------------
	const FTravelQuote Here = Travel->GetQuote(TEXT("Test_Truck"), Home);
	TestFalse(TEXT("You cannot travel to where you are"), Here.bPossible);
	TestFalse(TEXT("...and it says so"), Here.Reason.IsEmpty());

	Fleet->RegisterVehicle(TEXT("Test_Forklift"), TEXT("V08"));
	const FTravelQuote Yard = Travel->GetQuote(TEXT("Test_Forklift"), Away);
	TestFalse(TEXT("The forklift stays in the yard"), Yard.bPossible);

	const FTravelQuote Nowhere = Travel->GetQuote(TEXT("Test_Truck"), TEXT("DXX"));
	TestFalse(TEXT("An unknown place is refused"), Nowhere.bPossible);

	GI->Shutdown();
	return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS
