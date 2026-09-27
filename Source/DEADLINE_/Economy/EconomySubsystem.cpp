// Copyright DEADLINE. All Rights Reserved.

#include "Economy/EconomySubsystem.h"

#include "Core/DeadlineSettings.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Economy/MarketSubsystem.h"
#include "Engine/GameInstance.h"
#include "Fleet/FleetSubsystem.h"
#include "Inventory/InventorySubsystem.h"

void UEconomySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UProductCatalogSubsystem::StaticClass());
	Collection.InitializeDependency(UInventorySubsystem::StaticClass());
	Collection.InitializeDependency(UMarketSubsystem::StaticClass());
	// The fleet too: goods in transit are part of the company value, and cargo
	// that rots on the road has to reach the trade log the same way.
	Collection.InitializeDependency(UFleetSubsystem::StaticClass());
	Super::Initialize(Collection);

	const UDeadlineSettings& Settings = UDeadlineSettings::Get();
	Cash = Settings.StartingCash;
	Bank = Settings.StartingBank;

	// Spoilage is a loss, and losses belong in the trade log. The warehouse
	// knows what rotted; only this class knows what it was worth.
	if (UInventorySubsystem* Inventory = GetInventory())
	{
		Inventory->OnStockSpoiled.AddUniqueDynamic(this, &UEconomySubsystem::HandleStockSpoiled);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			Fleet->OnCargoSpoiled.AddUniqueDynamic(this, &UEconomySubsystem::HandleStockSpoiled);
		}
	}
}

void UEconomySubsystem::HandleStockSpoiled(FName ProductID, int32 Containers,
	bool bWasOnTheBooks)
{
	if (Containers <= 0)
	{
		return;
	}

	// A sale of nothing, for nothing. The cost basis replay then charges the
	// whole of what those containers cost against realised profit, which is
	// exactly what a write-off is, and the market screen shows the trade
	// instead of the stock silently going missing.
	Record(ProductID, -Containers, 0.f,
		bWasOnTheBooks ? ETradeLedger::White : ETradeLedger::Grey,
		EPaymentMethod::Cash);
	OnFundsChanged.Broadcast();
}

UInventorySubsystem* UEconomySubsystem::GetInventory() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
}

UProductCatalogSubsystem* UEconomySubsystem::GetCatalogue() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
}

UMarketSubsystem* UEconomySubsystem::GetMarket() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMarketSubsystem>() : nullptr;
}

int32 UEconomySubsystem::GetCurrentGameDay() const
{
	const UGameInstance* GI = GetGameInstance();
	const UTimeSubsystem* Time = GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
	return Time ? Time->GetDay() : 0;
}

// --- Funds -------------------------------------------------------------------

void UEconomySubsystem::AddCash(float Amount)
{
	Cash += Amount;
	OnFundsChanged.Broadcast();
}

void UEconomySubsystem::AddBank(float Amount)
{
	Bank += Amount;
	OnFundsChanged.Broadcast();
}

bool UEconomySubsystem::Withdraw(float Amount)
{
	if (Amount <= 0.f || Bank < Amount)
	{
		return false;
	}
	Bank -= Amount;
	Cash += Amount;
	OnFundsChanged.Broadcast();
	return true;
}

bool UEconomySubsystem::Deposit(float Amount)
{
	if (Amount <= 0.f || Cash < Amount)
	{
		return false;
	}
	Cash -= Amount;
	Bank += Amount;
	OnFundsChanged.Broadcast();
	return true;
}

// --- Prices ------------------------------------------------------------------

float UEconomySubsystem::GetMarketPrice(FName ProductID) const
{
	if (const UMarketSubsystem* Market = GetMarket())
	{
		return Market->GetPrice(ProductID);
	}
	// No market subsystem should be impossible in a running game, but a price
	// of zero would silently hand out free goods, so fall back to base price.
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	return Row ? Row->BasePrice : 0.f;
}

float UEconomySubsystem::GetBuyPrice(FName ProductID) const
{
	const UDeadlineSettings& Settings = UDeadlineSettings::Get();
	return GetMarketPrice(ProductID) * (1.f + Settings.BuyPremium + Settings.HandlingCost);
}

float UEconomySubsystem::GetSellPrice(FName ProductID) const
{
	const UDeadlineSettings& Settings = UDeadlineSettings::Get();
	return GetMarketPrice(ProductID) * FMath::Max(0.f, 1.f - Settings.SellDiscount - Settings.HandlingCost);
}

float UEconomySubsystem::GetConditionMultiplier(FName ProductID) const
{
	const UInventorySubsystem* Inventory = GetInventory();
	return Inventory ? Inventory->GetConditionMultiplier(ProductID) : 1.f;
}

float UEconomySubsystem::GetSellPriceForHeldStock(FName ProductID) const
{
	return GetSellPrice(ProductID) * GetConditionMultiplier(ProductID);
}

// --- Trading -----------------------------------------------------------------

bool UEconomySubsystem::TryBuy(FName ProductID, int32 Containers, ETradeLedger Ledger,
	EPaymentMethod Payment, bool bAffectInventory)
{
	if (Containers <= 0 || !IsPaymentAllowed(Ledger, Payment))
	{
		return false;
	}

	UInventorySubsystem* Inventory = GetInventory();
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	if (!Inventory || !Catalogue || !Catalogue->IsValidProduct(ProductID))
	{
		return false;
	}

	const float Total = GetBuyPrice(ProductID) * Containers;
	const float Available = (Payment == EPaymentMethod::Cash) ? Cash : Bank;
	if (Available < Total)
	{
		return false;
	}
	const bool bRecorded = (Ledger == ETradeLedger::White);
	if (bAffectInventory)
	{
		if (!Inventory->AddStock(ProductID, Containers, bRecorded))
		{
			return false;   // no free Box Units
		}
	}

	(Payment == EPaymentMethod::Cash ? Cash : Bank) -= Total;

	// Write the ledger BEFORE telling anyone the funds moved. Profit and loss
	// is replayed from this log, so a listener woken by OnFundsChanged would
	// otherwise replay a ledger that is missing the trade that just woke it,
	// and report a cost basis one trade out of date.
	Record(ProductID, Containers, -Total, Ledger, Payment);
	OnFundsChanged.Broadcast();
	return true;
}

bool UEconomySubsystem::TrySell(FName ProductID, int32 Containers, ETradeLedger Ledger,
	EPaymentMethod Payment, bool bAffectInventory, float ConditionOverride)
{
	if (Containers <= 0 || !IsPaymentAllowed(Ledger, Payment))
	{
		return false;
	}

	UInventorySubsystem* Inventory = GetInventory();
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	if (!Inventory || !Catalogue || !Catalogue->IsValidProduct(ProductID))
	{
		return false;
	}

	// Read the condition BEFORE the goods leave the ledger: once they are gone
	// there is nothing left to be old.
	const float Condition = ConditionOverride >= 0.f
		? FMath::Clamp(ConditionOverride, 0.f, 1.f)
		: GetConditionMultiplier(ProductID);

	const bool bRecorded = (Ledger == ETradeLedger::White);
	if (bAffectInventory && !Inventory->RemoveStock(ProductID, Containers, bRecorded))
	{
		return false;   // not enough physical stock
	}

	const float Total = GetSellPrice(ProductID) * Condition * Containers;
	(Payment == EPaymentMethod::Cash ? Cash : Bank) += Total;

	// Ledger first, then the broadcast. See TryBuy.
	Record(ProductID, -Containers, Total, Ledger, Payment);
	OnFundsChanged.Broadcast();
	return true;
}

void UEconomySubsystem::Record(FName ProductID, int32 Containers, float Amount,
	ETradeLedger Ledger, EPaymentMethod Payment)
{
	FTransactionRecord& New = Transactions.AddDefaulted_GetRef();
	New.ProductID = ProductID;
	New.Containers = Containers;
	New.Amount = Amount;
	New.Ledger = Ledger;
	New.Payment = Payment;
	New.GameDay = GetCurrentGameDay();

	OnTransactionRecorded.Broadcast(New);
}

// --- Profit and loss ----------------------------------------------------------
//
// All of it is replayed from the trade log rather than kept as running totals.
// A running total has to be saved, migrated, and kept correct at every call
// site that touches stock; a replay cannot disagree with the trades it came
// from. The log is a few hundred entries in a long run, so the cost is noise.

void UEconomySubsystem::ReplayCostBasis(FName ProductID, float& OutAverageUnitCost,
	float& OutRealisedProfit, int32& OutHeld, int32& OutTradeCount) const
{
	// Weighted average cost: every container held is worth the same average,
	// so a sale takes its cost out of the pool at that average. The
	// alternative, FIFO, would need per-lot tracking that nothing else in the
	// game wants yet.
	double PoolCost = 0.0;
	int32 PoolUnits = 0;
	double Realised = 0.0;
	int32 Trades = 0;

	for (const FTransactionRecord& Trade : Transactions)
	{
		if (Trade.ProductID != ProductID)
		{
			continue;
		}
		++Trades;

		if (Trade.Containers > 0)
		{
			// Bought: Amount is negative (money out).
			PoolCost += -static_cast<double>(Trade.Amount);
			PoolUnits += Trade.Containers;
		}
		else if (Trade.Containers < 0)
		{
			const int32 Sold = FMath::Min(-Trade.Containers, PoolUnits);
			const double Average = (PoolUnits > 0) ? PoolCost / PoolUnits : 0.0;
			const double CostOfGoods = Average * Sold;

			Realised += static_cast<double>(Trade.Amount) - CostOfGoods;
			PoolCost = FMath::Max(0.0, PoolCost - CostOfGoods);
			PoolUnits -= Sold;
		}
	}

	OutAverageUnitCost = (PoolUnits > 0) ? static_cast<float>(PoolCost / PoolUnits) : 0.f;
	OutRealisedProfit = static_cast<float>(Realised);
	OutHeld = PoolUnits;
	OutTradeCount = Trades;
}

float UEconomySubsystem::GetAverageUnitCost(FName ProductID) const
{
	float Average = 0.f;
	float Realised = 0.f;
	int32 Held = 0;
	int32 Trades = 0;
	ReplayCostBasis(ProductID, Average, Realised, Held, Trades);
	return Average;
}

FProductPnL UEconomySubsystem::GetProductPnL(FName ProductID) const
{
	FProductPnL Result;
	Result.ProductID = ProductID;

	int32 LedgerHeld = 0;
	ReplayCostBasis(ProductID, Result.AverageUnitCost, Result.RealisedProfit,
		LedgerHeld, Result.TradeCount);

	// Value what is actually on the shelves, not what the log implies. The two
	// differ after a dev cheat, or while a bought box is still being carried.
	const UInventorySubsystem* Inventory = GetInventory();
	const UGameInstance* GI = GetGameInstance();
	const UFleetSubsystem* Fleet = GI ? GI->GetSubsystem<UFleetSubsystem>() : nullptr;

	Result.ContainersHeld = Inventory ? Inventory->GetPhysicalStock(ProductID) : LedgerHeld;
	Result.LooseBoxesHeld = Inventory ? Inventory->GetLooseBoxes(ProductID) : 0;
	Result.ContainersInTransit = Fleet ? Fleet->GetTotalWholeContainers(ProductID) : 0;

	// Opened pallets count too (GDD 10.2), and so does anything on a truck:
	// loading a delivery must not make the company look poorer for the trip.
	const float HeldEquivalent = (Inventory
		? Inventory->GetPhysicalContainers(ProductID)
		: static_cast<float>(LedgerHeld))
		+ (Fleet ? Fleet->GetTotalContainers(ProductID) : 0.f);
	// Aged stock is worth less than the quote, and the warehouse and the road
	// are valued separately: a load that left a month ago is not as fresh as
	// what came in yesterday.
	const float WarehouseEquivalent = Inventory
		? Inventory->GetPhysicalContainers(ProductID)
		: static_cast<float>(LedgerHeld);
	const float FleetEquivalent = Fleet ? Fleet->GetTotalContainers(ProductID) : 0.f;

	Result.MarketValue = GetSellPrice(ProductID)
		* (WarehouseEquivalent * GetConditionMultiplier(ProductID)
			+ FleetEquivalent * (Fleet ? Fleet->GetConditionMultiplier(ProductID) : 1.f));
	Result.UnrealisedProfit = Result.MarketValue - Result.AverageUnitCost * HeldEquivalent;
	return Result;
}

TArray<FProductPnL> UEconomySubsystem::GetAllProductPnL() const
{
	// Everything traded, plus anything sitting in the warehouse that was never
	// traded for (cheats, and later contract deliveries).
	TSet<FName> Products;
	for (const FTransactionRecord& Trade : Transactions)
	{
		Products.Add(Trade.ProductID);
	}
	if (const UInventorySubsystem* Inventory = GetInventory())
	{
		for (const TPair<FName, FInventoryEntry>& Pair : Inventory->GetAllEntries())
		{
			if (Pair.Value.PhysicalStock > 0 || Pair.Value.PhysicalLooseBoxes > 0)
			{
				Products.Add(Pair.Key);
			}
		}
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			for (const FName& ID : Fleet->GetAllCarriedProductIDs())
			{
				Products.Add(ID);
			}
		}
	}

	TArray<FProductPnL> Result;
	Result.Reserve(Products.Num());
	for (const FName& ID : Products)
	{
		Result.Add(GetProductPnL(ID));
	}
	Result.Sort([](const FProductPnL& A, const FProductPnL& B)
	{
		return A.ProductID.LexicalLess(B.ProductID);
	});
	return Result;
}

float UEconomySubsystem::GetTotalRealisedProfit() const
{
	double Total = 0.0;
	TSet<FName> Seen;
	for (const FTransactionRecord& Trade : Transactions)
	{
		if (Seen.Contains(Trade.ProductID))
		{
			continue;
		}
		Seen.Add(Trade.ProductID);

		float Average = 0.f;
		float Realised = 0.f;
		int32 Held = 0;
		int32 Trades = 0;
		ReplayCostBasis(Trade.ProductID, Average, Realised, Held, Trades);
		Total += Realised;
	}
	return static_cast<float>(Total);
}

float UEconomySubsystem::GetTotalUnrealisedProfit() const
{
	double Total = 0.0;
	for (const FProductPnL& Entry : GetAllProductPnL())
	{
		Total += Entry.UnrealisedProfit;
	}
	return static_cast<float>(Total);
}

float UEconomySubsystem::GetStockValue() const
{
	// Every product held anywhere, each valued once by GetStockValueOf, so
	// the total and the per-product figure the forecast board shows cannot
	// drift apart.
	TSet<FName> Held;
	if (const UInventorySubsystem* Inventory = GetInventory())
	{
		for (const TPair<FName, FInventoryEntry>& Pair : Inventory->GetAllEntries())
		{
			Held.Add(Pair.Key);
		}
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			Held.Append(Fleet->GetAllCarriedProductIDs());
		}
	}

	double Total = 0.0;
	for (const FName& ID : Held)
	{
		Total += GetStockValueOf(ID);
	}
	return static_cast<float>(Total);
}

float UEconomySubsystem::GetStockValueOf(FName ProductID) const
{
	double Total = 0.0;
	if (const UInventorySubsystem* Inventory = GetInventory())
	{
		if (const FInventoryEntry* Entry = Inventory->GetAllEntries().Find(ProductID))
		{
			Total += static_cast<double>(GetSellPriceForHeldStock(ProductID)) * Entry->PhysicalContainers();
		}
	}

	// Everything riding in a truck as well (GDD 9: the goods are still yours
	// while they are on the road).
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			const float InTrucks = Fleet->GetTotalContainers(ProductID);
			if (InTrucks > 0.f)
			{
				Total += static_cast<double>(GetSellPrice(ProductID))
					* Fleet->GetConditionMultiplier(ProductID) * InTrucks;
			}
		}
	}
	return static_cast<float>(Total);
}

float UEconomySubsystem::GetHeldContainers(FName ProductID) const
{
	float Held = 0.f;
	if (const UInventorySubsystem* Inventory = GetInventory())
	{
		Held += Inventory->GetPhysicalContainers(ProductID);
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			Held += Fleet->GetTotalContainers(ProductID);
		}
	}
	return Held;
}

float UEconomySubsystem::GetCompanyValue() const
{
	return Cash + Bank + GetStockValue();
}

TArray<FTransactionRecord> UEconomySubsystem::GetTransactionsFor(FName ProductID) const
{
	TArray<FTransactionRecord> Result;
	for (const FTransactionRecord& Trade : Transactions)
	{
		if (Trade.ProductID == ProductID)
		{
			Result.Add(Trade);
		}
	}
	return Result;
}

// --- Save support -------------------------------------------------------------

void UEconomySubsystem::ResetAll()
{
	const UDeadlineSettings& Settings = UDeadlineSettings::Get();
	Cash = Settings.StartingCash;
	Bank = Settings.StartingBank;
	Transactions.Empty();
	OnFundsChanged.Broadcast();
}

void UEconomySubsystem::RestoreFunds(float InCash, float InBank)
{
	Cash = InCash;
	Bank = InBank;
	OnFundsChanged.Broadcast();
}
