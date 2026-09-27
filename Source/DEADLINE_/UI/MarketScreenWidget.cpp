// Copyright DEADLINE. All Rights Reserved.

#include "UI/MarketScreenWidget.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Economy/MarketSubsystem.h"
#include "Engine/GameInstance.h"
#include "Inventory/InventorySubsystem.h"
#include "UI/DeadlineUIPalette.h"
#include "UI/MarketChartWidget.h"
#include "UI/MarketRowWidget.h"

namespace
{
	template <typename T>
	T* GetSub(const UUserWidget* Widget)
	{
		const UGameInstance* GI = Widget ? Widget->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<T>() : nullptr;
	}

	/**
	 * Money is shown at two precisions on purpose.
	 *
	 * Unit prices keep their cents: the whole game turns on the gap between
	 * what you pay and what a buyer gives you, and that gap is often under a
	 * dollar. Rounding it away would hide the decision.
	 *
	 * Balances drop them. Cash and bank run into the tens of thousands, where
	 * two decimals are four characters of noise on a number nobody reads to
	 * the cent, and they make the top strip harder to scan.
	 */
	FText MoneyPrecise(float Amount)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 2;
		Options.MaximumFractionalDigits = 2;
		return FText::Format(NSLOCTEXT("Deadline", "MoneyFmt", "${0}"),
			FText::AsNumber(Amount, &Options));
	}

	FText MoneyWhole(float Amount)
	{
		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 0;
		return FText::Format(NSLOCTEXT("Deadline", "MoneyFmt", "${0}"),
			FText::AsNumber(FMath::RoundToFloat(Amount), &Options));
	}

	/** Box Units carry one decimal: half-BU containers exist (GDD 5.2). */
	const FNumberFormattingOptions& BUFormat()
	{
		static const FNumberFormattingOptions Options = []
		{
			FNumberFormattingOptions Made;
			Made.MinimumFractionalDigits = 1;
			Made.MaximumFractionalDigits = 1;
			return Made;
		}();
		return Options;
	}

	/** Signed whole money, for profit figures where the sign is the point. */
	FText MoneySigned(float Amount)
	{
		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 0;
		Options.AlwaysSign = true;
		return FText::Format(NSLOCTEXT("Deadline", "MoneyFmt", "${0}"),
			FText::AsNumber(FMath::RoundToFloat(Amount), &Options));
	}
}

UEconomySubsystem* UMarketScreenWidget::GetEconomy() const
{
	return GetSub<UEconomySubsystem>(this);
}

void UMarketScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (BuyButton)          { BuyButton->OnClicked.AddUniqueDynamic(this, &UMarketScreenWidget::HandleBuyClicked); }
	if (SellButton)         { SellButton->OnClicked.AddUniqueDynamic(this, &UMarketScreenWidget::HandleSellClicked); }
	if (QuantityUpButton)   { QuantityUpButton->OnClicked.AddUniqueDynamic(this, &UMarketScreenWidget::HandleQuantityUp); }
	if (QuantityDownButton) { QuantityDownButton->OnClicked.AddUniqueDynamic(this, &UMarketScreenWidget::HandleQuantityDown); }
	if (CloseButton)        { CloseButton->OnClicked.AddUniqueDynamic(this, &UMarketScreenWidget::HandleCloseClicked); }

	// Subscribe rather than poll: prices change once a day, funds and stock
	// only when the player does something.
	if (UMarketSubsystem* Market = GetSub<UMarketSubsystem>(this))
	{
		Market->OnMarketDayAdvanced.AddUniqueDynamic(this, &UMarketScreenWidget::HandleMarketDayAdvanced);
	}
	if (UEconomySubsystem* Economy = GetEconomy())
	{
		Economy->OnFundsChanged.AddUniqueDynamic(this, &UMarketScreenWidget::HandleFundsChanged);
	}
	if (UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this))
	{
		Inventory->OnStockChanged.AddUniqueDynamic(this, &UMarketScreenWidget::HandleStockChanged);
	}

	BuildRows();
	RefreshSummary();
}

void UMarketScreenWidget::NativeDestruct()
{
	if (UMarketSubsystem* Market = GetSub<UMarketSubsystem>(this))
	{
		Market->OnMarketDayAdvanced.RemoveDynamic(this, &UMarketScreenWidget::HandleMarketDayAdvanced);
	}
	if (UEconomySubsystem* Economy = GetEconomy())
	{
		Economy->OnFundsChanged.RemoveDynamic(this, &UMarketScreenWidget::HandleFundsChanged);
	}
	if (UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this))
	{
		Inventory->OnStockChanged.RemoveDynamic(this, &UMarketScreenWidget::HandleStockChanged);
	}
	Super::NativeDestruct();
}

// --- Building ---------------------------------------------------------------

void UMarketScreenWidget::BuildRows()
{
	Rows.Reset();
	if (!ProductList)
	{
		return;
	}
	ProductList->ClearChildren();

	const UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
	UMarketSubsystem* Market = GetSub<UMarketSubsystem>(this);
	if (!Catalogue || !Market)
	{
		return;
	}
	if (!RowWidgetClass)
	{
		SetStatus(TEXT("Row Widget Class ayarlanmamış (Class Defaults)."), true);
		return;
	}

	const TArray<FName> IDs = Catalogue->GetAllProductIDs();
	for (const FName& ID : IDs)
	{
		UMarketRowWidget* Row = CreateWidget<UMarketRowWidget>(GetOwningPlayer(), RowWidgetClass);
		if (!Row)
		{
			continue;
		}
		Row->OnRowClicked.AddDynamic(this, &UMarketScreenWidget::HandleRowClicked);
		ProductList->AddChild(Row);
		Rows.Add(Row);

		// Fill it here, from the ID we are iterating. A freshly created row
		// does not know which product it is, so a later pass that reads the ID
		// back off the row has nothing to read.
		FillRow(Row, ID);
	}

	if (SelectedProduct.IsNone() && IDs.Num() > 0)
	{
		SelectProduct(IDs[0]);
	}
}

void UMarketScreenWidget::FillRow(UMarketRowWidget* Row, FName ProductID) const
{
	const UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
	const UMarketSubsystem* Market = GetSub<UMarketSubsystem>(this);
	const UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	if (!Row || !Catalogue || !Market)
	{
		return;
	}

	Row->SetRow(Market->GetQuote(ProductID),
		Catalogue->GetDisplayName(ProductID),
		Inventory ? Inventory->GetPhysicalStock(ProductID) : 0);
	Row->SetSelected(ProductID == SelectedProduct);
}

void UMarketScreenWidget::RefreshRows()
{
	// Safe to read the ID back off the row now: BuildRows put it there.
	for (UMarketRowWidget* Row : Rows)
	{
		if (Row)
		{
			FillRow(Row, Row->GetProductID());
		}
	}
}

// --- Selection and detail ---------------------------------------------------

void UMarketScreenWidget::SelectProduct(FName ProductID)
{
	SelectedProduct = ProductID;
	Quantity = 1;

	for (UMarketRowWidget* Row : Rows)
	{
		if (Row)
		{
			Row->SetSelected(Row->GetProductID() == SelectedProduct);
		}
	}
	RefreshDetail();
}

void UMarketScreenWidget::RefreshDetail()
{
	const UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
	const UMarketSubsystem* Market = GetSub<UMarketSubsystem>(this);
	const UEconomySubsystem* Economy = GetEconomy();
	if (!Catalogue || !Market || !Economy || SelectedProduct.IsNone())
	{
		return;
	}

	const FProductRow* Product = Catalogue->FindProduct(SelectedProduct);
	const FProductPnL PnL = Economy->GetProductPnL(SelectedProduct);

	if (SelectedNameText)
	{
		SelectedNameText->SetText(FText::FromString(Catalogue->GetDisplayName(SelectedProduct)));
	}
	if (BuyPriceText)  { BuyPriceText->SetText(MoneyPrecise(Economy->GetBuyPrice(SelectedProduct))); }

	// Obsolescence (GDD 5.1) is a discount on YOUR goods, not a move in the
	// market. Showing the quote here while the sale pays less would make the
	// screen lie about the one number the player is deciding on.
	const float Condition = Economy->GetConditionMultiplier(SelectedProduct);
	if (SellPriceText)
	{
		SellPriceText->SetText(MoneyPrecise(Economy->GetSellPriceForHeldStock(SelectedProduct)));
		SellPriceText->SetColorAndOpacity(FSlateColor(
			Condition < 0.995f ? DeadlineUI::Warn : DeadlineUI::Body));
	}
	if (HeldText)
	{
		// Held count, then whatever the goods have to say for themselves. Both
		// stay silent for the sixty-odd products that neither rot nor age, so
		// the tile is quiet until it has a reason not to be.
		FString Held = FString::Printf(TEXT("%d"), PnL.ContainersHeld);
		if (PnL.ContainersHeld > 0 || PnL.LooseBoxesHeld > 0)
		{
			const UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
			const int32 DaysLeft = Inventory ? Inventory->GetDaysUntilSpoilage(SelectedProduct) : -1;
			if (DaysLeft >= 0)
			{
				Held += FString::Printf(TEXT("   %d gun"), DaysLeft);
			}
			if (Condition < 0.995f)
			{
				Held += FString::Printf(TEXT("   %%%.0f"), Condition * 100.f);
			}
		}
		HeldText->SetText(FText::FromString(Held));
	}
	if (QuantityText)
	{
		QuantityText->SetText(FText::FromString(FString::Printf(TEXT("%d"), Quantity)));
	}
	if (AverageCostText)
	{
		if (PnL.AverageUnitCost > 0.f)
		{
			// Compared against the sell price, because that is what the stock
			// would actually fetch. Comparing to the quoted price would
			// flatter the player by hiding the buyer's discount.
			// Kept short on purpose: this sits in a fixed-width stat tile, and
			// the cents of the gap are noise next to its sign and size.
			const float Sell = Economy->GetSellPrice(SelectedProduct);
			AverageCostText->SetText(FText::Format(
				NSLOCTEXT("Deadline", "AvgCostFmt", "{0}  ({1})"),
				MoneyPrecise(PnL.AverageUnitCost),
				MoneySigned(Sell - PnL.AverageUnitCost)));
			AverageCostText->SetColorAndOpacity(FSlateColor(
				Sell >= PnL.AverageUnitCost ? DeadlineUI::Profit : DeadlineUI::Loss));
		}
		else
		{
			AverageCostText->SetText(FText::FromString(TEXT("-")));
			AverageCostText->SetColorAndOpacity(FSlateColor(DeadlineUI::Body));
		}
	}

	if (TotalCostText)
	{
		const float BuyTotal = Economy->GetBuyPrice(SelectedProduct) * Quantity;
		const float SellTotal = Economy->GetSellPrice(SelectedProduct) * Quantity;
		const float NeededBU = Catalogue->GetVolumeBU(SelectedProduct) * Quantity;

		TotalCostText->SetText(FText::Format(
			NSLOCTEXT("Deadline", "TotalFmt", "{0} kutu  ·  al {1}  ·  sat {2}  ·  {3} BU"),
			FText::AsNumber(Quantity),
			MoneyPrecise(BuyTotal),
			MoneyPrecise(SellTotal),
			FText::AsNumber(NeededBU, &BUFormat())));

		// Amber the moment the trade stops being possible, so the player finds
		// out while choosing the quantity rather than by pressing Al and being
		// told no.
		const UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
		const bool bNoMoney = Economy->GetBank() < BuyTotal;
		// Room is per storage class, not warehouse-wide (GDD 11): a full cold
		// zone blocks frozen goods while the racks stand empty.
		const bool bNoRoom = Inventory && !Inventory->HasSpaceFor(SelectedProduct, Quantity);
		TotalCostText->SetColorAndOpacity(FSlateColor(
			(bNoMoney || bNoRoom) ? DeadlineUI::Warn : DeadlineUI::Body));
	}

	if (PriceChart)
	{
		const FRiskBandParams Band = FProductRow::GetRiskBandParams(
			Product ? Product->RiskBand : ERiskBand::Stable);
		const float BasePrice = Market->GetBasePrice(SelectedProduct);
		PriceChart->SetSeries(
			Market->GetPriceHistory(SelectedProduct, ChartDays),
			PnL.AverageUnitCost,
			static_cast<float>(BasePrice * Band.FloorMultiplier),
			static_cast<float>(BasePrice * Band.CeilingMultiplier));
	}
}

void UMarketScreenWidget::RefreshSummary()
{
	const UEconomySubsystem* Economy = GetEconomy();
	const UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	if (!Economy)
	{
		return;
	}

	if (CashText) { CashText->SetText(MoneyWhole(Economy->GetCash())); }
	if (BankText) { BankText->SetText(MoneyWhole(Economy->GetBank())); }
	if (CapacityText && Inventory)
	{
		CapacityText->SetText(FText::FromString(FString::Printf(TEXT("%.1f / %.1f BU"),
			Inventory->GetUsedBU(), Inventory->GetCapacityBU())));
	}
	if (ProfitText)
	{
		const float Realised = Economy->GetTotalRealisedProfit();
		const float Unrealised = Economy->GetTotalUnrealisedProfit();
		// Both numbers, labelled. One combined "profit" figure would hide
		// whether the money is banked or still sitting on a shelf.
		ProfitText->SetText(FText::Format(
			NSLOCTEXT("Deadline", "ProfitFmt", "kasa {0}   raf {1}"),
			MoneySigned(Realised), MoneySigned(Unrealised)));
		ProfitText->SetColorAndOpacity(FSlateColor(
			Realised + Unrealised >= 0.f ? DeadlineUI::Profit : DeadlineUI::Loss));
	}
}

void UMarketScreenWidget::SetStatus(const FString& Message, bool bProblem)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(bProblem ? DeadlineUI::Warn : DeadlineUI::Body));
	}
	if (bProblem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Deadline] Market screen: %s"), *Message);
	}
}

// --- Input ------------------------------------------------------------------

void UMarketScreenWidget::HandleRowClicked(FName ProductID)
{
	SelectProduct(ProductID);
}

void UMarketScreenWidget::HandleQuantityUp()
{
	Quantity = FMath::Clamp(Quantity + 1, 1, 999);
	RefreshDetail();
}

void UMarketScreenWidget::HandleQuantityDown()
{
	Quantity = FMath::Clamp(Quantity - 1, 1, 999);
	RefreshDetail();
}

void UMarketScreenWidget::HandleBuyClicked()
{
	UEconomySubsystem* Economy = GetEconomy();
	if (!Economy || SelectedProduct.IsNone())
	{
		return;
	}

	// White ledger, paid from the bank. The grey channel does not open until
	// Month 7; the arguments are spelled out rather than defaulted so this
	// call site has to be revisited when it does.
	if (Economy->TryBuy(SelectedProduct, Quantity, ETradeLedger::White, EPaymentMethod::Bank))
	{
		SetStatus(FString::Printf(TEXT("%d kutu alındı."), Quantity), false);
	}
	else
	{
		// TryBuy fails for money or for Box Units. Say which, or the player
		// just sees a button that does nothing.
		const UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
		const UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
		const float NeededBU = (Inventory && Catalogue)
			? Catalogue->GetVolumeBU(SelectedProduct) * Quantity : 0.f;
		const bool bNoRoom = Inventory && !Inventory->HasSpaceFor(SelectedProduct, Quantity);

		SetStatus(bNoRoom
			? FString::Printf(TEXT("%s dolu: %.1f BU gerekiyor."),
				*FStorageClassRules::DisplayName(
					Inventory->GetStorageClassFor(SelectedProduct)).ToString(), NeededBU)
			: FString(TEXT("Bankada yeterli para yok.")), true);
	}
}

void UMarketScreenWidget::HandleSellClicked()
{
	UEconomySubsystem* Economy = GetEconomy();
	if (!Economy || SelectedProduct.IsNone())
	{
		return;
	}

	if (Economy->TrySell(SelectedProduct, Quantity, ETradeLedger::White, EPaymentMethod::Bank))
	{
		SetStatus(FString::Printf(TEXT("%d kutu satıldı."), Quantity), false);
	}
	else
	{
		SetStatus(TEXT("Depoda yeterli stok yok."), true);
	}
}

void UMarketScreenWidget::HandleCloseClicked()
{
	RemoveFromParent();
}

// --- Subscriptions ----------------------------------------------------------

void UMarketScreenWidget::HandleMarketDayAdvanced(int32 NewDay)
{
	RefreshRows();
	RefreshDetail();
	RefreshSummary();
}

void UMarketScreenWidget::HandleFundsChanged()
{
	RefreshSummary();
	RefreshDetail();
}

void UMarketScreenWidget::HandleStockChanged(FName ProductID)
{
	RefreshRows();
	RefreshSummary();
	if (ProductID == SelectedProduct)
	{
		RefreshDetail();
	}
}
