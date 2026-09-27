// Copyright DEADLINE. All Rights Reserved.
//
// The notebook (GDD 17 "Not Defteri", roadmap Month 4): past forecasts, how
// they came out, and the "neden yanıldım" card for each. The forecast board's
// second tab, on N.
//
// Top: the tally -- how many forecasts came true, how many trades paid, the
// running total -- and, once a costly lesson has come up twice, the one habit
// worth changing. Left: one UNotebookRowWidget per commitment, newest first,
// live ones included so a forecast does not vanish between the commit and the
// verdict. Right: the selected one in full, worded exactly as its result card
// was (UCommitmentResultWidget's statics), so the notebook never tells a
// different story from the card the player already saw.
//
// Cancelled commitments are left out: they were never judged, and the
// notebook is about judgements.
//
// Nothing polls: the screen refreshes when the board changes and when a
// commitment is judged.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Forecast/ForecastSubsystem.h"
#include "NotebookScreenWidget.generated.h"

class UBorder;
class UButton;
class UNotebookRowWidget;
class UPanelWidget;
class UTextBlock;

UCLASS()
class DEADLINE__API UNotebookScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	void Refresh();

	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	void SelectCommitment(int32 CommitmentID);

	/** One row per commitment. Set by build_forecast_screen.py on the class
	    defaults. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|Forecast")
	TSubclassOf<UNotebookRowWidget> RowWidgetClass;

	/** The habit line for a lesson that keeps coming up. Empty for lessons
	    that are not a habit to change. Public for tests. */
	static FText HabitLine(ECommitmentLesson Lesson, int32 Count);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- Bound widgets. Exact names required. ------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> RowList;

	/** Shown instead of the list before the first commitment. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	/** Back to the board. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BoardTabButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HeaderDayText;

	/** "5 tahmin · 3'ü tuttu · kaynaklar ortalama %72 dedi · 2/4 işlem kârlı". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> SummaryText;

	/** The running total, in the outcome's colour. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TotalText;

	/** The habit worth changing, or the confidence check. Collapsed when
	    there is nothing to say. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PatternFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PatternText;

	// --- Detail: the selected commitment, as its result card said it ------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailVerdictText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailOddsText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailOutcomeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailLedgerText;

	/** Collapsed while the commitment is live: no verdict, no lesson. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DetailLessonFrame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailLessonTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DetailLessonText;

private:
	void ShowSummary(const FNotebookSummary& Summary);
	void ShowDetail(const FForecastCommitment* C);

	FString EventName(FName EventID) const;
	FString ProductName(FName ProductID) const;

	UFUNCTION()
	void HandleRowClicked(int32 CommitmentID);

	UFUNCTION()
	void HandleBoardChanged();

	UFUNCTION()
	void HandleCommitmentResolved(const FForecastCommitment& Commitment);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleBoardTabClicked();

	UForecastSubsystem* GetForecast() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNotebookRowWidget>> Rows;

	/** What the list shows, newest first. Copies: the subsystem's array can
	    grow under us while the screen is open. */
	TArray<FForecastCommitment> Entries;
	int32 SelectedID = INDEX_NONE;
};
