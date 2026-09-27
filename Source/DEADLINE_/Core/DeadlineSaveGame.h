// Copyright DEADLINE. All Rights Reserved.
//
// Versioned save payload. CLAUDE.md mistake #8: never ship a save struct
// without a version number. Bump SaveVersion whenever a field is added or its
// meaning changes, and handle the old value in USaveSubsystem::Migrate.

#pragma once

#include "CoreMinimal.h"
#include "Economy/EconomySubsystem.h"
#include "Inventory/InventorySubsystem.h"
#include "GameFramework/SaveGame.h"
#include "DeadlineSaveGame.generated.h"

USTRUCT()
struct FInventorySaveEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FName ProductID;

	UPROPERTY()
	int32 PhysicalStock = 0;

	UPROPERTY()
	int32 RecordedStock = 0;

	/** v2: boxes off an opened pallet (GDD 10.2). */
	UPROPERTY()
	int32 PhysicalLooseBoxes = 0;

	UPROPERTY()
	int32 RecordedLooseBoxes = 0;

	/** v5: when these goods arrived, split by acquisition (GDD 5.1). Without
	    it a reload would hand every crate the same birthday and a warehouse of
	    perishables would spoil in one lump. An empty list on an older save is
	    handled by FInventoryEntry::SyncBatches. */
	UPROPERTY()
	TArray<FStockBatch> Batches;
};

/** One truck's manifest. */
USTRUCT()
struct FVehicleCargoSaveEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FName VehicleKey;

	UPROPERTY()
	FName VehicleID;

	UPROPERTY()
	TArray<FInventorySaveEntry> Cargo;
};

UCLASS()
class DEADLINE__API UDeadlineSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Bump this on every layout change. See USaveSubsystem::Migrate.
	    v2 — storage split into the four GDD 11 classes, and pallets that can
	         be opened into loose boxes.
	    v3 — vehicle manifests: goods in a truck are still your goods.
	    v4 — which destination the player is standing at.
	    v5 — how old the goods are, so shelf life and obsolescence survive a
	         reload (GDD 5.1).
	    v6 — spare wooden pallets (GDD 5.6). */
	static constexpr int32 LatestVersion = 6;

	UPROPERTY()
	int32 SaveVersion = LatestVersion;

	/** Master seed for the run. Every subsystem stream derives from it, so a
	    reload reproduces the same market story (CLAUDE.md mistake #4). */
	UPROPERTY()
	int32 GameSeed = 0;

	UPROPERTY()
	double TotalGameMinutes = 0.0;

	UPROPERTY()
	float Cash = 0.f;

	UPROPERTY()
	float Bank = 0.f;

	/** v1 only: the single pooled capacity the warehouse used to have. Kept so
	    a v1 save still parses; v2 reads StorageUnitCounts instead. */
	UPROPERTY()
	float WarehouseCapacityBU = 0.f;

	/** v2: racks / pallet bays / cold zones / secure cages, indexed by
	    EStorageClass. This is what a warehouse upgrade will move. */
	UPROPERTY()
	TArray<int32> StorageUnitCounts;

	UPROPERTY()
	TArray<FInventorySaveEntry> Inventory;

	/** v6: empty pallets on hand. */
	UPROPERTY()
	int32 EmptyPallets = 0;

	UPROPERTY()
	TArray<FTransactionRecord> Transactions;

	/** v3: what every truck is carrying. */
	UPROPERTY()
	TArray<FVehicleCargoSaveEntry> Fleet;

	/** v4: row key into DT_Destinations. */
	UPROPERTY()
	FName CurrentLocationID;

	UPROPERTY()
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator PlayerRotation = FRotator::ZeroRotator;
};
