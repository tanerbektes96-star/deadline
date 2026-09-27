// Copyright DEADLINE. All Rights Reserved.

#include "UI/TravelRouteWidget.h"

void UTravelRouteWidget::SetRoute(FVector2D InFrom, FVector2D InTo)
{
	From = InFrom;
	To = InTo;
	bHasRoute = true;
}

void UTravelRouteWidget::ClearRoute()
{
	bHasRoute = false;
}

int32 UTravelRouteWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 Result = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (!bHasRoute || Size.X <= 1.f || Size.Y <= 1.f)
	{
		return Result;
	}

	const FVector2D Start(From.X * Size.X, From.Y * Size.Y);
	const FVector2D End(To.X * Size.X, To.Y * Size.Y);

	const float Total = FVector2D::Distance(Start, End);
	if (Total < 1.f)
	{
		return Result;
	}

	const FVector2D Direction = (End - Start) / Total;
	const float Step = FMath::Max(4.f, DashLength + GapLength);

	// One draw call per dash. A route is a couple of dozen of them, which is
	// cheaper than the widget tree it would take to lay the same thing out.
	for (float Along = 0.f; Along < Total; Along += Step)
	{
		const float DashEnd = FMath::Min(Along + DashLength, Total);
		const TArray<FVector2D> Segment = {
			Start + Direction * Along,
			Start + Direction * DashEnd
		};
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(), Segment, ESlateDrawEffect::None,
			RouteColour, /*bAntiAlias=*/true, Thickness);
	}

	return Result + 1;
}
