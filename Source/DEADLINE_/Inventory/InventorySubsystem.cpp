// Copyright DEADLINE. All Rights Reserved.

#include "Inventory/InventorySubsystem.h"

#include "Core/DeadlineSettings.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "Inventory/Condition.h"

namespace
{
	/** Boxes off an opened pallet are Box M sized, so they go on the racks
	    rather than back into the bay they came from (GDD 10.2 / 11). */
	constexpr EStorageClass LooseBoxClass = EStorageClass::Rack;
}

// --- FInventoryEntry: the batch list -----------------------------------------
// Batches are held oldest first. Everything below relies on that, and every
// insertion keeps it true.

void FInventoryEntry::DropEmptyBatches()
{
	Batches.RemoveAll([](const FStockBatch& Batch) { return Batch.IsEmpty(); });
}

void FInventoryEntry::AddBatch(int32 InContainers, int32 InLooseBoxes, double AcquiredAtMinute)
{
	InContainers = FMath::Max(0, InContainers);
	InLooseBoxes = FMath::Max(0, InLooseBoxes);
	if (InContainers == 0 && InLooseBoxes == 0)
	{
		return;
	}

	PhysicalStock += InContainers;
	PhysicalLooseBoxes += InLooseBoxes;

	// Merge with a batch acquired the same game day. Without this a player who
	// buys ten times a day grows a batch list all day; with it the list holds
	// at most one entry per day the goods were touched.
	const int32 Day = FMath::FloorToInt(AcquiredAtMinute / DeadlineCondition::MinutesPerDay);
	for (FStockBatch& Batch : Batches)
	{
		const int32 BatchDay = FMath::FloorToInt(Batch.AcquiredAtMinute / DeadlineCondition::MinutesPerDay);
		if (BatchDay == Day)
		{
			Batch.Containers += InContainers;
			Batch.LooseBoxes += InLooseBoxes;
			// Keep the older stamp: merging must never make goods younger.
			Batch.AcquiredAtMinute = FMath::Min(Batch.AcquiredAtMinute, AcquiredAtMinute);
			return;
		}
	}

	FStockBatch New;
	New.Containers = InContainers;
	New.LooseBoxes = InLooseBoxes;
	New.AcquiredAtMinute = AcquiredAtMinute;
	Batches.Add(New);
	Batches.Sort([](const FStockBatch& A, const FStockBatch& B)
	{
		return A.AcquiredAtMinute < B.AcquiredAtMinute;
	});
}

void FInventoryEntry::TakeOldestContainers(int32 Count)
{
	Count = FMath::Min(FMath::Max(0, Count), PhysicalStock);
	PhysicalStock -= Count;

	// Oldest out first. Selling the freshest stock while old crates rot behind
	// it would make shelf life a non-event.
	for (FStockBatch& Batch : Batches)
	{
		if (Count <= 0)
		{
			break;
		}
		const int32 Taken = FMath::Min(Batch.Containers, Count);
		Batch.Containers -= Taken;
		Count -= Taken;
	}
	DropEmptyBatches();
}

void FInventoryEntry::TakeOldestLooseBoxes(int32 Count)
{
	Count = FMath::Min(FMath::Max(0, Count), PhysicalLooseBoxes);
	PhysicalLooseBoxes -= Count;

	for (FStockBatch& Batch : Batches)
	{
		if (Count <= 0)
		{
			break;
		}
		const int32 Taken = FMath::Min(Batch.LooseBoxes, Count);
		Batch.LooseBoxes -= Taken;
		Count -= Taken;
	}
	DropEmptyBatches();
}

void FInventoryEntry::ConvertOldestToLoose(int32 Pallets)
{
	Pallets = FMath::Min(FMath::Max(0, Pallets), PhysicalStock);
	PhysicalStock -= Pallets;
	PhysicalLooseBoxes += Pallets * BoxesPerPallet;

	// The conversion happens inside the batch, so opening a pallet is not a way
	// to reset the clock on it.
	for (FStockBatch& Batch : Batches)
	{
		if (Pallets <= 0)
		{
			break;
		}
		const int32 Moved = FMath::Min(Batch.Containers, Pallets);
		Batch.Containers -= Moved;
		Batch.LooseBoxes += Moved * BoxesPerPallet;
		Pallets -= Moved;
	}
	DropEmptyBatches();
}

void FInventoryEntry::ConvertOldestToWhole(int32 Pallets)
{
	Pallets = FMath::Min(FMath::Max(0, Pallets), PhysicalLooseBoxes / BoxesPerPallet);
	PhysicalLooseBoxes -= Pallets * BoxesPerPallet;
	PhysicalStock += Pallets;

	int32 BoxesLeft = Pallets * BoxesPerPallet;
	int32 PalletsLeft = Pallets;
	for (FStockBatch& Batch : Batches)
	{
		if (BoxesLeft <= 0)
		{
			break;
		}
		const int32 Taken = FMath::Min(Batch.LooseBoxes, BoxesLeft);
		Batch.LooseBoxes -= Taken;
		BoxesLeft -= Taken;

		const int32 Whole = FMath::Min(Taken / BoxesPerPallet, PalletsLeft);
		Batch.Containers += Whole;
		PalletsLeft -= Whole;
	}
	// Boxes gathered across several batches can leave a pallet unassigned. It
	// belongs to the oldest batch still standing, because its goods do.
	if (PalletsLeft > 0 && Batches.Num() > 0)
	{
		Batches[0].Containers += PalletsLeft;
	}
	DropEmptyBatches();
}

double FInventoryEntry::OldestContainerMinute(double Fallback) const
{
	for (const FStockBatch& Batch : Batches)
	{
		if (Batch.Containers > 0)
		{
			return Batch.AcquiredAtMinute;
		}
	}
	return Fallback;
}

double FInventoryEntry::OldestLooseBoxMinute(double Fallback) const
{
	for (const FStockBatch& Batch : Batches)
	{
		if (Batch.LooseBoxes > 0)
		{
			return Batch.AcquiredAtMinute;
		}
	}
	return Fallback;
}

double FInventoryEntry::AverageAgeDays(double NowMinute) const
{
	double Weight = 0.0;
	double Sum = 0.0;
	for (const FStockBatch& Batch : Batches)
	{
		const double Quantity = Batch.Containers
			+ static_cast<double>(Batch.LooseBoxes) / BoxesPerPallet;
		if (Quantity <= 0.0)
		{
			continue;
		}
		Weight += Quantity;
		Sum += Quantity * DeadlineCondition::DaysBetween(Batch.AcquiredAtMinute, NowMinute);
	}
	return Weight > 0.0 ? FMath::Max(0.0, Sum / Weight) : 0.0;
}

bool FInventoryEntry::ExpireSpoiled(int32 ShelfLifeDays, double NowMinute,
	int32& OutContainers, int32& OutLooseBoxes)
{
	OutContainers = 0;
	OutLooseBoxes = 0;
	if (ShelfLifeDays <= 0)
	{
		return false;
	}

	for (FStockBatch& Batch : Batches)
	{
		if (!DeadlineCondition::HasSpoiled(ShelfLifeDays, Batch.AcquiredAtMinute, NowMinute))
		{
			// Oldest first, so the first batch still in date ends the search.
			break;
		}
		OutContainers += Batch.Containers;
		OutLooseBoxes += Batch.LooseBoxes;
		Batch.Containers = 0;
		Batch.LooseBoxes = 0;
	}

	if (OutContainers == 0 && OutLooseBoxes == 0)
	{
		return false;
	}

	DropEmptyBatches();
	PhysicalStock = FMath::Max(0, PhysicalStock - OutContainers);
	PhysicalLooseBoxes = FMath::Max(0, PhysicalLooseBoxes - OutLooseBoxes);

	// Rotten goods you had papers for get written off; goods you never had
	// papers for simply stop existing. Clamping to the physical count is what
	// keeps the discrepancy (GDD 7.1) honest instead of inventing a negative
	// book balance.
	RecordedStock = FMath::Min(RecordedStock, PhysicalStock);
	RecordedLooseBoxes = FMath::Min(RecordedLooseBoxes, PhysicalLooseBoxes);
	return true;
}

void FInventoryEntry::SyncBatches(double NowMinute)
{
	int32 BatchContainers = 0;
	int32 BatchLooseBoxes = 0;
	for (const FStockBatch& Batch : Batches)
	{
		BatchContainers += Batch.Containers;
		BatchLooseBoxes += Batch.LooseBoxes;
	}

	if (BatchContainers == PhysicalStock && BatchLooseBoxes == PhysicalLooseBoxes)
	{
		return;
	}

	// Surplus: goods the list has never heard of. They start ageing now.
	// Deficit: goods the list still thinks are there. Oldest go first.
	const int32 ExtraContainers = PhysicalStock - BatchContainers;
	const int32 ExtraLooseBoxes = PhysicalLooseBoxes - BatchLooseBoxes;

	PhysicalStock = BatchContainers;
	PhysicalLooseBoxes = BatchLooseBoxes;

	if (ExtraContainers > 0 || ExtraLooseBoxes > 0)
	{
		AddBatch(FMath::Max(0, ExtraContainers), FMath::Max(0, ExtraLooseBoxes), NowMinute);
	}
	if (ExtraContainers < 0)
	{
		TakeOldestContainers(-ExtraContainers);
	}
	if (ExtraLooseBoxes < 0)
	{
		TakeOldestLooseBoxes(-ExtraLooseBoxes);
	}
}

void UInventorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// The catalogue must exist before we can size anything in Box Units, and
	// the clock before anything can have an age.
	Collection.InitializeDependency(UProductCatalogSubsystem::StaticClass());
	Collection.InitializeDependency(UTimeSubsystem::StaticClass());
	Super::Initialize(Collection);

	ResetUnitCounts();
	EmptyPallets = FMath::Max(0, UDeadlineSettings::Get().StartingEmptyPallets);

	// The only thing that drives spoilage. Shelf life is measured in whole
	// days, so one sweep per day rollover catches everything a per-frame check
	// would (CLAUDE.md tick rule 2: subscribe, do not poll).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTimeSubsystem* Time = GI->GetSubsystem<UTimeSubsystem>())
		{
			Time->OnDayChanged.AddUniqueDynamic(this, &UInventorySubsystem::HandleDayChanged);
		}
	}
}

double UInventorySubsystem::NowMinute() const
{
	const UGameInstance* GI = GetGameInstance();
	const UTimeSubsystem* Time = GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
	return Time ? Time->GetTotalMinutes() : 0.0;
}

int32 UInventorySubsystem::ShelfLifeOf(FName ProductID) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	return Row ? Row->ShelfLifeDays : 0;
}

void UInventorySubsystem::EnsureSwept() const
{
	// The day rollover normally gets here first, but it is a 1 Hz poll and the
	// clock can jump (travel, a cheat, a load). A reader must never be told
	// about goods that have already rotted, so sweep on the way in. The sweep
	// itself is a day-number comparison once the day has been done.
	const_cast<UInventorySubsystem*>(this)->RefreshCondition();
}

void UInventorySubsystem::HandleDayChanged(int32 NewDay)
{
	RefreshCondition();
}

UProductCatalogSubsystem* UInventorySubsystem::GetCatalogue() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
}

void UInventorySubsystem::ResetUnitCounts()
{
	const UDeadlineSettings& Settings = UDeadlineSettings::Get();

	UnitCounts.Reset();
	UnitCounts.SetNumZeroed(NumStorageClasses);
	UnitCounts[static_cast<int32>(EStorageClass::Rack)]       = FMath::Max(0, Settings.StartingRacks);
	UnitCounts[static_cast<int32>(EStorageClass::PalletBay)]  = FMath::Max(0, Settings.StartingPalletBays);
	UnitCounts[static_cast<int32>(EStorageClass::ColdZone)]   = FMath::Max(0, Settings.StartingColdZones);
	UnitCounts[static_cast<int32>(EStorageClass::SecureRack)] = FMath::Max(0, Settings.StartingSecureCages);
}

// --- Product lookups ---------------------------------------------------------

float UInventorySubsystem::VolumeBUOf(FName ProductID) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	return Catalogue ? Catalogue->GetVolumeBU(ProductID) : 0.f;
}

float UInventorySubsystem::LooseBoxVolumeBUOf(FName ProductID) const
{
	return VolumeBUOf(ProductID) / FInventoryEntry::BoxesPerPallet;
}

float UInventorySubsystem::EntryVolumeBU(FName ProductID, const FInventoryEntry& Entry) const
{
	return VolumeBUOf(ProductID) * Entry.PhysicalStock
		+ LooseBoxVolumeBUOf(ProductID) * Entry.PhysicalLooseBoxes;
}

EStorageClass UInventorySubsystem::GetStorageClassFor(FName ProductID) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	return Row ? FStorageClassRules::ClassForContainer(Row->ContainerType)
	           : EStorageClass::Rack;
}

// --- Mutations ---------------------------------------------------------------

bool UInventorySubsystem::AddStock(FName ProductID, int32 Containers, bool bRecorded)
{
	return AddStockAged(ProductID, Containers, bRecorded, NowMinute());
}

bool UInventorySubsystem::AddStockAged(FName ProductID, int32 Containers, bool bRecorded,
	double AcquiredAtMinute)
{
	EnsureSwept();

	if (Containers <= 0 || !HasSpaceFor(ProductID, Containers))
	{
		return false;
	}

	FInventoryEntry& Entry = Entries.FindOrAdd(ProductID);
	Entry.AddBatch(Containers, 0, AcquiredAtMinute);
	if (bRecorded)
	{
		Entry.RecordedStock += Containers;
	}

	OnStockChanged.Broadcast(ProductID);
	return true;
}

bool UInventorySubsystem::RemoveStock(FName ProductID, int32 Containers, bool bRecorded)
{
	if (Containers <= 0)
	{
		return false;
	}

	EnsureSwept();

	FInventoryEntry* Entry = Entries.Find(ProductID);
	if (!Entry)
	{
		return false;
	}

	// Whole containers first, then complete pallets rebuilt out of loose boxes.
	// Without the second half, opening a pallet to carry it would lock those
	// goods out of every sale until the player closed it again.
	const int32 Available = Entry->PhysicalStock
		+ Entry->PhysicalLooseBoxes / FInventoryEntry::BoxesPerPallet;
	if (Available < Containers)
	{
		return false;
	}

	const int32 FromWhole = FMath::Min(Entry->PhysicalStock, Containers);
	const int32 FromLoose = Containers - FromWhole;

	Entry->TakeOldestContainers(FromWhole);
	Entry->TakeOldestLooseBoxes(FromLoose * FInventoryEntry::BoxesPerPallet);

	if (bRecorded)
	{
		// An invoiced sale cannot take more off the books than the books hold,
		// and it takes it in the same order: whole containers, then pallets'
		// worth of loose boxes.
		int32 Left = Containers;

		const int32 RecordedWhole = FMath::Min(Entry->RecordedStock, Left);
		Entry->RecordedStock -= RecordedWhole;
		Left -= RecordedWhole;

		const int32 RecordedPallets = FMath::Min(
			Entry->RecordedLooseBoxes / FInventoryEntry::BoxesPerPallet, Left);
		Entry->RecordedLooseBoxes -= RecordedPallets * FInventoryEntry::BoxesPerPallet;
	}

	if (Entry->IsEmpty())
	{
		Entries.Remove(ProductID);
	}

	OnStockChanged.Broadcast(ProductID);
	return true;
}

bool UInventorySubsystem::OpenPallet(FName ProductID, int32 Pallets)
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (Pallets <= 0 || !Row || Row->ContainerType != EContainerType::Pallet)
	{
		return false;
	}

	FInventoryEntry* Entry = Entries.Find(ProductID);
	if (!Entry || Entry->PhysicalStock < Pallets)
	{
		return false;
	}

	// The bay is about to give up 16 BU and the racks are about to take them.
	const int32 Boxes = Pallets * FInventoryEntry::BoxesPerPallet;
	const float NeededBU = LooseBoxVolumeBUOf(ProductID) * Boxes;
	if (NeededBU > GetClassFreeBU(LooseBoxClass) + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// Age rides along: an opened pallet is the same goods in a different shape,
	// so breaking one down is not a way to reset its shelf life.
	Entry->ConvertOldestToLoose(Pallets);

	// Nothing changed hands, so the books follow the goods: only pallets the
	// books knew about become boxes the books know about.
	const int32 RecordedMoved = FMath::Min(Entry->RecordedStock, Pallets);
	Entry->RecordedStock -= RecordedMoved;
	Entry->RecordedLooseBoxes += RecordedMoved * FInventoryEntry::BoxesPerPallet;

	// The wood stays behind (GDD 5.6). Taking the boxes off a pallet does not
	// make the pallet disappear, and closing one later needs it back.
	EmptyPallets += Pallets;

	OnStockChanged.Broadcast(ProductID);
	return true;
}

bool UInventorySubsystem::ClosePallet(FName ProductID, int32 Pallets)
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (Pallets <= 0 || !Row || Row->ContainerType != EContainerType::Pallet)
	{
		return false;
	}

	const int32 Boxes = Pallets * FInventoryEntry::BoxesPerPallet;

	FInventoryEntry* Entry = Entries.Find(ProductID);
	if (!Entry || Entry->PhysicalLooseBoxes < Boxes)
	{
		return false;
	}

	// You cannot stack sixteen boxes on nothing.
	if (EmptyPallets < Pallets)
	{
		return false;
	}

	const float NeededBU = VolumeBUOf(ProductID) * Pallets;
	if (NeededBU > GetClassFreeBU(EStorageClass::PalletBay) + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	Entry->ConvertOldestToWhole(Pallets);
	EmptyPallets -= Pallets;

	// Only whole pallets' worth move on the books, so a stray count of
	// recorded boxes is never rounded away.
	const int32 RecordedPallets = FMath::Min(
		Entry->RecordedLooseBoxes / FInventoryEntry::BoxesPerPallet, Pallets);
	Entry->RecordedLooseBoxes -= RecordedPallets * FInventoryEntry::BoxesPerPallet;
	Entry->RecordedStock += RecordedPallets;

	OnStockChanged.Broadcast(ProductID);
	return true;
}

bool UInventorySubsystem::HasSpaceForLooseBoxes(FName ProductID, int32 Boxes) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (Boxes <= 0 || !Row || Row->ContainerType != EContainerType::Pallet)
	{
		return false;
	}

	const float NeededBU = LooseBoxVolumeBUOf(ProductID) * Boxes;
	return NeededBU <= GetClassFreeBU(LooseBoxClass) + KINDA_SMALL_NUMBER;
}

bool UInventorySubsystem::AddLooseBoxes(FName ProductID, int32 Boxes, bool bRecorded)
{
	return AddLooseBoxesAged(ProductID, Boxes, bRecorded, NowMinute());
}

bool UInventorySubsystem::AddLooseBoxesAged(FName ProductID, int32 Boxes, bool bRecorded,
	double AcquiredAtMinute)
{
	EnsureSwept();

	if (!HasSpaceForLooseBoxes(ProductID, Boxes))
	{
		return false;
	}

	FInventoryEntry& Entry = Entries.FindOrAdd(ProductID);
	Entry.AddBatch(0, Boxes, AcquiredAtMinute);
	if (bRecorded)
	{
		Entry.RecordedLooseBoxes += Boxes;
	}

	OnStockChanged.Broadcast(ProductID);
	return true;
}

bool UInventorySubsystem::RemoveLooseBoxes(FName ProductID, int32 Boxes, bool bRecorded)
{
	if (Boxes <= 0)
	{
		return false;
	}

	FInventoryEntry* Entry = Entries.Find(ProductID);
	if (!Entry || Entry->PhysicalLooseBoxes < Boxes)
	{
		return false;
	}

	Entry->TakeOldestLooseBoxes(Boxes);
	if (bRecorded)
	{
		Entry->RecordedLooseBoxes = FMath::Max(0, Entry->RecordedLooseBoxes - Boxes);
	}

	if (Entry->IsEmpty())
	{
		Entries.Remove(ProductID);
	}

	OnStockChanged.Broadcast(ProductID);
	return true;
}

void UInventorySubsystem::AddEmptyPallets(int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	EmptyPallets += Count;
	OnCapacityChanged.Broadcast();
}

bool UInventorySubsystem::RemoveEmptyPallets(int32 Count)
{
	if (Count <= 0 || EmptyPallets < Count)
	{
		return false;
	}
	EmptyPallets -= Count;
	OnCapacityChanged.Broadcast();
	return true;
}

void UInventorySubsystem::ResetAll()
{
	Entries.Empty();
	ResetUnitCounts();
	EmptyPallets = FMath::Max(0, UDeadlineSettings::Get().StartingEmptyPallets);
	LastSweptDay = -1;
	OnCapacityChanged.Broadcast();
}

void UInventorySubsystem::RestoreEntry(FName ProductID, const FInventoryEntry& Entry)
{
	FInventoryEntry& Stored = Entries.Add(ProductID, Entry);

	// A save written before batches existed (v4 and earlier) carries counts
	// with no ages. Those goods start ageing from the moment they load rather
	// than rotting the instant the player presses Continue.
	Stored.SyncBatches(NowMinute());

	// Loading puts the clock and the shelves back together at once, so
	// whatever the last sweep concluded is stale.
	LastSweptDay = -1;
}

// --- Condition: spoilage and obsolescence (GDD 5.1) --------------------------

void UInventorySubsystem::RefreshCondition()
{
	const double Now = NowMinute();
	const int32 Today = FMath::FloorToInt(Now / DeadlineCondition::MinutesPerDay);
	if (Today == LastSweptDay)
	{
		return;
	}
	LastSweptDay = Today;

	// What was lost is collected first and announced afterwards. A listener on
	// OnStockSpoiled is free to touch the ledger, and it must not do that while
	// this loop still holds an iterator into it.
	struct FWriteOff
	{
		FName ProductID;
		int32 Containers = 0;
		bool bOnTheBooks = false;
		bool bEmptied = false;
	};
	TArray<FWriteOff> WriteOffs;

	for (TPair<FName, FInventoryEntry>& Pair : Entries)
	{
		const int32 ShelfLife = ShelfLifeOf(Pair.Key);
		if (ShelfLife <= 0)
		{
			continue;
		}

		const float RecordedBefore = Pair.Value.RecordedContainers();

		int32 LostContainers = 0;
		int32 LostBoxes = 0;
		if (!Pair.Value.ExpireSpoiled(ShelfLife, Now, LostContainers, LostBoxes))
		{
			continue;
		}

		FWriteOff Loss;
		Loss.ProductID = Pair.Key;
		// Loose boxes are sixteenths of a pallet. Rounding the remainder up
		// keeps a write-off from being quietly smaller than what was lost.
		Loss.Containers = LostContainers
			+ FMath::DivideAndRoundUp(LostBoxes, FInventoryEntry::BoxesPerPallet);
		// Did the paperwork move with the goods? Grey stock rots off the shelf
		// without ever having been on the books.
		Loss.bOnTheBooks = Pair.Value.RecordedContainers() < RecordedBefore - KINDA_SMALL_NUMBER;
		Loss.bEmptied = Pair.Value.IsEmpty();
		WriteOffs.Add(Loss);
	}

	for (const FWriteOff& Loss : WriteOffs)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline] %s: %d container(s) spoiled (%s)."),
			*Loss.ProductID.ToString(), Loss.Containers,
			Loss.bOnTheBooks ? TEXT("written off") : TEXT("grey"));

		if (Loss.bEmptied)
		{
			Entries.Remove(Loss.ProductID);
		}
		OnStockSpoiled.Broadcast(Loss.ProductID, Loss.Containers, Loss.bOnTheBooks);
		OnStockChanged.Broadcast(Loss.ProductID);
	}
}

float UInventorySubsystem::GetConditionMultiplier(FName ProductID) const
{
	EnsureSwept();

	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	const FInventoryEntry* Entry = Entries.Find(ProductID);
	if (!Row || !Entry || Row->ObsolescencePerDay <= 0.f)
	{
		return 1.f;
	}

	return DeadlineCondition::ObsolescenceMultiplier(Row->ObsolescencePerDay,
		Entry->AverageAgeDays(NowMinute()), UDeadlineSettings::Get().ObsolescenceFloor);
}

float UInventorySubsystem::GetAverageAgeDays(FName ProductID) const
{
	EnsureSwept();

	const FInventoryEntry* Entry = Entries.Find(ProductID);
	return Entry ? static_cast<float>(Entry->AverageAgeDays(NowMinute())) : 0.f;
}

int32 UInventorySubsystem::GetDaysUntilSpoilage(FName ProductID) const
{
	EnsureSwept();

	const int32 ShelfLife = ShelfLifeOf(ProductID);
	const FInventoryEntry* Entry = Entries.Find(ProductID);
	if (ShelfLife <= 0 || !Entry)
	{
		return -1;
	}

	const double Now = NowMinute();
	const double Oldest = FMath::Min(Entry->OldestContainerMinute(Now),
		Entry->OldestLooseBoxMinute(Now));
	return DeadlineCondition::DaysUntilSpoilage(ShelfLife, Oldest, Now);
}

double UInventorySubsystem::GetOldestAcquiredMinute(FName ProductID, bool bLoose) const
{
	const double Now = NowMinute();
	const FInventoryEntry* Entry = Entries.Find(ProductID);
	if (!Entry)
	{
		return Now;
	}
	return bLoose ? Entry->OldestLooseBoxMinute(Now) : Entry->OldestContainerMinute(Now);
}

TArray<FName> UInventorySubsystem::GetSpoilageWarnings(int32 DaysAhead) const
{
	EnsureSwept();

	TArray<TPair<int32, FName>> Soon;
	for (const TPair<FName, FInventoryEntry>& Pair : Entries)
	{
		const int32 Left = GetDaysUntilSpoilage(Pair.Key);
		if (Left >= 0 && Left <= DaysAhead)
		{
			Soon.Add(TPair<int32, FName>(Left, Pair.Key));
		}
	}
	Soon.Sort([](const TPair<int32, FName>& A, const TPair<int32, FName>& B)
	{
		return A.Key != B.Key ? A.Key < B.Key : FNameLexicalLess()(A.Value, B.Value);
	});

	TArray<FName> Result;
	Result.Reserve(Soon.Num());
	for (const TPair<int32, FName>& Pair : Soon)
	{
		Result.Add(Pair.Value);
	}
	return Result;
}

void UInventorySubsystem::AgeAllStockByDays(float Days)
{
	const double Shift = static_cast<double>(Days) * DeadlineCondition::MinutesPerDay;
	for (TPair<FName, FInventoryEntry>& Pair : Entries)
	{
		for (FStockBatch& Batch : Pair.Value.Batches)
		{
			Batch.AcquiredAtMinute -= Shift;
		}
	}

	// The cheat moved the goods, not the clock, so the day number has not
	// changed and the sweep would otherwise decline to run.
	LastSweptDay = -1;
	RefreshCondition();

	TArray<FName> IDs;
	Entries.GetKeys(IDs);
	for (const FName& ID : IDs)
	{
		OnStockChanged.Broadcast(ID);
	}
}

// --- Queries -----------------------------------------------------------------

int32 UInventorySubsystem::GetPhysicalStock(FName ProductID) const
{
	const FInventoryEntry* Entry = Entries.Find(ProductID);
	return Entry ? Entry->PhysicalStock : 0;
}

int32 UInventorySubsystem::GetRecordedStock(FName ProductID) const
{
	const FInventoryEntry* Entry = Entries.Find(ProductID);
	return Entry ? Entry->RecordedStock : 0;
}

int32 UInventorySubsystem::GetLooseBoxes(FName ProductID) const
{
	const FInventoryEntry* Entry = Entries.Find(ProductID);
	return Entry ? Entry->PhysicalLooseBoxes : 0;
}

int32 UInventorySubsystem::GetRecordedLooseBoxes(FName ProductID) const
{
	const FInventoryEntry* Entry = Entries.Find(ProductID);
	return Entry ? Entry->RecordedLooseBoxes : 0;
}

float UInventorySubsystem::GetPhysicalContainers(FName ProductID) const
{
	const FInventoryEntry* Entry = Entries.Find(ProductID);
	return Entry ? Entry->PhysicalContainers() : 0.f;
}

float UInventorySubsystem::GetDiscrepancy(FName ProductID) const
{
	const FInventoryEntry* Entry = Entries.Find(ProductID);
	return Entry ? Entry->Discrepancy() : 0.f;
}

float UInventorySubsystem::GetTotalDiscrepancy() const
{
	float Total = 0.f;
	for (const TPair<FName, FInventoryEntry>& Pair : Entries)
	{
		Total += FMath::Abs(Pair.Value.Discrepancy());
	}
	return Total;
}

// --- Storage classes ---------------------------------------------------------

int32 UInventorySubsystem::GetUnitCount(EStorageClass Class) const
{
	const int32 Index = static_cast<int32>(Class);
	return UnitCounts.IsValidIndex(Index) ? UnitCounts[Index] : 0;
}

void UInventorySubsystem::SetUnitCount(EStorageClass Class, int32 Units)
{
	const int32 Index = static_cast<int32>(Class);
	if (UnitCounts.Num() < NumStorageClasses)
	{
		UnitCounts.SetNumZeroed(NumStorageClasses);
	}
	if (!UnitCounts.IsValidIndex(Index))
	{
		return;
	}

	UnitCounts[Index] = FMath::Max(0, Units);
	OnCapacityChanged.Broadcast();
}

float UInventorySubsystem::GetClassCapacityBU(EStorageClass Class) const
{
	return GetUnitCount(Class) * FStorageClassRules::BUPerUnit(Class);
}

float UInventorySubsystem::GetClassUsedBU(EStorageClass Class) const
{
	float Used = 0.f;
	for (const TPair<FName, FInventoryEntry>& Pair : Entries)
	{
		if (Pair.Value.PhysicalStock > 0 && GetStorageClassFor(Pair.Key) == Class)
		{
			Used += VolumeBUOf(Pair.Key) * Pair.Value.PhysicalStock;
		}

		// Loose boxes always sit on the racks, whatever bay their pallet came from.
		if (Class == LooseBoxClass && Pair.Value.PhysicalLooseBoxes > 0)
		{
			Used += LooseBoxVolumeBUOf(Pair.Key) * Pair.Value.PhysicalLooseBoxes;
		}
	}
	return Used;
}

float UInventorySubsystem::GetClassFreeBU(EStorageClass Class) const
{
	return FMath::Max(0.f, GetClassCapacityBU(Class) - GetClassUsedBU(Class));
}

TArray<FStoredContainer> UInventorySubsystem::GetClassContents(EStorageClass Class) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();

	TArray<FStoredContainer> Result;
	if (!Catalogue)
	{
		return Result;
	}

	// Sorted, so the same warehouse always draws its boxes in the same slots
	// and nothing jumps around when an unrelated product changes.
	TArray<FName> IDs;
	Entries.GetKeys(IDs);
	IDs.Sort(FNameLexicalLess());

	for (const FName& ID : IDs)
	{
		const FProductRow* Row = Catalogue->FindProduct(ID);
		if (!Row)
		{
			continue;
		}
		const FInventoryEntry& Entry = Entries[ID];

		if (FStorageClassRules::ClassForContainer(Row->ContainerType) == Class)
		{
			FStoredContainer Whole;
			Whole.ProductID = ID;
			Whole.ContainerType = Row->ContainerType;
			Whole.VolumeBU = VolumeBUOf(ID);
			for (int32 i = 0; i < Entry.PhysicalStock; ++i)
			{
				Result.Add(Whole);
			}
		}

		if (Class == LooseBoxClass && Entry.PhysicalLooseBoxes > 0)
		{
			FStoredContainer Loose;
			Loose.ProductID = ID;
			Loose.ContainerType = EContainerType::BoxM;
			Loose.VolumeBU = LooseBoxVolumeBUOf(ID);
			Loose.bLooseBox = true;
			for (int32 i = 0; i < Entry.PhysicalLooseBoxes; ++i)
			{
				Result.Add(Loose);
			}
		}
	}
	return Result;
}

// --- Warehouse totals --------------------------------------------------------

float UInventorySubsystem::GetUsedBU() const
{
	float Used = 0.f;
	for (const TPair<FName, FInventoryEntry>& Pair : Entries)
	{
		Used += EntryVolumeBU(Pair.Key, Pair.Value);
	}
	return Used;
}

float UInventorySubsystem::GetCapacityBU() const
{
	float Capacity = 0.f;
	for (int32 Index = 0; Index < NumStorageClasses; ++Index)
	{
		Capacity += GetClassCapacityBU(FStorageClassRules::FromIndex(Index));
	}
	return Capacity;
}

bool UInventorySubsystem::HasSpaceFor(FName ProductID, int32 Containers) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	if (!Catalogue || !Catalogue->IsValidProduct(ProductID) || Containers <= 0)
	{
		return false;
	}

	const float NeededBU = VolumeBUOf(ProductID) * Containers;
	return NeededBU <= GetClassFreeBU(GetStorageClassFor(ProductID)) + KINDA_SMALL_NUMBER;
}

TArray<FName> UInventorySubsystem::GetStockedProductIDs() const
{
	TArray<FName> Result;
	Entries.GetKeys(Result);
	Result.Sort(FNameLexicalLess());
	return Result;
}

TArray<FName> UInventorySubsystem::GetStockedProductIDsInClass(EStorageClass Class) const
{
	TArray<FName> Result;
	for (const FName& ID : GetStockedProductIDs())
	{
		const FInventoryEntry& Entry = Entries[ID];
		const bool bWholeHere = Entry.PhysicalStock > 0 && GetStorageClassFor(ID) == Class;
		const bool bLooseHere = Class == LooseBoxClass && Entry.PhysicalLooseBoxes > 0;
		if (bWholeHere || bLooseHere)
		{
			Result.Add(ID);
		}
	}
	return Result;
}
