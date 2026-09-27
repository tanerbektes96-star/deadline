// Copyright DEADLINE. All Rights Reserved.
//
// The price history chart from GDD 17: a product's recent closes, with your
// own average cost drawn across it, so "am I under water on this?" is one
// glance rather than arithmetic.
//
// It paints itself instead of being assembled from child widgets, because a
// polyline is not something UMG's layout can express. Nothing else about it is
// special: it is still a UUserWidget the designer drops into the screen.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/DeadlineUIPalette.h"
#include "MarketChartWidget.generated.h"

UCLASS()
class DEADLINE__API UMarketChartWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * What to draw.
	 * @param InPrices      Closes, oldest first.
	 * @param InAverageCost Your average cost per container, or <= 0 to omit
	 *                      the line (you hold none of this product).
	 * @param InFloor       Risk band floor, drawn as a faint guide.
	 * @param InCeiling     Risk band ceiling.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Market")
	void SetSeries(const TArray<float>& InPrices, float InAverageCost, float InFloor, float InCeiling);

	UFUNCTION(BlueprintCallable, Category = "Deadline|Market")
	void ClearSeries();

	// --- Colours, from Assets/DEADLINE_UI_Prompts.md 1.2 --------------------

	/** Price line. Cold blue, the interaction/accent colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deadline|Market")
	FLinearColor PriceColour = DeadlineUI::Accent;

	/** Average cost line. Amber: it is a warning line, not a good/bad one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deadline|Market")
	FLinearColor AverageCostColour = DeadlineUI::Warn;

	/** Band floor/ceiling guides and the baseline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deadline|Market")
	FLinearColor GuideColour = DeadlineUI::Line;

	/** Horizontal grid divisions. The panel needs to look like a chart even on
	    day one, when the price line is still a single straight segment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deadline|Market", meta = (ClampMin = "2", ClampMax = "12"))
	int32 GridLineCount = 5;

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** Map a price to a Y coordinate inside the widget. */
	float PriceToY(float Price, const FVector2D& Size) const;

	TArray<float> Prices;
	float AverageCost = 0.f;
	float BandFloor = 0.f;
	float BandCeiling = 0.f;

	/** Drawing range, worked out in SetSeries so paint stays cheap. */
	float MinPrice = 0.f;
	float MaxPrice = 1.f;
};
