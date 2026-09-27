// Copyright DEADLINE. All Rights Reserved.
//
// The forecast board screen (GDD 17), on N.
//
// Left: one UForecastCardWidget per signal or running event. Right: the
// selected one in full -- the odds and the timing in large type, your stake
// and what it is worth if it happens, the products it touches with your stock
// in each, and one sentence saying what the decision actually is. The Month 4
// gate test asks for the decision to be understood in 30 seconds; the detail
// panel is laid out to be read top to bottom in that time.
//
// Below the product list, on a signal that pushes prices: the commitment panel
// -- which product, how many containers, how many days to hold after the day
// -- and the budget it locks. Once committed the same panel shows progress
// and offers to call it off.
//
// Nothing polls (CLAUDE.md tick rules, method 2): the screen refreshes when
// the board says it changed (day, stock) and when a bulletin lands.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Events/NewsSubsystem.h"
#include "Forecast/ForecastSubsystem.h"
#include "ForecastScreenWidget.generated.h"

class UBorder;
class UButton;
class UForecastCardWidget;
class UPanelWidget;
class UTextBlock;

UCLASS()
class DEADLINE__API UForecastScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	void Refresh();

	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	void SelectEntry(FName EventID);

	/** One card per entry. Set by build_forecast_screen.py on the class
	    defaults -- the one thing the tree cannot say. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Forecast")
	TSubclassOf<UForecastCardWidget> CardWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- Bound widgets. Exact names required. ------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> CardList;

	/** Shown instead of the list when there is nothing to forecast. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HeaderDayText;

	/** Total exposure across the board and the money you have to act with. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SummaryText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	/** The notebook tab: past forecasts and their results. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> NotebookTabButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailStatusText;

	/** "%75 · 2 gün sonra", large. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailOddsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailBodyText;

	/** Your stake and what it is worth if it happens. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailStakeText;

	/** One row per product the entry touches. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> DetailProductList;

	/** The decision in one sentence. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailHintText;

	/** Hidden when there is nothing to list or decide, so an empty board
	    does not show a heading over nothing and a blank box. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailProductsLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DetailHintFrame;

	// --- Commitment panel (roadmap Month 4). Shown on signals that push a
	// price; hidden on running events and route events. -------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CommitFrame;

	/** Empty while choosing; the progress line once committed. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CommitStatusText;

	/** Product / amount / duration pickers, collapsed once committed. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> CommitControls;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CommitProductText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ProductPrevButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ProductNextButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CommitQtyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QtyMinusButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QtyPlusButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CommitHoldText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HoldMinusButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> HoldPlusButton;

	/** The budget that would be locked, or why it cannot be. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CommitBudgetText;

	/** TAAHHÜT ET while choosing, VAZGEÇ once committed. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CommitButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CommitButtonText;

private:
	void ShowDetail(const FForecastEntry* Entry);
	void AddProductRow(const FForecastProductLine& Line, bool bActive);
	void ShowCommitPanel(const FForecastEntry* Entry);

	UFUNCTION() void HandleProductPrev();
	UFUNCTION() void HandleProductNext();
	UFUNCTION() void HandleQtyMinus();
	UFUNCTION() void HandleQtyPlus();
	UFUNCTION() void HandleHoldMinus();
	UFUNCTION() void HandleHoldPlus();
	UFUNCTION() void HandleCommitClicked();
	void StepProduct(int32 Delta);
	void RefreshCommitPanel();

	UFUNCTION()
	void HandleCardClicked(FName EventID);

	UFUNCTION()
	void HandleBoardChanged();

	UFUNCTION()
	void HandleBulletin(const FNewsBulletin& Bulletin);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleNotebookTabClicked();

	UForecastSubsystem* GetForecast() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UForecastCardWidget>> Cards;

	TArray<FForecastEntry> Board;
	FName SelectedEventID;

	// The pickers. Reset whenever the selection moves to another entry.
	FName CommitForEventID;
	int32 CommitProductIndex = 0;
	int32 CommitQty = 1;
	int32 CommitHold = 2;
};
