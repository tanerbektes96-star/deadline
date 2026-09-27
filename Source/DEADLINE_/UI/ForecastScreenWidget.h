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
// Commitments (the next roadmap item) will add their controls to the detail
// panel.
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

private:
	void ShowDetail(const FForecastEntry* Entry);
	void AddProductRow(const FForecastProductLine& Line, bool bActive);

	UFUNCTION()
	void HandleCardClicked(FName EventID);

	UFUNCTION()
	void HandleBoardChanged();

	UFUNCTION()
	void HandleBulletin(const FNewsBulletin& Bulletin);

	UFUNCTION()
	void HandleCloseClicked();

	UForecastSubsystem* GetForecast() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UForecastCardWidget>> Cards;

	TArray<FForecastEntry> Board;
	FName SelectedEventID;
};
