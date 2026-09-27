// Copyright DEADLINE. All Rights Reserved.
//
// The warehouse ledger. Inventory is DATA — the boxes you see in the world are
// a separate presentation layer (CLAUDE.md mistake #2).
//
// Two ledgers (GDD 7.1): PhysicalStock is what is really on the shelves,
// RecordedStock is what the official books say. Every mutating function takes
// bRecorded so the caller must always answer "is this white or grey?"
// (CLAUDE.md mistake #6). The grey *channel* itself does not open until
// Month 7; the ledger split exists from day one so nothing has to be
// retrofitted later.
//
// Quantities are counted in CONTAINERS, not individual units: one box of water
// is one container worth VolumeBU 2.0. BasePrice in DT_Products is per container.
//
// Month 3 — storage is no longer one pooled capacity number. Every product
// lives in the storage class its container type belongs to (Inventory/
// StorageClass.h, GDD 11), each class has its own capacity, and a class can be
// full while the warehouse as a whole has room. Nothing records *where* a box
// is, because the container type already decides it.
//
// Month 3 — a pallet can be opened into 16 boxes and closed again (GDD 10.2).
// The 16 boxes are the same goods in a different shape, so they stay on the
// same product row as PhysicalLooseBoxes and move out of the pallet bay into
// the racks. They are not separately sellable: a sale takes whole containers,
// gathering loose boxes back into one only when it has to.

#pragma once

#include "CoreMinimal.h"
#include "Inventory/StorageClass.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InventorySubsystem.generated.h"

class UProductCatalogSubsystem;

/**
 * One acquisition of one product, kept so the goods can have an AGE.
 *
 * Why the ledger is not just two numbers any more: spoilage and obsolescence
 * (GDD 5.1) both ask "how old is this?", and an aggregate count cannot answer.
 * The tempting shortcut -- one average age per product -- is wrong in a way
 * players find immediately: dropping one fresh crate onto ten old ones would
 * make the whole stack younger.
 *
 * So goods are held in batches, oldest first, and consumed oldest first. The
 * list stays short because same-day acquisitions merge into one batch.
 *
 * Batches track PHYSICAL goods only. The recorded ledger stays an aggregate:
 * paperwork does not age.
 */
USTRUCT(BlueprintType)
struct FStockBatch
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Containers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 LooseBoxes = 0;

	/** Clock reading (UTimeSubsystem total minutes) when these goods became
	    yours. Absolute, not a countdown, so a save/load or a travel jump needs
	    no catching up -- the age is recomputed from the clock (CLAUDE.md tick
	    rule 1, lazy evaluation). */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	double AcquiredAtMinute = 0.0;

	bool IsEmpty() const { return Containers <= 0 && LooseBoxes <= 0; }
};

USTRUCT(BlueprintType)
struct FInventoryEntry
{
	GENERATED_BODY()

	/** Boxes one pallet breaks into, and rebuilds from (GDD 10.2). */
	static constexpr int32 BoxesPerPallet = 16;

	/** Whole containers actually sitting in the warehouse. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 PhysicalStock = 0;

	/** Whole containers the official books know about. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 RecordedStock = 0;

	/** Boxes from opened pallets, physically on the racks. Only ever non-zero
	    for a Pallet product; 16 of them are worth one pallet. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 PhysicalLooseBoxes = 0;

	/** The same count, on the books. Opening a pallet moves both ledgers, so a
	    white pallet stays white once broken down and a grey one stays grey. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 RecordedLooseBoxes = 0;

	/** The physical goods again, split by when they arrived. Sorted oldest
	    first. Kept in step with PhysicalStock / PhysicalLooseBoxes by the
	    mutators below -- do not move those counters by hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TArray<FStockBatch> Batches;

	/** Holdings in container-equivalents: whole containers plus the fraction of
	    a pallet the loose boxes add up to. */
	float PhysicalContainers() const
	{
		return PhysicalStock + static_cast<float>(PhysicalLooseBoxes) / BoxesPerPallet;
	}

	float RecordedContainers() const
	{
		return RecordedStock + static_cast<float>(RecordedLooseBoxes) / BoxesPerPallet;
	}

	/** Positive = unexplained goods, negative = goods that vanished off the
	    books. Measured in container-equivalents so that breaking a grey pallet
	    down does not make the exposure disappear. */
	float Discrepancy() const { return PhysicalContainers() - RecordedContainers(); }

	bool IsEmpty() const
	{
		return PhysicalStock == 0 && RecordedStock == 0
			&& PhysicalLooseBoxes == 0 && RecordedLooseBoxes == 0;
	}

	// --- Ageing (the batch list) -------------------------------------------
	// Every one of these moves the counters as well. That is the point: there
	// is one way to change how much you hold, and it always says when.

	/** Take goods in, stamped with the moment they became yours. Same-day
	    arrivals merge, so a season of trading is still a handful of batches. */
	void AddBatch(int32 InContainers, int32 InLooseBoxes, double AcquiredAtMinute);

	/** Consume whole containers, oldest first. */
	void TakeOldestContainers(int32 Count);

	/** Consume loose boxes, oldest first. */
	void TakeOldestLooseBoxes(int32 Count);

	/** Break pallets into boxes without the goods getting any younger
	    (GDD 10.2): the boxes keep the batch their pallet came from. */
	void ConvertOldestToLoose(int32 Pallets);

	/** And back again. */
	void ConvertOldestToWhole(int32 Pallets);

	/** Clock reading of the oldest whole container, or Fallback if none. */
	double OldestContainerMinute(double Fallback) const;

	/** Clock reading of the oldest loose box, or Fallback if none. */
	double OldestLooseBoxMinute(double Fallback) const;

	/** Age of everything held, weighted by container-equivalents. */
	double AverageAgeDays(double NowMinute) const;

	/**
	 * Throw away everything past its shelf life.
	 * @return true if anything was lost; the counts come back in the out params.
	 */
	bool ExpireSpoiled(int32 ShelfLifeDays, double NowMinute,
		int32& OutContainers, int32& OutLooseBoxes);

	/**
	 * Make the batch list agree with the counters, stamping any goods it does
	 * not know about with NowMinute.
	 *
	 * This exists for one job: a save written before batches existed (v4 and
	 * earlier) has counts and no ages, and its goods have to start ageing from
	 * the moment they are loaded rather than spoiling on sight.
	 */
	void SyncBatches(double NowMinute);

private:
	void DropEmptyBatches();
};

/** One physical container sitting in a storage class, in a stable order. The
    rack actors turn this list into Instanced Static Mesh entries. */
USTRUCT(BlueprintType)
struct FStoredContainer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FName ProductID;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EContainerType ContainerType = EContainerType::BoxM;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	float VolumeBU = 0.f;

	/** True if this is a box off an opened pallet, not a whole container. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bLooseBox = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStockChanged, FName, ProductID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCapacityChanged);

/**
 * Goods rotted on the shelf (GDD 5.1 ShelfLifeDays).
 *
 * @param Containers          whole containers lost, loose boxes folded in as
 *                            sixteenths and rounded up.
 * @param bWasOnTheBooks      true if the write-off came off the recorded
 *                            ledger too. Grey goods rot without paperwork.
 *
 * The economy listens to this and books the loss. Inventory does not reach
 * into the economy itself: the warehouse knows what it lost, not what it was
 * worth, and the dependency only points one way.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStockSpoiled,
	FName, ProductID, int32, Containers, bool, bWasOnTheBooks);

UCLASS()
class DEADLINE__API UInventorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- Events (CLAUDE.md: the UI subscribes, nothing polls) ---------------

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Inventory")
	FOnStockChanged OnStockChanged;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Inventory")
	FOnCapacityChanged OnCapacityChanged;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Inventory")
	FOnStockSpoiled OnStockSpoiled;

	// --- Mutations ---------------------------------------------------------

	/**
	 * Put containers into the warehouse.
	 * @param bRecorded  true = invoiced (both ledgers move), false = grey (physical only).
	 * @return false if the product's storage class has no room.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Inventory")
	bool AddStock(FName ProductID, int32 Containers, bool bRecorded);

	/**
	 * Put containers back that were already yours, keeping their age.
	 *
	 * Picking a box off the rack and setting it down again must not make it
	 * fresh, or every perishable in the game has an infinite shelf life and
	 * the player only has to jiggle it. AcquiredAtMinute comes from the box
	 * itself (AContainerActor::AcquiredAtMinute).
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Inventory")
	bool AddStockAged(FName ProductID, int32 Containers, bool bRecorded, double AcquiredAtMinute);

	/**
	 * Take containers out of the warehouse.
	 *
	 * Whole containers go first; when they run out, loose boxes are gathered
	 * back into containers to cover the rest, so goods broken down for carrying
	 * are never stranded outside the market.
	 *
	 * @param bRecorded  true = invoiced sale (both ledgers move), false = grey (physical only).
	 * @return false if there is not enough physical stock.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Inventory")
	bool RemoveStock(FName ProductID, int32 Containers, bool bRecorded);

	/**
	 * Break pallets into loose boxes (GDD 10.2). The goods do not change hands,
	 * so both ledgers convert together and the white/grey split is preserved.
	 *
	 * The wooden pallet does not evaporate: it stays behind as an empty one
	 * (GDD 5.6 "Ahşap palet"), which is what makes the loop physically honest.
	 * A person cannot lift a loaded pallet, but they can carry an empty one,
	 * and they can walk goods onto it a box at a time.
	 *
	 * @return false without enough pallets, or without rack space for the boxes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Inventory")
	bool OpenPallet(FName ProductID, int32 Pallets = 1);

	/** Gather 16 loose boxes back onto a pallet. Needs a spare empty pallet to
	    stack them on, and room in the bay. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Inventory")
	bool ClosePallet(FName ProductID, int32 Pallets = 1);

	// --- Empty pallets (GDD 5.6) --------------------------------------------
	//
	// Not stock: an empty pallet has no market price, no shelf life and nothing
	// an inspection would care about, so it is one number rather than two
	// ledgers. It is equipment that happens to be consumable.
	//
	// It takes no Box Units either. Empty pallets stack flat against a wall,
	// and giving them a volume would only ever produce a capacity refusal the
	// player cannot act on.

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	int32 GetEmptyPallets() const { return EmptyPallets; }

	/** Take empty pallets in. Negative counts are refused, not silently eaten. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Storage")
	void AddEmptyPallets(int32 Count);

	/** @return false if there are not that many to take. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Storage")
	bool RemoveEmptyPallets(int32 Count);

	/**
	 * Put single boxes off an opened pallet onto the racks. This is how a box
	 * carried in your hands gets back into the ledger without rebuilding the
	 * whole pallet first.
	 * @return false unless the product ships on pallets and the racks have room.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Inventory")
	bool AddLooseBoxes(FName ProductID, int32 Boxes, bool bRecorded);

	/** The same, for boxes that are coming home rather than arriving new. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Inventory")
	bool AddLooseBoxesAged(FName ProductID, int32 Boxes, bool bRecorded, double AcquiredAtMinute);

	/** Take single boxes off the racks, for carrying. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Inventory")
	bool RemoveLooseBoxes(FName ProductID, int32 Boxes, bool bRecorded);

	/** Rack space for that many loose boxes of this product. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	bool HasSpaceForLooseBoxes(FName ProductID, int32 Boxes) const;

	/** Wipe the ledger and put storage back to its starting size. */
	void ResetAll();

	// --- Queries -----------------------------------------------------------

	/** Whole containers on the shelves. Loose boxes are not counted here. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	int32 GetPhysicalStock(FName ProductID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	int32 GetRecordedStock(FName ProductID) const;

	/** Boxes sitting on the racks from an opened pallet. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	int32 GetLooseBoxes(FName ProductID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	int32 GetRecordedLooseBoxes(FName ProductID) const;

	/** Whole containers plus the pallet-fraction the loose boxes add up to. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	float GetPhysicalContainers(FName ProductID) const;

	/** Physical minus recorded. Exactly what an inspection looks at (GDD 7.1). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	float GetDiscrepancy(FName ProductID) const;

	/** Sum of absolute discrepancy across every product. Total exposure. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	float GetTotalDiscrepancy() const;

	// --- Condition: spoilage and obsolescence (GDD 5.1) ---------------------
	// Nothing ticks. Ages are read off the clock when somebody asks, and the
	// spoilage sweep runs on the day rollover -- shelf life is measured in
	// whole days, so nothing can change in between (CLAUDE.md tick rule 1).

	/**
	 * Throw out everything past its shelf life, right now.
	 *
	 * Called on the day rollover and before any query that would otherwise
	 * report goods that have already rotted. Cheap to call twice in a day: it
	 * returns immediately once the day it last swept matches the clock.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Condition")
	void RefreshCondition();

	/**
	 * What the goods you hold are worth as a fraction of the market price,
	 * after obsolescence. 1.0 = as good as new.
	 *
	 * This is a property of YOUR stock, not of the market: the quoted price is
	 * a pure function of seed, product and day (GDD 8.1) and stays that way.
	 * Two players holding the same product on the same day get the same quote
	 * and different money, because one of them bought earlier.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Condition")
	float GetConditionMultiplier(FName ProductID) const;

	/** Weighted average age of the stock on hand, in days. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Condition")
	float GetAverageAgeDays(FName ProductID) const;

	/** Whole days before the oldest goods on hand rot. -1 = never spoils. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Condition")
	int32 GetDaysUntilSpoilage(FName ProductID) const;

	/** Clock reading of the oldest goods on hand, for a box being picked up. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Condition")
	double GetOldestAcquiredMinute(FName ProductID, bool bLoose) const;

	/** Products that will spoil within DaysAhead, soonest first. The HUD warns
	    about these; without it, spoilage is a silent tax. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Condition")
	TArray<FName> GetSpoilageWarnings(int32 DaysAhead = 3) const;

	/** Dev cheat: push everything on the shelves back by this many days. */
	void AgeAllStockByDays(float Days);

	// --- Storage classes (GDD 11) ------------------------------------------

	/** Where this product's containers live. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	EStorageClass GetStorageClassFor(FName ProductID) const;

	/** How many racks / bays / zones / cages the warehouse has. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	int32 GetUnitCount(EStorageClass Class) const;

	/** Gain or lose storage units. This is what a warehouse upgrade moves. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Storage")
	void SetUnitCount(EStorageClass Class, int32 Units);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	float GetClassCapacityBU(EStorageClass Class) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	float GetClassUsedBU(EStorageClass Class) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	float GetClassFreeBU(EStorageClass Class) const;

	/** Every container in a class, in a stable order, for the rack visuals. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Storage")
	TArray<FStoredContainer> GetClassContents(EStorageClass Class) const;

	// --- Whole-warehouse totals (the HUD summary line) ----------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	float GetUsedBU() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	float GetCapacityBU() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	float GetFreeBU() const { return FMath::Max(0.f, GetCapacityBU() - GetUsedBU()); }

	/** Room in the class this product belongs to — not in the warehouse as a
	    whole. A full cold zone blocks frozen goods while the racks stand empty. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	bool HasSpaceFor(FName ProductID, int32 Containers) const;

	/** Product IDs currently holding any physical or recorded stock. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	TArray<FName> GetStockedProductIDs() const;

	/** Stocked product IDs whose containers belong in this class. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Inventory")
	TArray<FName> GetStockedProductIDsInClass(EStorageClass Class) const;

	// --- Save / load --------------------------------------------------------

	const TMap<FName, FInventoryEntry>& GetAllEntries() const { return Entries; }
	void RestoreEntry(FName ProductID, const FInventoryEntry& Entry);
	void RestoreEmptyPallets(int32 Count) { EmptyPallets = FMath::Max(0, Count); }

private:
	UProductCatalogSubsystem* GetCatalogue() const;

	/** Clock reading, or 0 if there is no clock (tests without a game instance). */
	double NowMinute() const;

	/** Shelf life in days for a product, 0 if it does not spoil. */
	int32 ShelfLifeOf(FName ProductID) const;

	/** Run the spoilage sweep before answering a query. Safe to call often:
	    it does nothing once the current day has already been swept. */
	void EnsureSwept() const;

	UFUNCTION()
	void HandleDayChanged(int32 NewDay);

	/** Container volume for the product, 0 if unknown. */
	float VolumeBUOf(FName ProductID) const;

	/** BU one loose box of this product takes: a sixteenth of its pallet. */
	float LooseBoxVolumeBUOf(FName ProductID) const;

	/** BU an entry occupies, whole containers and loose boxes together. */
	float EntryVolumeBU(FName ProductID, const FInventoryEntry& Entry) const;

	/** Fill UnitCounts from the project settings. */
	void ResetUnitCounts();

	UPROPERTY()
	TMap<FName, FInventoryEntry> Entries;

	/** Units per storage class, indexed by EStorageClass. */
	UPROPERTY()
	TArray<int32> UnitCounts;

	/** Spare wooden pallets on hand (GDD 5.6). */
	UPROPERTY()
	int32 EmptyPallets = 0;

	/** Game day the spoilage sweep last ran on. Shelf life is whole days, so
	    sweeping again inside the same day cannot find anything new. */
	int32 LastSweptDay = -1;
};
