// Copyright DEADLINE. All Rights Reserved.
//
// What is in the trucks. The fleet's cargo is DATA, exactly like the warehouse
// ledger (CLAUDE.md mistake #2) — AVehicleActor draws it, it does not own it.
//
// Why not put the cargo on the actor: goods in a truck are still your goods.
// The company valuation, the save file and the inspection all have to be able
// to see them without walking the level, and a save has to survive the level
// being reloaded. So the cargo lives here, keyed by the vehicle's own key, and
// the actor in the world is a view of it.
//
// Cargo uses the same FInventoryEntry as the warehouse, so the two-ledger split
// (GDD 7.1) rides along: a grey box loaded onto a truck is still grey, and a
// pallet broken into loose boxes can be carried aboard one box at a time
// (GDD 10.2).

#pragma once

#include "CoreMinimal.h"
#include "Data/VehicleRow.h"
#include "Inventory/InventorySubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FleetSubsystem.generated.h"

class UDataTable;
class UProductCatalogSubsystem;

/** One vehicle's hold. */
USTRUCT(BlueprintType)
struct FVehicleCargo
{
	GENERATED_BODY()

	/** Row key into DT_Vehicles: what kind of vehicle this is. */
	UPROPERTY(BlueprintReadOnly, Category = "Fleet")
	FName VehicleID;

	/** Product -> containers aboard, both ledgers, loose boxes included. */
	UPROPERTY(BlueprintReadOnly, Category = "Fleet")
	TMap<FName, FInventoryEntry> Entries;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCargoChanged, FName, VehicleKey);

UCLASS()
class DEADLINE__API UFleetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Fleet")
	FOnCargoChanged OnCargoChanged;

	/** Goods rotted in the hold. Same shape and same listener as the
	    warehouse's OnStockSpoiled: a crate of fish does not care which side of
	    the loading bay it was standing on. */
	UPROPERTY(BlueprintAssignable, Category = "Deadline|Fleet")
	FOnStockSpoiled OnCargoSpoiled;

	// --- Fleet roster -------------------------------------------------------

	/** Called by a vehicle actor as it comes up. Keeps whatever cargo the key
	    already had, so driving into a level twice does not empty the truck. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Fleet")
	void RegisterVehicle(FName VehicleKey, FName VehicleID);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	bool HasVehicle(FName VehicleKey) const { return Vehicles.Contains(VehicleKey); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	TArray<FName> GetVehicleKeys() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	FName GetVehicleID(FName VehicleKey) const;

	/** DT_Vehicles row for a vehicle type, or nullptr if the ID is unknown. */
	const FVehicleRow* FindVehicleRow(FName VehicleID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Deadline|Fleet")
	bool GetVehicleRow(FName VehicleID, FVehicleRow& OutRow) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	TArray<FName> GetAllVehicleIDs() const;

	// --- Loading ------------------------------------------------------------

	/**
	 * Can this vehicle take that much of this product?
	 * @param bLoose  true for single boxes off an opened pallet.
	 * @param OutReason  player-facing refusal, empty when the answer is yes.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Deadline|Fleet")
	bool CanLoad(FName VehicleKey, FName ProductID, int32 Count, bool bLoose, FText& OutReason) const;

	/** Put goods aboard. Same white/grey question as the warehouse ledger. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Fleet")
	bool Load(FName VehicleKey, FName ProductID, int32 Count, bool bRecorded, bool bLoose);

	/**
	 * Load goods that already have a history, keeping their age.
	 *
	 * A truck is not a fridge that stops time. Without this, moving stock into
	 * the hold and back out would be a free way to renew a shelf life, and the
	 * cheapest strategy in the game would be to park perishables in a van.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Fleet")
	bool LoadAged(FName VehicleKey, FName ProductID, int32 Count, bool bRecorded, bool bLoose,
		double AcquiredAtMinute);

	/** Take goods back off. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Fleet")
	bool Unload(FName VehicleKey, FName ProductID, int32 Count, bool bRecorded, bool bLoose);

	/** First product in the hold, for an empty-handed unload. Reports whether
	    it comes off as a loose box. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Deadline|Fleet")
	FName PickUnloadProduct(FName VehicleKey, bool& bOutLoose) const;

	/** Clock reading of the oldest goods of this product in the hold, so a box
	    coming off a truck keeps the age it went on with. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	double GetOldestAcquiredMinute(FName VehicleKey, FName ProductID, bool bLoose) const;

	// --- Condition (GDD 5.1) -------------------------------------------------

	/** Throw out cargo past its shelf life. Runs on the day rollover, exactly
	    like the warehouse sweep, and costs nothing on a day already swept. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Fleet")
	void RefreshCondition();

	/** Obsolescence multiplier for this product across the whole fleet, 1.0 if
	    nobody is carrying any. Valued separately from the warehouse: a truck
	    loaded a month ago is not worth what the racks are. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	float GetConditionMultiplier(FName ProductID) const;

	// --- Queries ------------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	float GetCapacityBU(FName VehicleKey) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	float GetUsedBU(FName VehicleKey) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	float GetFreeBU(FName VehicleKey) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	float GetWeightKg(FName VehicleKey) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	float GetMaxWeightKg(FName VehicleKey) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	bool IsEmpty(FName VehicleKey) const;

	/** Every container aboard, in a stable order, for the cargo stack. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	TArray<FStoredContainer> GetCargoContents(FName VehicleKey) const;

	// --- Fleet-wide totals --------------------------------------------------
	// Goods in transit are still your goods, so the valuation and the profit
	// and loss have to be able to see them without walking the level.

	/** Whole containers of this product across every truck. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	int32 GetTotalWholeContainers(FName ProductID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	int32 GetTotalLooseBoxes(FName ProductID) const;

	/** Whole containers plus the pallet-fraction the loose boxes add up to. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	float GetTotalContainers(FName ProductID) const;

	/** Every product ID carried by any truck. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Fleet")
	TArray<FName> GetAllCarriedProductIDs() const;

	const TMap<FName, FVehicleCargo>& GetAllCargo() const { return Vehicles; }

	// --- Save / load --------------------------------------------------------

	void ResetAll();
	void RestoreCargo(FName VehicleKey, const FVehicleCargo& Cargo);

private:
	UProductCatalogSubsystem* GetCatalogue() const;

	double NowMinute() const;

	UFUNCTION()
	void HandleDayChanged(int32 NewDay);

	float VolumeBUOf(FName ProductID) const;
	float LooseBoxVolumeBUOf(FName ProductID) const;
	float WeightKgOf(FName ProductID) const;

	/** Load DT_Vehicles from the project settings, once. */
	void EnsureTable() const;

	UPROPERTY()
	TMap<FName, FVehicleCargo> Vehicles;

	UPROPERTY(Transient)
	mutable TObjectPtr<UDataTable> VehicleTable;

	/** Game day the cargo sweep last ran on. */
	int32 LastSweptDay = -1;
};
