// Copyright DEADLINE. All Rights Reserved.
//
// One line in the notebook: a forecast you committed to, judged or still
// running. Read left to right: when, what, did it come, what it made you.
// Everything is bound by name (CLAUDE.md UMG pattern); build_forecast_screen.py
// creates the tree.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Forecast/ForecastSubsystem.h"
#include "NotebookRowWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNotebookRowClicked, int32, CommitmentID);

UCLASS()
class DEADLINE__API UNotebookRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fill the row. EventName and ProductName come in resolved, so the row
	    does not look up catalogues once per line. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	void SetCommitment(const FForecastCommitment& Commitment, const FString& EventName, const FString& ProductName);

	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Forecast")
	int32 GetCommitmentID() const { return CommitmentID; }

	FOnNotebookRowClicked OnRowClicked;

protected:
	virtual void NativeConstruct() override;

	// --- Bound widgets. Exact names required. ------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RowButton;

	/** Panel, or raised when selected. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> RowFrame;

	/** A thin strip in the outcome's colour, so a column of rows reads as a
	    run of wins and losses at a glance. Never the only signal: the amount
	    is printed beside it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ToneStrip;

	/** "GÜN 14". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> DayText;

	/** Event · product. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	/** "TUTTU · kaynak %75" / "SÜRÜYOR · 16. gün değerlendirilir". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** "+$1,240", "İŞLEM YOK", or "—" while live. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ResultText;

private:
	UFUNCTION()
	void HandleClicked();

	int32 CommitmentID = INDEX_NONE;
	bool bSelected = false;
};
