// Copyright DEADLINE. All Rights Reserved.
//
// The market screen (GDD 17): every product with its current price, trend and
// history, and the buy/sell controls.
//
// All wiring is in C++ and every child is bound by name, so the Blueprint needs
// no graph. The exact names the Designer must use are in
// Assets/KULLANICI_GOREVLERI.md.
//
// Nothing polls. The screen fills itself once when it opens and then updates
// only when the market, the funds or the ledger say something changed
// (CLAUDE.md tick rules, method 2).

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Economy/EconomySubsystem.h"
#include "MarketScreenWidget.generated.h"

class UButton;
class UMarketChartWidget;
class UMarketRowWidget;
class UPanelWidget;
class UTextBlock;

UCLASS()
class DEADLINE__API UMarketScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Which product's detail panel and chart are showing. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Market")
	void SelectProduct(FName ProductID);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	FName GetSelectedProduct() const { return SelectedProduct; }

	/** How many days of history the chart shows. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Market", meta = (ClampMin = "5"))
	int32 ChartDays = 30;

	/** Row widget to spawn per product. Set this in the Blueprint's Class
	    Defaults -- it is the one thing the Designer cannot infer. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Market")
	TSubclassOf<UMarketRowWidget> RowWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- Bound widgets. Exact names required. ------------------------------

	/** Holds one UMarketRowWidget per product. A ScrollBox or VerticalBox. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ProductList;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UMarketChartWidget> PriceChart;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SelectedNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BuyPriceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SellPriceText;

	/** Your weighted average cost, the amber line on the chart. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AverageCostText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HeldText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CashText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BankText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CapacityText;

	/** Realised and unrealised profit, so the screen answers "am I winning?" */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ProfitText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BuyButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SellButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuantityUpButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> QuantityDownButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	/** Last action's result, e.g. "Depoda yer yok". */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	/** What the chosen quantity actually costs, and what it would fetch, and
	    the Box Units it needs. Without this the player is doing the
	    multiplication in their head before every trade. Turns amber when the
	    bank or the warehouse cannot take it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TotalCostText;

private:
	UFUNCTION() void HandleBuyClicked();
	UFUNCTION() void HandleSellClicked();
	UFUNCTION() void HandleQuantityUp();
	UFUNCTION() void HandleQuantityDown();
	UFUNCTION() void HandleCloseClicked();

	UFUNCTION() void HandleRowClicked(FName ProductID);
	UFUNCTION() void HandleMarketDayAdvanced(int32 NewDay);
	UFUNCTION() void HandleFundsChanged();
	UFUNCTION() void HandleStockChanged(FName ProductID);

	/** Build a row per product. Only on open and after a catalogue reload:
	    the product list does not change day to day. */
	void BuildRows();

	/** Put one product's numbers into one row. The product is passed in, not
	    read off the row: a row created a moment ago does not know yet. */
	void FillRow(UMarketRowWidget* Row, FName ProductID) const;

	/** Refresh every row's numbers in place, without rebuilding widgets. */
	void RefreshRows();

	/** Refresh the detail panel, chart and buy/sell affordability. */
	void RefreshDetail();

	/** Cash, bank, capacity, profit. */
	void RefreshSummary();

	void SetStatus(const FString& Message, bool bProblem);

	UEconomySubsystem* GetEconomy() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMarketRowWidget>> Rows;

	FName SelectedProduct;
	int32 Quantity = 1;
};
