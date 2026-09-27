// Copyright DEADLINE. All Rights Reserved.
//
// Project-wide tuning knobs, editable in Project Settings > Game > Deadline
// and stored in Config/DefaultGame.ini. Nothing here is hardcoded in gameplay
// code (CLAUDE.md: balance numbers live in data, not in C++).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DeveloperSettings.h"
#include "DeadlineSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Deadline"))
class DEADLINE__API UDeadlineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** DT_Products — the product catalogue (row type FProductRow). */
	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> ProductTable;

	/** DT_Vehicles — the fleet table (row type FVehicleRow, GDD 9.5). */
	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> VehicleTable;

	/** DT_Destinations — the travel points (row type FDestinationRow, GDD 9.2). */
	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> DestinationTable;

	/** DT_Events — world event templates (row type FEventRow, GDD 13). */
	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (AllowedClasses = "/Script/Engine.DataTable"))
	TSoftObjectPtr<UDataTable> EventTable;

	/** WBP_MarketScreen. Named here rather than on the pawn so the Designer
	    never has to wire it up: rebuild the Blueprint, keep the name, done. */
	UPROPERTY(Config, EditAnywhere, Category = "Data")
	TSoftClassPtr<UUserWidget> MarketScreenWidget;

	/** WBP_TravelScreen — the map (GDD 9.2 / 17). Same arrangement as above. */
	UPROPERTY(Config, EditAnywhere, Category = "Data")
	TSoftClassPtr<UUserWidget> TravelScreenWidget;

	/** WBP_ForecastScreen — the forecast board (GDD 17), on N. */
	UPROPERTY(Config, EditAnywhere, Category = "Data")
	TSoftClassPtr<UUserWidget> ForecastScreenWidget;

	// --- Language ---------------------------------------------------------

	/** The game's language: "tr" or "en". Turkish by default. Separate from
	    the engine culture so PIE plays in Turkish whatever language the editor
	    runs in. Overridden per run with -DeadlineLang=en. */
	UPROPERTY(Config, EditAnywhere, Category = "Language")
	FString GameLanguage = TEXT("tr");

	// --- Starting funds (GDD 8.3) -----------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	float StartingCash = 5000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	float StartingBank = 45000.f;

	// --- Warehouse (GDD 11) -------------------------------------------------
	// Capacity is not one pooled number: each storage class has its own, and a
	// full cold zone does not become roomier because the racks are empty.
	// The defaults add up to the 240 BU the warehouse started with in Month 1:
	// 8x12 + 4x16 + 2x24 + 4x8.

	/** Racks, 12 BU each. Holds Box S / M / L. */
	UPROPERTY(Config, EditAnywhere, Category = "Warehouse", meta = (ClampMin = "0"))
	int32 StartingRacks = 8;

	/** Pallet bays, 16 BU each. Holds Pallets and Crates. */
	UPROPERTY(Config, EditAnywhere, Category = "Warehouse", meta = (ClampMin = "0"))
	int32 StartingPalletBays = 4;

	/** Cold zones, 24 BU each. Holds Cold Totes. */
	UPROPERTY(Config, EditAnywhere, Category = "Warehouse", meta = (ClampMin = "0"))
	int32 StartingColdZones = 2;

	/** Caged racks, 8 BU each. Holds Secure Cases. */
	UPROPERTY(Config, EditAnywhere, Category = "Warehouse", meta = (ClampMin = "0"))
	int32 StartingSecureCages = 4;

	/** Spare wooden pallets in the bay at the start (GDD 5.6).
	    Opening a pallet leaves one behind and closing one uses it up, so the
	    count balances itself; these are here so the very first pallet you
	    assemble out of loose boxes has something to sit on. */
	UPROPERTY(Config, EditAnywhere, Category = "Warehouse", meta = (ClampMin = "0"))
	int32 StartingEmptyPallets = 2;

	/** What fits on the forks, in Box Units. One pallet's worth, which is what
	    the jack is built around and what DT_Vehicles gives the V08 forklift.
	    This is the whole point of the tool: by hand you move one box, on the
	    forks you build a pallet and make one trip instead of sixteen. */
	UPROPERTY(Config, EditAnywhere, Category = "Warehouse", meta = (ClampMin = "1"))
	float PalletJackCapacityBU = 16.f;

	/** How much a loaded pallet jack slows you down (GDD 5.2: pallets and
	    crates move on a jack, never in your arms). Slow enough that walking a
	    pallet across the warehouse is a decision, not a reflex. */
	UPROPERTY(Config, EditAnywhere, Category = "Warehouse", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float PalletJackSpeedMultiplier = 0.55f;

	// --- Travel (GDD 9.4) ---------------------------------------------------
	// Nobody drives, so the "city" is a picture with markers on it. One number
	// turns marker separation into kilometres; there is no road network to keep
	// consistent with anything.

	/** How many km the travel map spans corner to corner, near enough.
	    This one number sets what a delivery costs. At 45 km a typical run is
	    20-35 minutes each way, so three or four of them fill a working morning
	    and "is this trip worth it?" is a real question. Fuel stays small on
	    purpose -- a van burning 0.11 L/km is a few dollars a trip; the cost of
	    a delivery is the clock, not the pump (GDD 9.4 lists time first). */
	UPROPERTY(Config, EditAnywhere, Category = "Travel", meta = (ClampMin = "1"))
	float CityScaleKm = 45.f;

	UPROPERTY(Config, EditAnywhere, Category = "Travel", meta = (ClampMin = "0"))
	float FuelPricePerLitre = 1.85f;

	/** Extra fuel a fully loaded truck burns. GDD 9.4 says up to 10%. */
	UPROPERTY(Config, EditAnywhere, Category = "Travel", meta = (ClampMin = "0"))
	float LoadedFuelPenalty = 0.10f;

	/** Travel time between 07-09 and 17-19 (GDD 9.4). */
	UPROPERTY(Config, EditAnywhere, Category = "Travel", meta = (ClampMin = "1"))
	float RushHourMultiplier = 1.35f;

	/** The cab transition, in real seconds. GDD 9.2 asks for 2-3. */
	UPROPERTY(Config, EditAnywhere, Category = "Travel", meta = (ClampMin = "0.1"))
	float TravelTransitionSeconds = 2.5f;

	// --- Condition over time (GDD 5.1) --------------------------------------

	/** The value goods never fall below, however old, as a fraction of the
	    market price. Old stock is cheap, not free: somebody always takes a
	    two-year-old monitor at the right number. Without a floor, holding a
	    fast-obsoleting product long enough would make it literally worthless
	    and the only sane play would be to dump everything on day one. */
	UPROPERTY(Config, EditAnywhere, Category = "Condition", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ObsolescenceFloor = 0.25f;

	// --- Trading frictions -------------------------------------------------
	// Mirrors EconomyPrototype/agent.py so the C++ loop matches the prototype.

	/** Mark-up paid over market price when buying at a counter (GDD 6.1). */
	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	float BuyPremium = 0.06f;

	/** Discount buyers take off market price when you sell (GDD 6.1). */
	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	float SellDiscount = 0.08f;

	/** Logistics + labour + tax charged on each side of a trade (GDD 8.2). */
	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	float HandlingCost = 0.05f;

	// --- Event calendar (GDD 13) -------------------------------------------
	// First-pass numbers, meant to be tuned by playing. See UEventSubsystem.

	/** Chance, each day, that a new signal appears (a real event on its way
	    or a false rumour). 0.4 = one every two and a half days on average. */
	UPROPERTY(Config, EditAnywhere, Category = "Events", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EventDailyChance = 0.4f;

	/** Most signals and events in flight at once, rumours included. */
	UPROPERTY(Config, EditAnywhere, Category = "Events", meta = (ClampMin = "1"))
	int32 MaxConcurrentEvents = 3;

	/** Days between the signal and the start, inclusive range. The warning
	    is the whole point: without it there is nothing to forecast. */
	UPROPERTY(Config, EditAnywhere, Category = "Events", meta = (ClampMin = "1"))
	int32 SignalLeadDaysMin = 1;

	UPROPERTY(Config, EditAnywhere, Category = "Events", meta = (ClampMin = "1"))
	int32 SignalLeadDaysMax = 3;

	/** The confidence a signal is shown with is rolled in this range, and the
	    signal then comes true with exactly that probability, so 70% means 70%.
	    The mean (0.75 here) sets the share of true signals; 1 - mean are
	    false rumours. */
	UPROPERTY(Config, EditAnywhere, Category = "Events", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SignalConfidenceMin = 0.55f;

	UPROPERTY(Config, EditAnywhere, Category = "Events", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SignalConfidenceMax = 0.95f;

	/** How much of the gap to an event's target price closes each day while
	    it runs. Mean reversion is suspended for affected products meanwhile
	    and takes them back down once it ends. */
	UPROPERTY(Config, EditAnywhere, Category = "Events", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float EventApproachRate = 0.45f;

	/** Cap on stacked event targets for one product, as a fraction of base.
	    The risk band ceiling still applies on top. */
	UPROPERTY(Config, EditAnywhere, Category = "Events", meta = (ClampMin = "0.0"))
	float MaxEventTarget = 3.f;

	// --- Forecast commitments (GDD 4 step 2) --------------------------------

	/** Longest hold after the expected day before a commitment is judged. */
	UPROPERTY(Config, EditAnywhere, Category = "Forecast", meta = (ClampMin = "1"))
	int32 CommitHoldDaysMax = 5;

	/** What the board's duration control starts on. */
	UPROPERTY(Config, EditAnywhere, Category = "Forecast", meta = (ClampMin = "1"))
	int32 CommitHoldDaysDefault = 2;

	// --- Player (GDD 3.2) ---------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Player", meta = (ClampMin = "70", ClampMax = "100"))
	float DefaultFOV = 80.f;

	/** cm/s. GDD lists 3.2 m/s. */
	UPROPERTY(Config, EditAnywhere, Category = "Player")
	float WalkSpeed = 320.f;

	/** cm/s. GDD lists 5.0 m/s. Disabled while carrying a container. */
	UPROPERTY(Config, EditAnywhere, Category = "Player")
	float SprintSpeed = 500.f;

	/** How far the interaction trace reaches, in cm (GDD 3.2). */
	UPROPERTY(Config, EditAnywhere, Category = "Player")
	float InteractionDistance = 200.f;

	static const UDeadlineSettings& Get()
	{
		return *GetDefault<UDeadlineSettings>();
	}
};
