// Copyright DEADLINE. All Rights Reserved.
//
// The forecast board (GDD 4 step 2, GDD 17, roadmap Month 4): the one screen
// where a signal turns into a decision. The Month 4 gate test is "the forecast
// decision is understood in 30 seconds", so every entry answers the same four
// questions in the same order:
//
//   What might happen, and how sure is the source?      (name, confidence)
//   When?                                                (expected day)
//   How big, if it does?                                 (the table's range)
//   What does it mean for me?                            (exposure, gain range)
//
// Exposure (maruziyet, GDD 8.2) is the value of the goods you already hold in
// the products the event would push. GDD 8.2 also counts contract penalties,
// debt falling due and seizure risk; none of those systems exist yet, and each
// joins the sum when it lands.
//
// Everything here is built from the truth-free views of UEventSubsystem. The
// impact range is the event's template range from DT_Events, never the value
// the calendar rolled, so the board cannot give the forecast away.
//
// Commitments (amount + budget lock + duration) are the next roadmap item and
// will live in this subsystem too.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForecastSubsystem.generated.h"

class UEconomySubsystem;
class UEventSubsystem;
class UMarketSubsystem;
class UProductCatalogSubsystem;
class UTimeSubsystem;

UENUM(BlueprintType)
enum class EForecastStatus : uint8
{
	/** A rumour or forecast: it may or may not happen. */
	Signal,
	/** Happening now. */
	Active
};

/** One product an entry would push, and your stake in it. */
USTRUCT(BlueprintType)
struct FForecastProductLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	FName ProductID;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	FString Name;

	/** Today's market price for one container. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float Price = 0.f;

	/** Signals: 0. Active events: change since the day before it began, %. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float ChangePercent = 0.f;

	/** Containers you hold, warehouse and trucks. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float HeldContainers = 0.f;

	/** What they would fetch today. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float HeldValue = 0.f;
};

USTRUCT(BlueprintType)
struct FForecastEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	FName EventID;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	EForecastStatus Status = EForecastStatus::Signal;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	FString Name;

	/** The rumour, or the headline once it is happening. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	FString Text;

	/** Signals: the source's confidence, 0..1. Active events: 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float Confidence = 0.f;

	/** Signals: the expected day. Active events: the last day it runs. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 Day = 0;

	/** Days from today to Day. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 DaysAway = 0;

	/** Price push if it happens, as fractions of base: the template range. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float ImpactMin = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float ImpactMax = 0.f;

	/** > 1 when trips get dearer. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float RouteCostMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	TArray<FForecastProductLine> Products;

	/** Value of the goods you hold that it would push. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float Exposure = 0.f;

	/** Exposure x ImpactMin / ImpactMax: what holding on is worth if it
	    happens. Not weighted by confidence -- the board shows the stake and
	    the odds side by side and leaves the bet to the player. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float GainIfHappensMin = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float GainIfHappensMax = 0.f;
};

/** The board changed: new day, new bulletin, or your stock moved. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnForecastBoardChanged);

UCLASS()
class DEADLINE__API UForecastSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Forecast")
	FOnForecastBoardChanged OnBoardChanged;

	/** Today's board: open signals first (soonest first), then events
	    running. Entries are built fresh on every call; nothing is cached. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Forecast")
	TArray<FForecastEntry> GetBoard() const;

	/** Sum of exposure across the board, each product counted once. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Forecast")
	float GetTotalExposure() const;

private:
	void FillProducts(FForecastEntry& Entry, const TArray<FName>& ProductIDs, int32 EventStartDay) const;

	UFUNCTION()
	void HandleDayChanged(int32 NewDay);

	UFUNCTION()
	void HandleStockChanged(FName ProductID);

	UEventSubsystem* GetEvents() const;
	UEconomySubsystem* GetEconomy() const;
	UMarketSubsystem* GetMarket() const;
	UProductCatalogSubsystem* GetCatalogue() const;
	UTimeSubsystem* GetTime() const;
};
