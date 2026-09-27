// Copyright DEADLINE. All Rights Reserved.
//
// Every product's price. GDD 8.1, implemented the lazy way CLAUDE.md asks for:
// nothing ticks, nothing is recomputed on a timer. A product's price is only
// brought up to date when someone actually asks for it, and the arithmetic is
// a pure function of (run seed, product, day) so a product nobody has looked
// at since day 3 lands on exactly the same number on day 200 as one that has
// been watched every day.
//
// The maths itself lives in MarketModel.h so it can be tested without a game.

#pragma once

#include "CoreMinimal.h"
#include "Data/ProductRow.h"
#include "Math/RandomStream.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MarketSubsystem.generated.h"

class UProductCatalogSubsystem;
class USaveSubsystem;
class UTimeSubsystem;

/** Which way today's price moved against yesterday's. Drives the HUD arrow. */
UENUM(BlueprintType)
enum class EPriceTrend : uint8
{
	Flat,
	Rising,
	Falling
};

/** Everything a market row needs, in one read. */
USTRUCT(BlueprintType)
struct FMarketQuote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Market")
	FName ProductID;

	/** Today's price for one container. */
	UPROPERTY(BlueprintReadOnly, Category = "Market")
	float Price = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Market")
	float BasePrice = 0.f;

	/** Price / BasePrice. The number the risk bands are expressed in. */
	UPROPERTY(BlueprintReadOnly, Category = "Market")
	float Multiple = 1.f;

	/** Percent change against yesterday's close. */
	UPROPERTY(BlueprintReadOnly, Category = "Market")
	float DayChangePercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Market")
	EPriceTrend Trend = EPriceTrend::Flat;

	UPROPERTY(BlueprintReadOnly, Category = "Market")
	ERiskBand RiskBand = ERiskBand::Stable;
};

/** Fired once per game day, after prices have moved. The UI subscribes; it
    must not poll (CLAUDE.md tick rules, method 2). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarketDayAdvanced, int32, NewDay);

UCLASS()
class DEADLINE__API UMarketSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Market")
	FOnMarketDayAdvanced OnMarketDayAdvanced;

	// --- Prices ------------------------------------------------------------

	/** Today's quoted market price for one container. 0 if the ID is unknown. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	float GetPrice(FName ProductID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	float GetBasePrice(FName ProductID) const;

	/** Price as a multiple of base. 1.0 = normal, 2.0 = double. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	float GetPriceMultiple(FName ProductID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	FMarketQuote GetQuote(FName ProductID) const;

	/** Quotes for every product in the catalogue, for the market screen. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	TArray<FMarketQuote> GetAllQuotes() const;

	/** Price on a past day. Days beyond today are not simulated ahead; asking
	    for one returns today's price. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	float GetPriceOnDay(FName ProductID, int32 Day) const;

	/** The last MaxDays closes, oldest first, for the price history chart
	    (GDD 17). Shorter than MaxDays early in a run. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	TArray<float> GetPriceHistory(FName ProductID, int32 MaxDays = 30) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	EPriceTrend GetTrend(FName ProductID) const;

	/** The day the market has caught up to: the clock's day. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	int32 GetMarketDay() const;

	/**
	 * Simulate a product forward from day 0 without touching live state.
	 * Dev/test only -- the parity test and Dl_MarketDump use it to inspect a
	 * whole series without playing 200 days.
	 */
	TArray<double> PeekSeries(FName ProductID, int32 Days) const;

	/** Drop all cached prices. Called on new game and on load, where the run
	    seed changes and every series has to be rebuilt from it. */
	void ResetAll();

private:
	/** One product's live state. Not a UPROPERTY: plain data, no UObjects. */
	struct FProductMarketState
	{
		double Price = 0.0;
		/** The price the model pulls back toward. Equal to BasePrice for now;
		    it becomes a moving target when contracts and reputation land. */
		double NormalPrice = 0.0;
		double BasePrice = 0.0;
		ERiskBand Band = ERiskBand::Stable;
		int32 LastDay = 0;
		FRandomStream Stream;
		/** History[d] is the close on day d. Filled in as days are caught up. */
		TArray<float> History;
	};

	/** Get the state for a product, creating and seeding it on first use.
	    Does NOT bring it up to date -- CatchUp does that. */
	FProductMarketState* FindOrCreateState(FName ProductID) const;

	/** Walk a product forward to TargetDay, one modelled day per step. */
	void CatchUp(FProductMarketState& State, int32 TargetDay) const;

	/** State for a product, brought up to today. The one entry point every
	    public query goes through. */
	FProductMarketState* GetCurrentState(FName ProductID) const;

	UFUNCTION()
	void HandleDayChanged(int32 NewDay);

	/** A CSV reload can change BasePrice and RiskBand, which every cached
	    series was built from. Throw the lot away and rebuild on next read. */
	UFUNCTION()
	void HandleCatalogueReloaded();

	UProductCatalogSubsystem* GetCatalogue() const;
	UTimeSubsystem* GetTime() const;
	int32 GetRunSeed() const;

	/** Lazy evaluation writes to the cache from const query functions, which
	    is exactly what mutable is for: the observable price never depends on
	    whether the cache was warm. */
	mutable TMap<FName, FProductMarketState> States;

	/** The seed the cached states were built from. If the run seed changes
	    under us (new game, load) the cache is stale and gets thrown away. */
	mutable int32 CachedSeed = 0;
	mutable bool bSeedCached = false;
};
