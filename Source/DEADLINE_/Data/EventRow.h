// Copyright DEADLINE. All Rights Reserved.
//
// FEventRow — one row of Content/Deadline/Data/DT_Events.csv: one kind of
// world event (GDD 13). The row is a template; UEventSubsystem rolls the
// actual dates, strength and duration from the run seed.
//
// An event reaches the game in one of two ways, picked by Kind:
//   Price  -- pushes the price of every product whose EventTags share a tag
//             with the event's Tags (DT_Products.csv). Impact is how far above
//             base price the event drags them, as a fraction: 0.30 = +30%.
//   Route  -- multiplies the fuel cost of every trip (GDD 9.4).
// A row can do both: E16 Fuel Spike makes trips dearer and fuel goods pricier.
//
// Import note, as for DT_Products: the first CSV column (ID) becomes the row
// name, and Tags is a raw "A|B" string because the CSV importer cannot parse a
// pipe-delimited array. Use GetTags().

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EventRow.generated.h"

USTRUCT(BlueprintType)
struct DEADLINE__API FEventRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FString NameTR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FString NameEN;

	/** Raw "Tag1|Tag2" string, matched against FProductRow::EventTags. Empty
	    for an event that touches no prices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FString Tags;

	/** Price push while active, as a fraction of base price. The roll lands
	    between Min and Max. 0 for an event that touches no prices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	float ImpactMin = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	float ImpactMax = 0.f;

	/** Length in whole days, inclusive range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	int32 DurationMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	int32 DurationMax = 1;

	/** Multiplier on every trip's fuel cost while active. 1 = no effect. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	float RouteCostMultiplier = 1.f;

	/** Relative chance of being the one rolled when the calendar adds an
	    event. 1 is ordinary; the big crises are rarer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	float Weight = 1.f;

	/** The signal before it starts: a rumour or a forecast (GDD 4, step 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FString SignalTR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FString SignalEN;

	/** The headline once it is actually happening. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FString HeadlineTR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Event")
	FString HeadlineEN;

	/** Split Tags ("A|B") into individual tags. */
	void GetTags(TArray<FName>& OutTags) const
	{
		OutTags.Reset();
		TArray<FString> Parts;
		Tags.ParseIntoArray(Parts, TEXT("|"), /*InCullEmpty=*/true);
		for (const FString& Part : Parts)
		{
			OutTags.Add(FName(*Part.TrimStartAndEnd()));
		}
	}
};
