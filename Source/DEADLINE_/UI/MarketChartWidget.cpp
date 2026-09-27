// Copyright DEADLINE. All Rights Reserved.

#include "UI/MarketChartWidget.h"

#include "Rendering/DrawElements.h"

void UMarketChartWidget::SetSeries(const TArray<float>& InPrices, float InAverageCost,
	float InFloor, float InCeiling)
{
	Prices = InPrices;
	AverageCost = InAverageCost;
	BandFloor = InFloor;
	BandCeiling = InCeiling;

	MinPrice = TNumericLimits<float>::Max();
	MaxPrice = TNumericLimits<float>::Lowest();
	for (const float Price : Prices)
	{
		MinPrice = FMath::Min(MinPrice, Price);
		MaxPrice = FMath::Max(MaxPrice, Price);
	}
	if (AverageCost > 0.f)
	{
		// Keep the cost line on screen even when the price has run away from
		// it -- that is exactly the case the player needs to see.
		MinPrice = FMath::Min(MinPrice, AverageCost);
		MaxPrice = FMath::Max(MaxPrice, AverageCost);
	}

	if (Prices.Num() == 0)
	{
		MinPrice = 0.f;
		MaxPrice = 1.f;
	}
	else if (FMath::IsNearlyEqual(MinPrice, MaxPrice))
	{
		// A dead flat series would divide by zero. Give it some room.
		const float FlatRoom = FMath::Max(1.f, FMath::Abs(MaxPrice) * 0.05f);
		MinPrice -= FlatRoom;
		MaxPrice += FlatRoom;
	}
	else
	{
		const float Headroom = (MaxPrice - MinPrice) * 0.08f;
		MinPrice -= Headroom;
		MaxPrice += Headroom;
	}

	Invalidate(EInvalidateWidgetReason::Paint);
}

void UMarketChartWidget::ClearSeries()
{
	Prices.Reset();
	AverageCost = 0.f;
	BandFloor = 0.f;
	BandCeiling = 0.f;
	MinPrice = 0.f;
	MaxPrice = 1.f;
	Invalidate(EInvalidateWidgetReason::Paint);
}

float UMarketChartWidget::PriceToY(float Price, const FVector2D& Size) const
{
	const float Span = FMath::Max(KINDA_SMALL_NUMBER, MaxPrice - MinPrice);
	const float Normalised = (Price - MinPrice) / Span;
	// Y grows downward, so a high price sits near the top.
	return static_cast<float>(Size.Y) * (1.f - FMath::Clamp(Normalised, 0.f, 1.f));
}

int32 UMarketChartWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X <= 1.0 || Size.Y <= 1.0)
	{
		return LayerId;
	}

	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry();
	const ESlateDrawEffect Effects = bParentEnabled
		? ESlateDrawEffect::None
		: ESlateDrawEffect::DisabledEffect;

	// Band guides first, so the price line paints over them.
	auto DrawHorizontal = [&](float Price, const FLinearColor& Colour, float Thickness)
	{
		if (Price <= 0.f || Price < MinPrice || Price > MaxPrice)
		{
			return;
		}
		const float Y = PriceToY(Price, Size);
		TArray<FVector2D> Line = { FVector2D(0.0, Y), FVector2D(Size.X, Y) };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry, Line,
			Effects, Colour, /*bAntiAlias=*/false, Thickness);
	};

	// An evenly spaced grid, so the panel reads as a chart even before the
	// price has wandered anywhere (DEADLINE_UI_Prompts 2.4: "a faint
	// horizontal grid"). Drawn first and faintly; everything else sits on top.
	{
		FLinearColor GridColour = GuideColour;
		GridColour.A *= 0.55f;
		for (int32 Line = 1; Line < GridLineCount; ++Line)
		{
			const float Y = static_cast<float>(Size.Y) * Line / GridLineCount;
			TArray<FVector2D> Grid = { FVector2D(0.0, Y), FVector2D(Size.X, Y) };
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId, Geometry, Grid,
				Effects, GridColour, /*bAntiAlias=*/false, 1.f);
		}
	}

	DrawHorizontal(BandFloor, GuideColour, 1.f);
	DrawHorizontal(BandCeiling, GuideColour, 1.f);
	DrawHorizontal(AverageCost, AverageCostColour, 2.f);

	// One price is day zero of a new run: there is no line to draw yet, but the
	// gridded panel above still tells the player they are looking at a chart.
	if (Prices.Num() < 2)
	{
		return LayerId;
	}

	TArray<FVector2D> Points;
	Points.Reserve(Prices.Num());
	const double StepX = Size.X / static_cast<double>(Prices.Num() - 1);
	for (int32 Index = 0; Index < Prices.Num(); ++Index)
	{
		Points.Add(FVector2D(StepX * Index, PriceToY(Prices[Index], Size)));
	}

	FSlateDrawElement::MakeLines(OutDrawElements, ++LayerId, Geometry, Points,
		Effects, PriceColour, /*bAntiAlias=*/true, 2.f);

	return LayerId;
}
