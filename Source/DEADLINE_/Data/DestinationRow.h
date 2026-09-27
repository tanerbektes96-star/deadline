// Copyright DEADLINE. All Rights Reserved.
//
// FDestinationRow — one row of Content/Deadline/Data/DT_Destinations.csv: a
// place the player can travel to (GDD 9.2, and the six supply channels of
// GDD 6.1 that give those places a reason to exist).
//
// The important thing about this table is what it does NOT contain: any road
// geometry. Because nobody ever drives (GDD 9.1), a destination is a point on a
// picture plus a category. Distance between two places is the distance between
// their marker positions, scaled by one tuning number, so the city needs no
// navigable layout and no plan — only a list of places. That is the production
// saving the no-driving decision was made for.
//
// MapX and MapY are 0..1 across the travel map image, so the same numbers place
// the marker on screen and price the route.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DestinationRow.generated.h"

USTRUCT(BlueprintType)
struct DEADLINE__API FDestinationRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destination")
	FString NameTR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destination")
	FString NameEN;

	/** Home / Supplier / Buyer / Port / Grey (GDD 6.1). Drives the marker
	    colour on the map and, from Month 7, which channels open there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destination")
	FName Category;

	/** Marker position across the travel map, 0..1 left to right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destination")
	float MapX = 0.f;

	/** Marker position down the travel map, 0..1 top to bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destination")
	float MapY = 0.f;

	/** Where the run starts. Exactly one row should have this set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Destination")
	bool IsHome = false;
};
