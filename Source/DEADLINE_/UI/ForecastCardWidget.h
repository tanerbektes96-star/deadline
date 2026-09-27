// Copyright DEADLINE. All Rights Reserved.
//
// One entry on the forecast board: a signal or an event running.
//
// Read left to right, top to bottom, in the order the decision is made: what
// and when, how sure, how big, what it means for you. Everything is bound by
// name (CLAUDE.md UMG pattern); build_forecast_screen.py creates the tree.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Forecast/ForecastSubsystem.h"
#include "ForecastCardWidget.generated.h"

class UBorder;
class UButton;
class UProgressBar;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnForecastCardClicked, FName, EventID);

UCLASS()
class DEADLINE__API UForecastCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	void SetEntry(const FForecastEntry& Entry);

	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Forecast")
	FName GetEventID() const { return EventID; }

	FOnForecastCardClicked OnCardClicked;

protected:
	virtual void NativeConstruct() override;

	// --- Bound widgets. Exact names required. ------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CardButton;

	/** Outline of the card; highlighted when selected. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> CardFrame;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	/** "SÖYLENTİ · 2 gün sonra" / "SÜRÜYOR · gün 14'e kadar". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	/** The number first, as a number: "%75". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ConfidenceText;

	/** The same number as a bar, for scanning a column of cards. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> ConfidenceBar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> BodyText;

	/** "Olursa: +%20–35 · 4 ürün". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ImpactText;

	/** "Maruziyet $1,240" or "Elinde yok". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ExposureText;

private:
	UFUNCTION()
	void HandleClicked();

	FName EventID;
	bool bSelected = false;
};
