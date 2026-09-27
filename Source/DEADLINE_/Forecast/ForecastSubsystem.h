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
// Commitments (GDD 4 step 2: "picks a product, says it will rise, sets the
// amount and the budget"). A commitment is made on an open signal, for one of
// the products it would push:
//
//   amount    containers you mean to buy before it lands;
//   budget    amount x today's buy price, locked in UEconomySubsystem until
//             the expected day -- the money is still yours, but only that
//             product can spend it; what is left comes back on the day;
//   duration  how many days after the expected day you hold before the
//             forecast is judged.
//
// On the resolve day the commitment is judged against what actually happened
// (the event started on its day or it did not -- public by then, never the
// calendar's truth ahead of time) and against what the goods bought for it are
// worth: sales since the commitment plus today's value of the rest, less what
// they cost. The result card and the notebook (next roadmap items) read it.

#pragma once

#include "CoreMinimal.h"
#include "Economy/EconomySubsystem.h"
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

	/** Your live commitment on this signal, or INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 CommitmentID = INDEX_NONE;
};

/** The one reason a result came out the way it did (GDD 4 step 6: the
    "neden yanıldım" card). Picked by UForecastSubsystem when it judges, from
    what is public by then; the result card and the notebook put it in words. */
UENUM(BlueprintType)
enum class ECommitmentLesson : uint8
{
	None,
	/** It happened and you made money. */
	GoodCall,
	/** It happened, but you bought nothing: the gain you walked past. */
	MissedIt,
	/** It happened, but the rise never covered the buy/sell spread. */
	EatenBySpread,
	/** It happened and the rise was enough, but you held past the peak. */
	HeldTooLong,
	/** The rumour was false and it cost you. Odds, not a mistake. */
	FalseRumour,
	/** The rumour was false and you had bought nothing: no harm done. */
	FalseRumourSpared,
	/** The rumour was false, yet the price went your way anyway. */
	LuckyWin
};

UENUM(BlueprintType)
enum class ECommitmentState : uint8
{
	/** Before the expected day: the budget is locked, buy against it. */
	Open,
	/** Expected day passed, lock released: holding until the resolve day. */
	Holding,
	/** Judged. */
	Resolved,
	/** Called off before the expected day. Not judged. */
	Cancelled
};

USTRUCT(BlueprintType)
struct FForecastCommitment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 ID = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	FName EventID;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	FName ProductID;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	ECommitmentState State = ECommitmentState::Open;

	/** What the source said when you committed. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float Confidence = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 CommitDay = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 ExpectedDay = 0;

	/** Days held after ExpectedDay. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 HoldDays = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 TargetContainers = 0;

	/** Locked at commit: TargetContainers x BuyPriceAtCommit. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float Budget = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float BuyPriceAtCommit = 0.f;

	/** Bought since committing, up to the expected day. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 BoughtContainers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float Spent = 0.f;

	/** Sold since committing, capped at what was bought for it. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 SoldContainers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float Proceeds = 0.f;

	/** Still locked. Mirrors UEconomySubsystem while Open; kept here so a save
	    can put the lock back. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float LockRemaining = 0.f;

	// --- Filled in when judged ---------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	bool bEventHappened = false;

	/** Market price of one container on the resolve day. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float PriceAtResolve = 0.f;

	/** Proceeds + value of the unsold rest - Spent. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float Result = 0.f;

	/** Market price on the day committed, before any mark-up. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float MarketPriceAtCommit = 0.f;

	/** First day the event was over, or INDEX_NONE if it never ran. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 EventEndDay = INDEX_NONE;

	/** Best day to have sold, expected day to resolve day, and what a
	    buyer would have paid for one container on it. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	int32 PeakDay = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float PeakSellPrice = 0.f;

	/** Highest market price in the window over MarketPriceAtCommit, - 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float PeakRise = 0.f;

	/** The rise a buy-and-sell needs just to break even, from the frictions. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float BreakEvenRise = 0.f;

	/** Selling everything at the peak: with what you bought, or, if you
	    bought nothing, with the amount you committed to. */
	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	float BestResult = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Forecast")
	ECommitmentLesson Lesson = ECommitmentLesson::None;

	int32 GetResolveDay() const { return ExpectedDay + HoldDays; }
	bool IsLive() const { return State == ECommitmentState::Open || State == ECommitmentState::Holding; }
};

/** Why a commitment was refused, for the panel to say. */
UENUM(BlueprintType)
enum class ECommitRefusal : uint8
{
	None,
	/** Not an open signal today, or the product is not one it pushes. */
	NotOnBoard,
	/** You already have a live commitment on this product. */
	AlreadyCommitted,
	BadAmount,
	/** The unlocked funds do not cover the budget. */
	NotEnoughFunds
};

/** The board changed: new day, new bulletin, or your stock moved. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnForecastBoardChanged);

/** A commitment was judged: the hook for the result card (GDD 4 step 6). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCommitmentResolved, const FForecastCommitment&, Commitment);

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

	// --- Commitments -------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Forecast")
	FOnCommitmentResolved OnCommitmentResolved;

	/** Would Commit() accept this? Also what the panel asks before enabling
	    the button. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Forecast")
	ECommitRefusal CanCommit(FName EventID, FName ProductID, int32 Containers) const;

	/** Commit on today's open signal EventID: lock Containers x today's buy
	    price of ProductID, judge HoldDays after the expected day. Returns the
	    new commitment's ID, or INDEX_NONE (see CanCommit). */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	int32 Commit(FName EventID, FName ProductID, int32 Containers, int32 HoldDays);

	/** Call off an Open commitment: the lock comes back, nothing is judged. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	bool CancelCommitment(int32 CommitmentID);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Deadline|Forecast")
	bool GetCommitment(int32 CommitmentID, FForecastCommitment& Out) const;

	/** Every commitment this run, oldest first: the notebook's source. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Forecast")
	const TArray<FForecastCommitment>& GetCommitments() const { return Commitments; }

	/** Most containers the unlocked funds buy at today's price. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Forecast")
	int32 GetMaxAffordable(FName ProductID) const;

	/** New game. */
	void ResetAll();

	/** Load: put the commitments back and re-lock the open budgets. Call
	    after the economy has its funds back. */
	void RestoreCommitments(const TArray<FForecastCommitment>& InCommitments);

private:
	const FForecastCommitment* FindLive(FName ProductID) const;
	FForecastCommitment* FindByID(int32 CommitmentID);
	void CloseLock(FForecastCommitment& C);
	void Judge(FForecastCommitment& C);

public:
	/** Pick the lesson from a judged commitment's numbers. Pure, for tests. */
	static ECommitmentLesson PickLesson(const FForecastCommitment& C);

private:
	void AdvanceCommitments(int32 Today);

	UFUNCTION()
	void HandleTransaction(const FTransactionRecord& Record);

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

	TArray<FForecastCommitment> Commitments;
	int32 NextCommitmentID = 1;
};
