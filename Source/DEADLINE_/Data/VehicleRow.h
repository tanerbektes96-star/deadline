// Copyright DEADLINE. All Rights Reserved.
//
// FVehicleRow — one row of Content/Deadline/Data/DT_Vehicles.csv, the GDD 9.5
// fleet table.
//
// GDD 9.5 gives the capacity and a word for speed ("Yavaş", "Orta", "Hızlı").
// Route cost (GDD 9.4) needs numbers, so speed, fuel and axle weight are
// placeholder figures derived from that column and live in the CSV precisely
// so they can be tuned without a rebuild (CLAUDE.md mistake #7). Capacity and
// the two carry restrictions are the GDD's own and should not drift.
//
// Import note: the first CSV column (ID) becomes the DataTable RowName, so
// there is no ID field on the struct — read it from the row's FName key.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "VehicleRow.generated.h"

USTRUCT(BlueprintType)
struct DEADLINE__API FVehicleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	FString NameTR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	FString NameEN;

	/** Cargo hold, in Box Units. Straight from the GDD 9.5 table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	float CapacityBU = 0.f;

	/** Cruising speed, km/h. Feeds travel time (GDD 9.4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	float SpeedKmh = 0.f;

	/** Litres per km when empty. Load weight adds up to 10% on top (GDD 9.4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	float FuelPerKm = 0.f;

	/** Axle limit. Kilograms are only used here and for hand carrying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	float MaxWeightKg = 0.f;

	/** False on the open bed (V03): a Secure Case will not ride on it (GDD 9.5). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	bool AllowsSecure = true;

	/** True on the reefer (V06): it carries Cold Totes and nothing else. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	bool ColdOnly = false;

	/** True on the forklift (V08): moves goods inside the yard, never travels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	bool WarehouseOnly = false;

	// Greybox body, in metres, until real vehicles arrive in Month 9. Three
	// floats rather than a vector so the CSV stays plain numbers.

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	float BodyLengthM = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	float BodyWidthM = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	float BodyHeightM = 2.2f;
};
