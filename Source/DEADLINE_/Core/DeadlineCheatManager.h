// Copyright DEADLINE. All Rights Reserved.
//
// Dev cheats. CLAUDE.md requires at least one manual trigger per system, so
// every subsystem added from here on gets a console command here too.
//
// Open the console with ~ in a development build and type e.g.
//   Dl_AddCash 10000
//   Dl_SpawnBox E08
//   Dl_AdvanceTime 480
//   Dl_Inventory
// CheatManager is compiled out of shipping builds.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "DeadlineCheatManager.generated.h"

UCLASS()
class DEADLINE__API UDeadlineCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	// --- Money -------------------------------------------------------------

	UFUNCTION(exec)
	void Dl_AddCash(float Amount = 10000.f);

	UFUNCTION(exec)
	void Dl_AddBank(float Amount = 10000.f);

	// --- Time --------------------------------------------------------------

	/** Jump the clock forward. 1440 minutes = one day. */
	UFUNCTION(exec)
	void Dl_AdvanceTime(int32 Minutes = 60);

	/** 0 paused, 1 normal, 2 fast, 4 fastest (GDD 16). */
	UFUNCTION(exec)
	void Dl_SetSpeed(int32 Speed = 1);

	// --- Boxes and stock ---------------------------------------------------

	/** Spawn a container of ProductID in front of the player. */
	UFUNCTION(exec)
	void Dl_SpawnBox(FName ProductID = TEXT("S01"), bool bRecorded = true);

	/** Put containers straight into the warehouse ledger. */
	UFUNCTION(exec)
	void Dl_AddStock(FName ProductID = TEXT("S01"), int32 Containers = 1, bool bRecorded = true);

	/** Resize one storage class (GDD 11). Class: Rack / PalletBay / ColdZone /
	    SecureRack. This is what a warehouse upgrade will do. */
	UFUNCTION(exec)
	void Dl_SetZone(FName Class = TEXT("Rack"), int32 Units = 8);

	/** Print every storage class with its units, used and free Box Units. */
	UFUNCTION(exec)
	void Dl_Storage();

	// --- Condition over time (GDD 5.1) --------------------------------------

	/** Print every stocked product's age, days left before it spoils and what
	    obsolescence has taken off its value. */
	UFUNCTION(exec)
	void Dl_Condition();

	/**
	 * Age everything on the shelves and in the trucks by this many days
	 * without moving the clock, then sweep.
	 *
	 * Waiting out a 180-day shelf life in real time is three days of play, so
	 * there is no other way to test spoilage at all. Negative values make
	 * stock younger, which is how you undo a test.
	 */
	UFUNCTION(exec)
	void Dl_AgeStock(float Days = 1.f);

	// --- Pallets (GDD 10.2 / 5.6) ------------------------------------------

	/** Add spare wooden pallets, or print the count with 0. */
	UFUNCTION(exec)
	void Dl_Pallets(int32 Count = 0);

	/** Teleport to the pallet jack, or bring it to you if it is far away. */
	UFUNCTION(exec)
	void Dl_Jack();

	/** Break pallets into 16 boxes each, straight in the ledger. */
	UFUNCTION(exec)
	void Dl_OpenPallet(FName ProductID = TEXT("I01"), int32 Pallets = 1);

	/** Gather 16 loose boxes back onto a pallet. */
	UFUNCTION(exec)
	void Dl_ClosePallet(FName ProductID = TEXT("I01"), int32 Pallets = 1);

	// --- Fleet (GDD 9.5 / 10.1) --------------------------------------------

	/** Print every truck: model, load, weight, and what is aboard. */
	UFUNCTION(exec)
	void Dl_Fleet();

	/** Put goods straight into a truck's manifest, skipping the walk.
	    VehicleKey blank = the first truck in the fleet. */
	UFUNCTION(exec)
	void Dl_LoadCargo(FName ProductID = TEXT("S01"), int32 Count = 1,
		FName VehicleKey = NAME_None, bool bRecorded = true);

	/** Empty a truck back out. VehicleKey blank = the first truck. */
	UFUNCTION(exec)
	void Dl_EmptyVehicle(FName VehicleKey = NAME_None);

	/** List DT_Vehicles: capacity, speed, fuel and carry restrictions. */
	UFUNCTION(exec)
	void Dl_Vehicles();

	// --- Travel (GDD 9.2 / 9.4) --------------------------------------------

	/** List the travel points with distance, time and fuel from where you are. */
	UFUNCTION(exec)
	void Dl_Destinations();

	/** Drive to a destination. VehicleKey blank = the first truck. */
	UFUNCTION(exec)
	void Dl_Travel(FName DestinationID = TEXT("D04"), FName VehicleKey = NAME_None);

	/** Open the travel map, for when the screen exists but nothing is bound. */
	UFUNCTION(exec)
	void Dl_OpenMap();

	// --- Navigation --------------------------------------------------------

	/** Stand in front of an interaction point, already facing it.
	    Where: Supplier / Buyer / Storage / Vehicle, or a storage class by name
	    (Rack / PalletBay / ColdZone / SecureRack). Makes the gate test
	    repeatable without walking the greybox every time. */
	UFUNCTION(exec)
	void Dl_Goto(FName Where = TEXT("Supplier"));

	// --- Market ------------------------------------------------------------

	/** Today's quote for one product, or the whole catalogue if left blank. */
	UFUNCTION(exec)
	void Dl_Market(FName ProductID = NAME_None);

	/** Print a product's price series from day 0 to Days, without playing
	    them. The numbers this prints are exactly what the same run seed will
	    actually produce on those days. */
	UFUNCTION(exec)
	void Dl_MarketDump(FName ProductID = TEXT("S01"), int32 Days = 30);

	/** Write every product's series to Saved/MarketDump_<seed>.csv, for
	    diffing against EconomyPrototype by hand. */
	UFUNCTION(exec)
	void Dl_MarketDumpAll(int32 Days = 200);

	/** Open or close the market screen, for when IA_Market is not bound yet. */
	UFUNCTION(exec)
	void Dl_OpenMarket();

	// --- Events (GDD 13) ---------------------------------------------------

	/** Today's signals and active events, then the calendar for the next
	    Days days WITH the answers (which signals come true). It gives the
	    forecast away -- that is what a cheat is for. */
	UFUNCTION(exec)
	void Dl_Events(int32 Days = 10);

	/** Start an event now, with no signal. Impact < 0 and Duration < 1 roll
	    from DT_Events. Not saved. E.g. Dl_ForceEvent E12 1.5 5 */
	UFUNCTION(exec)
	void Dl_ForceEvent(FName EventID = TEXT("E02"), float Impact = -1.f, int32 Duration = -1);

	/** The news bulletins already out over the last Days days, newest first
	    (GDD 16: 08:00 bulletin, 12:00 update). Only what the player could
	    have heard -- no answers. */
	UFUNCTION(exec)
	void Dl_News(int32 Days = 2);

	/** The forecast board as the player sees it: signals, odds, exposure. */
	UFUNCTION(exec)
	void Dl_Forecast();

	/** Open the forecast board, for when the screen exists but N is not bound. */
	UFUNCTION(exec)
	void Dl_OpenForecast();

	/** Commit on today's signal: Dl_Commit E19 F03 4 2. No event picks the
	    first price signal on the board, no product its first product. */
	UFUNCTION(exec)
	void Dl_Commit(FName EventID = NAME_None, FName ProductID = NAME_None, int32 Containers = 1, int32 HoldDays = 2);

	/** Every commitment this run and the funds locked for them. */
	UFUNCTION(exec)
	void Dl_Commitments();

	/** Show the result card for a judged commitment; 0 = the latest. */
	UFUNCTION(exec)
	void Dl_ShowResult(int32 CommitmentID = 0);

	// --- Balance data ------------------------------------------------------

	/** Re-read Content/Deadline/Data/DT_Products.csv into the running game.
	    Edit the CSV, run this, see the new numbers without restarting.
	    In-memory only -- re-import the asset in the editor to keep it. */
	UFUNCTION(exec)
	void Dl_ReloadProducts();

	// --- Reporting ---------------------------------------------------------

	/** Dump funds, capacity and both ledgers to the log and the screen. */
	UFUNCTION(exec)
	void Dl_Inventory();

	/** List every product ID in DT_Products, with price and volume. */
	UFUNCTION(exec)
	void Dl_Products(FName LaunchPhase = NAME_None);

	// --- Save --------------------------------------------------------------

	UFUNCTION(exec)
	void Dl_Save();

	UFUNCTION(exec)
	void Dl_Load();

	UFUNCTION(exec)
	void Dl_NewGame(int32 Seed = 0);

private:
	void Report(const FString& Message, bool bWarning = false) const;

	/**
	 * Print several lines as one block, top line first.
	 *
	 * Report() on its own reads upside down for anything longer than a line:
	 * the engine's on-screen message stack draws the newest message at the
	 * top, so a table printed in order arrives with its header underneath it.
	 * This emits backwards so it lands the right way up, and logs in the
	 * order it was written.
	 */
	void ReportBlock(const TArray<FString>& Lines, bool bWarning = false) const;

	/** The condition table, as lines. Shared so Dl_AgeStock can print what it
	    did and the resulting table as one block rather than two. */
	void BuildConditionReport(TArray<FString>& OutLines) const;
};
