// Copyright DEADLINE. All Rights Reserved.
//
// Reads DT_Products once and answers product lookups. Every other system asks
// this for volume, price and licence data instead of touching the DataTable.
//
// ReloadFromCSV re-reads the source CSV into the live table while the game is
// running, so balance numbers can be changed and seen without a restart
// (roadmap Month 2). It edits the in-memory table only: to make a change
// stick, re-import the asset in the editor -- see
// Content/Deadline/Data/import_products_datatable.py.

#pragma once

#include "CoreMinimal.h"
#include "Data/ProductRow.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProductCatalogSubsystem.generated.h"

class UDataTable;

/** Fired after the catalogue is re-read from CSV. Anything caching product
    numbers (prices, capacities) must drop that cache. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCatalogueReloaded);

UCLASS()
class DEADLINE__API UProductCatalogSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Catalogue")
	FOnCatalogueReloaded OnCatalogueReloaded;

	/**
	 * Re-read the product CSV into the live DataTable.
	 *
	 * @param CsvPath  Absolute path, or empty for the default
	 *                 Content/Deadline/Data/DT_Products.csv.
	 * @param OutProblems  Per-row complaints from the importer. A non-empty
	 *                 list does not mean nothing loaded: bad rows are skipped.
	 * @return false only if the file could not be read at all.
	 */
	bool ReloadFromCSV(const FString& CsvPath, TArray<FString>& OutProblems);

	/** Where ReloadFromCSV looks when given no path. */
	static FString DefaultCSVPath();

	/** Row for a product ID, or nullptr if the ID is unknown. */
	const FProductRow* FindProduct(FName ProductID) const;

	/** Blueprint-friendly lookup. Returns false if the ID is unknown. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Deadline|Catalogue")
	bool GetProduct(FName ProductID, FProductRow& OutProduct) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Catalogue")
	bool IsValidProduct(FName ProductID) const { return FindProduct(ProductID) != nullptr; }

	/** Every product ID in the catalogue. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Catalogue")
	TArray<FName> GetAllProductIDs() const;

	/** Product IDs whose LaunchPhase matches, e.g. "EA". */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Catalogue")
	TArray<FName> GetProductIDsForPhase(FName LaunchPhase) const;

	/**
	 * The name to show a player, in their language.
	 *
	 * DT_Products carries NameTR and NameEN side by side and the launch plan
	 * lists four languages, so picking one here rather than at every call site
	 * is the only place this decision belongs. Chinese and Russian columns join
	 * the CSV when those translations exist; until then they fall back to
	 * English, which is what a missing translation should do.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Catalogue")
	FString GetDisplayName(FName ProductID) const;

	/** Box Units one container of this product takes up. 0 if unknown. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Catalogue")
	float GetVolumeBU(FName ProductID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Catalogue")
	int32 GetProductCount() const { return CachedRows.Num(); }

private:
	/** Point CachedRows at the table's current rows. */
	void RebuildCache();

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ProductTable;

	/** ProductID -> row, built once at Initialize. */
	TMap<FName, FProductRow*> CachedRows;
};
