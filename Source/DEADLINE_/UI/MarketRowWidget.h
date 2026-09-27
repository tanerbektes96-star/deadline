// Copyright DEADLINE. All Rights Reserved.
//
// One product line in the market list.
//
// Every child is bound by name (CLAUDE.md UMG pattern), so the Blueprint needs
// no graph at all: put widgets with these exact names in the Designer and the
// engine wires them up. A missing name is a compile error on the Blueprint,
// which is the point -- it fails loudly instead of silently showing nothing.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Economy/MarketSubsystem.h"
#include "MarketRowWidget.generated.h"

class UBorder;
class UButton;
class UMarketRowWidget;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarketRowClicked, FName, ProductID);

UCLASS()
class DEADLINE__API UMarketRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fill the row in. Called by UMarketScreenWidget, not by Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Market")
	void SetRow(const FMarketQuote& Quote, const FString& DisplayName, int32 ContainersHeld);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Market")
	FName GetProductID() const { return ProductID; }

	/** Highlights this row as the selected one. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Market")
	void SetSelected(bool bInSelected);

	FOnMarketRowClicked OnRowClicked;

protected:
	virtual void NativeConstruct() override;

	// --- Bound widgets. Name them exactly this in the Designer. ------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ProductNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PriceText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MultipleText;

	/** Percent change plus an up/down word -- never colour alone
	    (DEADLINE_UI_Prompts 1.2: colour-blind readers need a second signal). */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ChangeText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HeldText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SelectButton;

	/** Optional: the row still works without it. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RiskBandText;

	/** Optional colour chip for the risk band, as in DEADLINE_UI_Prompts 2.4.
	    Purely a scanning aid: RiskBandText spells the band out, so the colour
	    is never the only signal (UI prompts 1.2, colour blindness). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BandSwatch;

private:
	UFUNCTION()
	void HandleSelectClicked();

	FName ProductID;
	bool bSelected = false;
};
