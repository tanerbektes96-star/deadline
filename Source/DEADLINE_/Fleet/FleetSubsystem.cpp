// Copyright DEADLINE. All Rights Reserved.

#include "Fleet/FleetSubsystem.h"

#include "Core/DeadlineSettings.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Inventory/Condition.h"

void UFleetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Cargo volume and weight are product data, so the catalogue comes first,
	// and the clock before cargo can have an age.
	Collection.InitializeDependency(UProductCatalogSubsystem::StaticClass());
	Collection.InitializeDependency(UTimeSubsystem::StaticClass());
	Super::Initialize(Collection);

	EnsureTable();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTimeSubsystem* Time = GI->GetSubsystem<UTimeSubsystem>())
		{
			Time->OnDayChanged.AddUniqueDynamic(this, &UFleetSubsystem::HandleDayChanged);
		}
	}
}

double UFleetSubsystem::NowMinute() const
{
	const UGameInstance* GI = GetGameInstance();
	const UTimeSubsystem* Time = GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
	return Time ? Time->GetTotalMinutes() : 0.0;
}

void UFleetSubsystem::HandleDayChanged(int32 NewDay)
{
	RefreshCondition();
}

UProductCatalogSubsystem* UFleetSubsystem::GetCatalogue() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
}

void UFleetSubsystem::EnsureTable() const
{
	if (VehicleTable)
	{
		return;
	}

	const TSoftObjectPtr<UDataTable>& Soft = UDeadlineSettings::Get().VehicleTable;
	if (Soft.IsNull())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Deadline] No VehicleTable set in Project Settings > Deadline."));
		return;
	}

	VehicleTable = Soft.LoadSynchronous();
	if (!VehicleTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[Deadline] DT_Vehicles could not be loaded."));
	}
}

// --- Product lookups ---------------------------------------------------------

float UFleetSubsystem::VolumeBUOf(FName ProductID) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	return Catalogue ? Catalogue->GetVolumeBU(ProductID) : 0.f;
}

float UFleetSubsystem::LooseBoxVolumeBUOf(FName ProductID) const
{
	return VolumeBUOf(ProductID) / FInventoryEntry::BoxesPerPallet;
}

float UFleetSubsystem::WeightKgOf(FName ProductID) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	return Row ? Row->WeightKg : 0.f;
}

// --- Fleet roster ------------------------------------------------------------

void UFleetSubsystem::RegisterVehicle(FName VehicleKey, FName VehicleID)
{
	if (VehicleKey.IsNone())
	{
		return;
	}

	// Keep whatever this key already carries: coming back to a level must not
	// empty the truck that was left loaded.
	FVehicleCargo& Cargo = Vehicles.FindOrAdd(VehicleKey);
	Cargo.VehicleID = VehicleID;

	OnCargoChanged.Broadcast(VehicleKey);
}

TArray<FName> UFleetSubsystem::GetVehicleKeys() const
{
	TArray<FName> Keys;
	Vehicles.GetKeys(Keys);
	Keys.Sort(FNameLexicalLess());
	return Keys;
}

FName UFleetSubsystem::GetVehicleID(FName VehicleKey) const
{
	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	return Cargo ? Cargo->VehicleID : NAME_None;
}

const FVehicleRow* UFleetSubsystem::FindVehicleRow(FName VehicleID) const
{
	EnsureTable();
	if (!VehicleTable || VehicleID.IsNone())
	{
		return nullptr;
	}
	return VehicleTable->FindRow<FVehicleRow>(VehicleID, TEXT("UFleetSubsystem"), /*bWarnIfMissing=*/false);
}

bool UFleetSubsystem::GetVehicleRow(FName VehicleID, FVehicleRow& OutRow) const
{
	if (const FVehicleRow* Row = FindVehicleRow(VehicleID))
	{
		OutRow = *Row;
		return true;
	}
	return false;
}

TArray<FName> UFleetSubsystem::GetAllVehicleIDs() const
{
	EnsureTable();

	TArray<FName> IDs;
	if (VehicleTable)
	{
		IDs = VehicleTable->GetRowNames();
		IDs.Sort(FNameLexicalLess());
	}
	return IDs;
}

// --- Loading -----------------------------------------------------------------

bool UFleetSubsystem::CanLoad(FName VehicleKey, FName ProductID, int32 Count,
	bool bLoose, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	const FVehicleRow* Vehicle = Cargo ? FindVehicleRow(Cargo->VehicleID) : nullptr;
	if (!Cargo || !Vehicle || Count <= 0)
	{
		OutReason = NSLOCTEXT("Deadline", "FleetUnknown", "No such vehicle");
		return false;
	}

	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Product = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (!Product)
	{
		// Name the ID that was refused: "unknown goods" on its own sends you
		// hunting through the catalogue for a typo you cannot see.
		OutReason = FText::Format(
			NSLOCTEXT("Deadline", "FleetUnknownProduct", "Unknown goods '{0}'"),
			FText::FromName(ProductID));
		return false;
	}

	// A loose box is a Box M off a pallet, so the carry rules follow the box,
	// not the pallet it came from.
	const EContainerType Type = bLoose ? EContainerType::BoxM : Product->ContainerType;

	// GDD 9.5 carry restrictions.
	if (Vehicle->ColdOnly && Type != EContainerType::ColdTote)
	{
		OutReason = NSLOCTEXT("Deadline", "FleetColdOnly", "Reefer carries cold totes only");
		return false;
	}
	if (!Vehicle->AllowsSecure && Type == EContainerType::SecureCase)
	{
		OutReason = NSLOCTEXT("Deadline", "FleetNoSecure", "Open bed cannot carry a secure case");
		return false;
	}

	const float NeededBU = (bLoose ? LooseBoxVolumeBUOf(ProductID) : VolumeBUOf(ProductID)) * Count;
	if (NeededBU > GetFreeBU(VehicleKey) + KINDA_SMALL_NUMBER)
	{
		OutReason = NSLOCTEXT("Deadline", "FleetFullBU", "Cargo hold full");
		return false;
	}

	const float NeededKg = (bLoose ? WeightKgOf(ProductID) / FInventoryEntry::BoxesPerPallet
	                               : WeightKgOf(ProductID)) * Count;
	if (GetWeightKg(VehicleKey) + NeededKg > Vehicle->MaxWeightKg + KINDA_SMALL_NUMBER)
	{
		OutReason = NSLOCTEXT("Deadline", "FleetOverweight", "Over the axle limit");
		return false;
	}

	return true;
}

bool UFleetSubsystem::Load(FName VehicleKey, FName ProductID, int32 Count,
	bool bRecorded, bool bLoose)
{
	return LoadAged(VehicleKey, ProductID, Count, bRecorded, bLoose, NowMinute());
}

bool UFleetSubsystem::LoadAged(FName VehicleKey, FName ProductID, int32 Count,
	bool bRecorded, bool bLoose, double AcquiredAtMinute)
{
	FText Ignored;
	if (!CanLoad(VehicleKey, ProductID, Count, bLoose, Ignored))
	{
		return false;
	}

	FInventoryEntry& Entry = Vehicles[VehicleKey].Entries.FindOrAdd(ProductID);
	Entry.AddBatch(bLoose ? 0 : Count, bLoose ? Count : 0, AcquiredAtMinute);
	if (bRecorded)
	{
		(bLoose ? Entry.RecordedLooseBoxes : Entry.RecordedStock) += Count;
	}

	OnCargoChanged.Broadcast(VehicleKey);
	return true;
}

bool UFleetSubsystem::Unload(FName VehicleKey, FName ProductID, int32 Count,
	bool bRecorded, bool bLoose)
{
	FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	FInventoryEntry* Entry = Cargo ? Cargo->Entries.Find(ProductID) : nullptr;
	if (!Entry || Count <= 0)
	{
		return false;
	}

	if (bLoose)
	{
		if (Entry->PhysicalLooseBoxes < Count)
		{
			return false;
		}
		Entry->TakeOldestLooseBoxes(Count);
		if (bRecorded)
		{
			Entry->RecordedLooseBoxes = FMath::Max(0, Entry->RecordedLooseBoxes - Count);
		}
	}
	else
	{
		if (Entry->PhysicalStock < Count)
		{
			return false;
		}
		Entry->TakeOldestContainers(Count);
		if (bRecorded)
		{
			Entry->RecordedStock = FMath::Max(0, Entry->RecordedStock - Count);
		}
	}

	if (Entry->IsEmpty())
	{
		Cargo->Entries.Remove(ProductID);
	}

	OnCargoChanged.Broadcast(VehicleKey);
	return true;
}

FName UFleetSubsystem::PickUnloadProduct(FName VehicleKey, bool& bOutLoose) const
{
	bOutLoose = false;

	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	if (!Cargo)
	{
		return NAME_None;
	}

	TArray<FName> IDs;
	Cargo->Entries.GetKeys(IDs);
	IDs.Sort(FNameLexicalLess());

	for (const FName& ID : IDs)
	{
		const FInventoryEntry& Entry = Cargo->Entries[ID];
		if (Entry.PhysicalStock > 0)
		{
			return ID;
		}
		if (Entry.PhysicalLooseBoxes > 0)
		{
			bOutLoose = true;
			return ID;
		}
	}
	return NAME_None;
}

// --- Queries -----------------------------------------------------------------

float UFleetSubsystem::GetCapacityBU(FName VehicleKey) const
{
	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	const FVehicleRow* Row = Cargo ? FindVehicleRow(Cargo->VehicleID) : nullptr;
	return Row ? Row->CapacityBU : 0.f;
}

float UFleetSubsystem::GetUsedBU(FName VehicleKey) const
{
	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	if (!Cargo)
	{
		return 0.f;
	}

	float Used = 0.f;
	for (const TPair<FName, FInventoryEntry>& Pair : Cargo->Entries)
	{
		Used += VolumeBUOf(Pair.Key) * Pair.Value.PhysicalStock
			+ LooseBoxVolumeBUOf(Pair.Key) * Pair.Value.PhysicalLooseBoxes;
	}
	return Used;
}

float UFleetSubsystem::GetFreeBU(FName VehicleKey) const
{
	return FMath::Max(0.f, GetCapacityBU(VehicleKey) - GetUsedBU(VehicleKey));
}

float UFleetSubsystem::GetWeightKg(FName VehicleKey) const
{
	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	if (!Cargo)
	{
		return 0.f;
	}

	float Kg = 0.f;
	for (const TPair<FName, FInventoryEntry>& Pair : Cargo->Entries)
	{
		const float PerContainer = WeightKgOf(Pair.Key);
		Kg += PerContainer * Pair.Value.PhysicalStock
			+ (PerContainer / FInventoryEntry::BoxesPerPallet) * Pair.Value.PhysicalLooseBoxes;
	}
	return Kg;
}

float UFleetSubsystem::GetMaxWeightKg(FName VehicleKey) const
{
	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	const FVehicleRow* Row = Cargo ? FindVehicleRow(Cargo->VehicleID) : nullptr;
	return Row ? Row->MaxWeightKg : 0.f;
}

bool UFleetSubsystem::IsEmpty(FName VehicleKey) const
{
	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	return !Cargo || Cargo->Entries.IsEmpty();
}

TArray<FStoredContainer> UFleetSubsystem::GetCargoContents(FName VehicleKey) const
{
	TArray<FStoredContainer> Result;

	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	if (!Cargo || !Catalogue)
	{
		return Result;
	}

	// Sorted, so the load does not reshuffle itself when an unrelated product
	// changes (same reason as the rack stacks).
	TArray<FName> IDs;
	Cargo->Entries.GetKeys(IDs);
	IDs.Sort(FNameLexicalLess());

	for (const FName& ID : IDs)
	{
		const FProductRow* Row = Catalogue->FindProduct(ID);
		if (!Row)
		{
			continue;
		}
		const FInventoryEntry& Entry = Cargo->Entries[ID];

		FStoredContainer Whole;
		Whole.ProductID = ID;
		Whole.ContainerType = Row->ContainerType;
		Whole.VolumeBU = VolumeBUOf(ID);
		for (int32 i = 0; i < Entry.PhysicalStock; ++i)
		{
			Result.Add(Whole);
		}

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
	return Result;
}

// --- Fleet-wide totals -------------------------------------------------------

int32 UFleetSubsystem::GetTotalWholeContainers(FName ProductID) const
{
	int32 Total = 0;
	for (const TPair<FName, FVehicleCargo>& Truck : Vehicles)
	{
		if (const FInventoryEntry* Entry = Truck.Value.Entries.Find(ProductID))
		{
			Total += Entry->PhysicalStock;
		}
	}
	return Total;
}

int32 UFleetSubsystem::GetTotalLooseBoxes(FName ProductID) const
{
	int32 Total = 0;
	for (const TPair<FName, FVehicleCargo>& Truck : Vehicles)
	{
		if (const FInventoryEntry* Entry = Truck.Value.Entries.Find(ProductID))
		{
			Total += Entry->PhysicalLooseBoxes;
		}
	}
	return Total;
}

float UFleetSubsystem::GetTotalContainers(FName ProductID) const
{
	return GetTotalWholeContainers(ProductID)
		+ static_cast<float>(GetTotalLooseBoxes(ProductID)) / FInventoryEntry::BoxesPerPallet;
}

TArray<FName> UFleetSubsystem::GetAllCarriedProductIDs() const
{
	TSet<FName> Seen;
	for (const TPair<FName, FVehicleCargo>& Truck : Vehicles)
	{
		for (const TPair<FName, FInventoryEntry>& Pair : Truck.Value.Entries)
		{
			if (!Pair.Value.IsEmpty())
			{
				Seen.Add(Pair.Key);
			}
		}
	}

	TArray<FName> Result = Seen.Array();
	Result.Sort(FNameLexicalLess());
	return Result;
}

double UFleetSubsystem::GetOldestAcquiredMinute(FName VehicleKey, FName ProductID,
	bool bLoose) const
{
	const double Now = NowMinute();
	const FVehicleCargo* Cargo = Vehicles.Find(VehicleKey);
	const FInventoryEntry* Entry = Cargo ? Cargo->Entries.Find(ProductID) : nullptr;
	if (!Entry)
	{
		return Now;
	}
	return bLoose ? Entry->OldestLooseBoxMinute(Now) : Entry->OldestContainerMinute(Now);
}

void UFleetSubsystem::RefreshCondition()
{
	const double Now = NowMinute();
	const int32 Today = FMath::FloorToInt(Now / DeadlineCondition::MinutesPerDay);
	if (Today == LastSweptDay)
	{
		return;
	}
	LastSweptDay = Today;

	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	if (!Catalogue)
	{
		return;
	}

	struct FWriteOff
	{
		FName VehicleKey;
		FName ProductID;
		int32 Containers = 0;
		bool bOnTheBooks = false;
	};
	TArray<FWriteOff> WriteOffs;

	for (TPair<FName, FVehicleCargo>& VehiclePair : Vehicles)
	{
		TArray<FName> Emptied;
		for (TPair<FName, FInventoryEntry>& Pair : VehiclePair.Value.Entries)
		{
			const FProductRow* Row = Catalogue->FindProduct(Pair.Key);
			if (!Row || Row->ShelfLifeDays <= 0)
			{
				continue;
			}

			const float RecordedBefore = Pair.Value.RecordedContainers();

			int32 LostContainers = 0;
			int32 LostBoxes = 0;
			if (!Pair.Value.ExpireSpoiled(Row->ShelfLifeDays, Now, LostContainers, LostBoxes))
			{
				continue;
			}

			FWriteOff Loss;
			Loss.VehicleKey = VehiclePair.Key;
			Loss.ProductID = Pair.Key;
			Loss.Containers = LostContainers
				+ FMath::DivideAndRoundUp(LostBoxes, FInventoryEntry::BoxesPerPallet);
			Loss.bOnTheBooks = Pair.Value.RecordedContainers() < RecordedBefore - KINDA_SMALL_NUMBER;
			WriteOffs.Add(Loss);

			if (Pair.Value.IsEmpty())
			{
				Emptied.Add(Pair.Key);
			}
		}

		for (const FName& ID : Emptied)
		{
			VehiclePair.Value.Entries.Remove(ID);
		}
	}

	for (const FWriteOff& Loss : WriteOffs)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline] %s: %d container(s) of %s spoiled in transit."),
			*Loss.VehicleKey.ToString(), Loss.Containers, *Loss.ProductID.ToString());

		OnCargoSpoiled.Broadcast(Loss.ProductID, Loss.Containers, Loss.bOnTheBooks);
		OnCargoChanged.Broadcast(Loss.VehicleKey);
	}
}

float UFleetSubsystem::GetConditionMultiplier(FName ProductID) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (!Row || Row->ObsolescencePerDay <= 0.f)
	{
		return 1.f;
	}

	const double Now = NowMinute();
	double Weight = 0.0;
	double AgeSum = 0.0;
	for (const TPair<FName, FVehicleCargo>& VehiclePair : Vehicles)
	{
		const FInventoryEntry* Entry = VehiclePair.Value.Entries.Find(ProductID);
		if (!Entry)
		{
			continue;
		}
		const double Quantity = Entry->PhysicalContainers();
		Weight += Quantity;
		AgeSum += Quantity * Entry->AverageAgeDays(Now);
	}

	if (Weight <= 0.0)
	{
		return 1.f;
	}
	return DeadlineCondition::ObsolescenceMultiplier(Row->ObsolescencePerDay,
		AgeSum / Weight, UDeadlineSettings::Get().ObsolescenceFloor);
}

void UFleetSubsystem::ResetAll()
{
	TArray<FName> Keys;
	Vehicles.GetKeys(Keys);

	// Keep the roster, empty the holds: the trucks are still parked where they
	// were, they are just no longer carrying last game's goods.
	for (const FName& Key : Keys)
	{
		Vehicles[Key].Entries.Empty();
		OnCargoChanged.Broadcast(Key);
	}
	LastSweptDay = -1;
}

void UFleetSubsystem::RestoreCargo(FName VehicleKey, const FVehicleCargo& Cargo)
{
	FVehicleCargo& Stored = Vehicles.Add(VehicleKey, Cargo);

	// A manifest saved before batches existed (v4 and earlier) has counts and
	// no ages. Give it one rather than rotting the load on sight.
	const double Now = NowMinute();
	for (TPair<FName, FInventoryEntry>& Pair : Stored.Entries)
	{
		Pair.Value.SyncBatches(Now);
	}
	LastSweptDay = -1;
}
