// Copyright DEADLINE. All Rights Reserved.
//
// Money and trades. Cash vs Bank is a real split (GDD 6.3): grey business is
// cash only, and cash found in the warehouse is evidence during an inspection.
//
// Prices come from UMarketSubsystem (GDD 8.1). This class only adds the
// frictions on top: the counter mark-up you pay and the discount a buyer
// takes, which is why buying and immediately reselling loses money on a flat
// market. Profit has to come from the price actually moving.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EconomySubsystem.generated.h"

class UInventorySubsystem;
class UMarketSubsystem;
class UProductCatalogSubsystem;

/** Which ledger a trade touches. Forces every call site to answer the question. */
UENUM(BlueprintType)
enum class ETradeLedger : uint8
{
	/** Invoiced. Physical and recorded stock both move. Bank or cash. */
	White,
	/** Off the books. Only physical stock moves. Cash only (GDD 6.3). */
	Grey
};

UENUM(BlueprintType)
enum class EPaymentMethod : uint8
{
	Cash,
	Bank
};

USTRUCT(BlueprintType)
struct FTransactionRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	FName ProductID;

	/** Positive = bought, negative = sold. In containers. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 Containers = 0;

	/** Total money moved, including frictions. Positive = money in. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	float Amount = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	ETradeLedger Ledger = ETradeLedger::White;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	EPaymentMethod Payment = EPaymentMethod::Bank;

	/** In-game day the trade happened on. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 GameDay = 0;
};

/** Profit and loss for one product, worked out from the trade log.
    Nothing here is stored: it is all replayed from Transactions, so it cannot
    drift out of step with the trades that produced it and needs no save
    migration. */
USTRUCT(BlueprintType)
struct FProductPnL
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	FName ProductID;

	/** Whole containers actually in the warehouse right now. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 ContainersHeld = 0;

	/** Boxes off opened pallets, worth a sixteenth of a container each
	    (GDD 10.2). They carry value, so the market value below counts them. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 LooseBoxesHeld = 0;

	/** Whole containers of this product riding in a truck. Counted in the
	    market value: goods in transit have not stopped being yours. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 ContainersInTransit = 0;

	/** Weighted average price paid per container still held. This is the
	    "your average cost" line on the market screen's price chart (GDD 17). */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	float AverageUnitCost = 0.f;

	/** Profit already banked on containers that have been sold. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	float RealisedProfit = 0.f;

	/** What the held stock would fetch if sold at today's price. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	float MarketValue = 0.f;

	/** Paper profit on stock still held: MarketValue minus what it cost. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	float UnrealisedProfit = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 TradeCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFundsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransactionRecorded, const FTransactionRecord&, Record);

UCLASS()
class DEADLINE__API UEconomySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Economy")
	FOnFundsChanged OnFundsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Economy")
	FOnTransactionRecorded OnTransactionRecorded;

	// --- Funds -------------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetCash() const { return Cash; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetBank() const { return Bank; }

	/** Cash + bank. Full company value (GDD 8.2) arrives with the market model. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetTotalFunds() const { return Cash + Bank; }

	UFUNCTION(BlueprintCallable, Category = "Deadline|Economy")
	void AddCash(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Deadline|Economy")
	void AddBank(float Amount);

	/** Move money bank -> cash. Withdrawals over the daily threshold will add
	    Heat once UComplianceSubsystem exists (GDD 6.3, Month 7). */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Economy")
	bool Withdraw(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Deadline|Economy")
	bool Deposit(float Amount);

	// --- Budget locks (forecast commitments, roadmap Month 4) ---------------
	//
	// A lock is money set aside for buying one product. It stays in your
	// accounts -- nothing moves -- but TryBuy will not spend it on anything
	// else, and buying that product draws on it first. Only trading honours
	// locks: fuel and other running costs must never strand you because of a
	// forecast.

	/** Set aside Amount for ProductID. False if the unlocked funds are short. */
	bool LockFunds(FName ProductID, float Amount);

	/** Drop ProductID's lock. Returns what was still locked. */
	float ReleaseLock(FName ProductID);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetLockedFunds() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetLockedFundsFor(FName ProductID) const;

	/** Cash + bank minus every lock: what you can spend freely. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetAvailableFunds() const { return Cash + Bank - GetLockedFunds(); }

	// --- Prices ------------------------------------------------------------

	/** Today's quoted market price for one container (UMarketSubsystem). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetMarketPrice(FName ProductID) const;

	/** What you actually pay per container at a counter (market + mark-up). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetBuyPrice(FName ProductID) const;

	/** What a buyer actually pays you per container (market - discount).
	    This is the QUOTE: brand-new goods, nothing held against them. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetSellPrice(FName ProductID) const;

	/**
	 * What a buyer pays for the goods YOU are holding, after obsolescence
	 * (GDD 5.1). Same market, older stock, less money.
	 *
	 * The quote above stays a pure function of seed, product and day, which is
	 * what the parity test checks; the discount for age is a property of your
	 * warehouse and lives here instead.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetSellPriceForHeldStock(FName ProductID) const;

	/** 0..1 value the goods on your shelves have kept. 1.0 = as good as new. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetConditionMultiplier(FName ProductID) const;

	// --- Trading -----------------------------------------------------------

	/**
	 * Buy containers: money out, stock in.
	 * Grey trades are refused unless paid in cash (GDD 6.3).
	 *
	 * @param bAffectInventory  true when the goods land straight in the
	 *        warehouse ledger. false at a face-to-face counter, where you walk
	 *        away holding a box: the ledger only learns about it when the box
	 *        is actually put into storage.
	 * @return false if funds, capacity or the ledger/payment rule block it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Economy")
	bool TryBuy(FName ProductID, int32 Containers, ETradeLedger Ledger, EPaymentMethod Payment,
		bool bAffectInventory = true);

	/** Sell containers: stock out, money in. Same grey/cash and inventory rules. */
	/**
	 * @param ConditionOverride  the age discount to sell at, or a negative
	 *        number to read it off the warehouse. A box already in your hands
	 *        has left the ledger, so whoever is holding it has to supply the
	 *        condition it left with -- otherwise carrying stock to a counter
	 *        would make it new again.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Economy")
	bool TrySell(FName ProductID, int32 Containers, ETradeLedger Ledger, EPaymentMethod Payment,
		bool bAffectInventory = true, float ConditionOverride = -1.f);

	/** Can this ledger be settled with this payment method? */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	static bool IsPaymentAllowed(ETradeLedger Ledger, EPaymentMethod Payment)
	{
		return Ledger == ETradeLedger::White || Payment == EPaymentMethod::Cash;
	}

	// --- Profit and loss ---------------------------------------------------

	/** Weighted average price paid per container of stock still held. 0 if you
	    have never bought any (cheated-in stock included -- it cost nothing). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetAverageUnitCost(FName ProductID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	FProductPnL GetProductPnL(FName ProductID) const;

	/** One entry per product you have ever traded or still hold. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	TArray<FProductPnL> GetAllProductPnL() const;

	/** Profit banked on everything sold so far, across all products. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetTotalRealisedProfit() const;

	/** Paper profit on everything still on the shelves. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetTotalUnrealisedProfit() const;

	/** What the whole warehouse would fetch at today's prices. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetStockValue() const;

	/** GetStockValue for one product: warehouse and trucks, at today's
	    price and the goods' condition. What rides on a forecast (GDD 17). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetStockValueOf(FName ProductID) const;

	/** Containers of a product you hold, warehouse and trucks together. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetHeldContainers(FName ProductID) const;

	/** GDD 8.2 company value, as far as it exists yet: cash + bank + stock.
	    Equipment, debt and pending fines join it when those systems land. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	float GetCompanyValue() const;

	// --- History / save ----------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	const TArray<FTransactionRecord>& GetTransactions() const { return Transactions; }

	/** Just this product's trades, newest last. For the market screen. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Economy")
	TArray<FTransactionRecord> GetTransactionsFor(FName ProductID) const;

	void ResetAll();
	void RestoreFunds(float InCash, float InBank);
	void RestoreTransactions(const TArray<FTransactionRecord>& InTransactions) { Transactions = InTransactions; }

private:
	UInventorySubsystem* GetInventory() const;

	/** Book a spoiled write-off as a sale for nothing, so the loss lands in
	    realised profit through the same replay every other trade goes through
	    (see ReplayCostBasis) and needs no separate running total. */
	UFUNCTION()
	void HandleStockSpoiled(FName ProductID, int32 Containers, bool bWasOnTheBooks);
	UProductCatalogSubsystem* GetCatalogue() const;
	UMarketSubsystem* GetMarket() const;
	int32 GetCurrentGameDay() const;

	void Record(FName ProductID, int32 Containers, float Amount, ETradeLedger Ledger, EPaymentMethod Payment);

	/** Replay the trade log for one product under the weighted-average-cost
	    method. OutHeld is the position the log implies, which is not always
	    what the warehouse holds: Dl_AddStock puts boxes on the shelf without a
	    trade, and a box bought at a counter is carried before it is stored. */
	void ReplayCostBasis(FName ProductID, float& OutAverageUnitCost,
		float& OutRealisedProfit, int32& OutHeld, int32& OutTradeCount) const;

	UPROPERTY()
	float Cash = 0.f;

	UPROPERTY()
	float Bank = 0.f;

	UPROPERTY()
	TArray<FTransactionRecord> Transactions;

	/** Not saved here: the forecast commitments own them and put them back on
	    load (UForecastSubsystem::RestoreCommitments). */
	TMap<FName, float> Locks;
};
