// Copyright DEADLINE. All Rights Reserved.
//
// The dashed line between where you are and where you are thinking of going.
//
// It paints itself rather than being assembled from child widgets, for the same
// reason UMarketChartWidget does: a line between two arbitrary points is not
// something UMG's layout can express. Nothing else about it is special.
//
// The two endpoints are given in the same 0..1 map space the destination table
// uses, so the route is drawn from the numbers that priced it.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/DeadlineUIPalette.h"
#include "TravelRouteWidget.generated.h"

UCLASS()
class DEADLINE__API UTravelRouteWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Draw a route.
	 * @param InFrom  where you are, 0..1 across and down the map.
	 * @param InTo    where you are considering.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Travel")
	void SetRoute(FVector2D InFrom, FVector2D InTo);

	UFUNCTION(BlueprintCallable, Category = "Deadline|Travel")
	void ClearRoute();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deadline|Travel")
	FLinearColor RouteColour = DeadlineUI::Accent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deadline|Travel", meta = (ClampMin = "2"))
	float DashLength = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deadline|Travel", meta = (ClampMin = "2"))
	float GapLength = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deadline|Travel", meta = (ClampMin = "1"))
	float Thickness = 2.f;

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	FVector2D From = FVector2D::ZeroVector;
	FVector2D To = FVector2D::ZeroVector;
	bool bHasRoute = false;
};
