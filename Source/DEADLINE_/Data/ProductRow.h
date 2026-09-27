// Copyright DEADLINE. All Rights Reserved.
//
// FProductRow — one row of Content/Deadline/Data/DT_Products.csv.
//
// This is the C++ mirror of EconomyPrototype/products.py. Column names,
// order and types match the CSV exactly so the file imports straight into
// a DataTable (DT_Products) with this struct as the row type.
//
// Import note: the first CSV column (ID) becomes the DataTable RowName, so
// there is no ID field on the struct — read it from the row's FName key.
// EventTags is stored as the raw "Tag1|Tag2" string; call GetEventTags()
// to split it (Unreal's CSV importer cannot parse a pipe-delimited array).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ProductRow.generated.h"

/** Price volatility class. GDD 8.1 — drives the mean-reversion band. */
UENUM(BlueprintType)
enum class ERiskBand : uint8
{
	Stable,
	Reactive,
	Volatile,
	Crisis
};

/** Physical container. GDD 5.2 — every one maps to a Box Unit (BU) volume. */
UENUM(BlueprintType)
enum class EContainerType : uint8
{
	BoxS,
	BoxM,
	BoxL,
	Pallet,
	Crate,
	ColdTote,
	SecureCase
};

/** Quality tier. GDD 5.4 — only meaningful on the 8 tiered products.
    Unbranded is for grey / commodity goods that carry no brand at all
    (plain brown carton, no label) and therefore have no tier. */
UENUM(BlueprintType)
enum class EBrandTier : uint8
{
	Value,
	Standard,
	Premium,
	Unbranded
};

/** Hard limits and mean-reversion strength for one risk band (GDD 8.1 table).
    These are doubles, not floats, on purpose: they are multiplied into the
    price every simulated day, and the C++ series has to line up with the
    Python reference in EconomyPrototype. 0.15f and 0.15 are different numbers,
    and the difference compounds. */
USTRUCT(BlueprintType)
struct FRiskBandParams
{
	GENERATED_BODY()

	/** Daily noise amplitude, as a fraction of normal price (e.g. 0.03 = +/-3%). */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	double Volatility = 0.0;

	/** Hard floor as a multiple of BasePrice. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	double FloorMultiplier = 0.0;

	/** Hard ceiling as a multiple of BasePrice. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	double CeilingMultiplier = 0.0;

	/** Pull toward normal price each step (0..1). Higher = snaps back faster. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	double MeanReversionPull = 0.0;
};

USTRUCT(BlueprintType)
struct DEADLINE__API FProductRow : public FTableRowBase
{
	GENERATED_BODY()

	// --- Identity -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	FString NameTR;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	FString NameEN;

	/** Fictional brand. Empty for grey / unbranded goods (GDD 5.4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	FString Brand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	EBrandTier BrandTier = EBrandTier::Standard;

	/** Free-form: Staples, Food, Components, Precious, Grey, ... (GDD 5.1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Product")
	FName Category;

	// --- Economy -------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	ERiskBand RiskBand = ERiskBand::Stable;

	/** Base buy price in dollars. The mean-reversion "normal" price. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	float BasePrice = 0.f;

	/** Daily base demand volume. Also caps how fast stock can be traded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	float BaseDemand = 0.f;

	/** How much demand falls when price rises: Demand *= (Price/Base)^-Elasticity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	float Elasticity = 0.f;

	// --- Volume / handling (BU system, GDD 5.2) -----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logistics")
	EContainerType ContainerType = EContainerType::BoxM;

	/** Box Units for this container. Redundant with ContainerType but kept for
	    fast capacity math and CSV validation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logistics")
	float VolumeBU = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logistics")
	int32 UnitsPerContainer = 1;

	/** Per-container weight. Used for manual-carry and vehicle axle limits only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logistics")
	float WeightKg = 0.f;

	// --- Condition over time -----------------------------------------------

	/** Days until spoilage. 0 = never spoils. (Different from obsolescence!) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	int32 ShelfLifeDays = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	bool RequiresCold = false;

	/** Daily value loss as a fraction (electronics, fashion). Not spoilage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	float ObsolescencePerDay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	bool RequiresSecure = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	bool Fragile = false;

	// --- Grey market (GDD 7.3) -------------------------------------------

	/** None / Standard / Restricted / Prohibited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compliance")
	FName LicenseRequired;

	/** Pharma / Hazmat / Excise / Precious / Precursor / ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Compliance")
	FName LicenseType;

	// --- Simulation wiring -------------------------------------------------

	/** Raw "Tag1|Tag2|Tag3" string. Use GetEventTags() to split. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	FString EventTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	FName SubstituteGroup;

	/** EA / U1 / U2 — which release this product is active in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	FName LaunchPhase;

	// --- Helpers ---------------------------------------------------------

	/** Split EventTags ("A|B|C") into individual tags. */
	void GetEventTags(TArray<FName>& OutTags) const
	{
		OutTags.Reset();
		TArray<FString> Parts;
		EventTags.ParseIntoArray(Parts, TEXT("|"), /*InCullEmpty=*/true);
		for (const FString& Part : Parts)
		{
			OutTags.Add(FName(*Part.TrimStartAndEnd()));
		}
	}

	double PriceFloor() const   { return BasePrice * GetRiskBandParams(RiskBand).FloorMultiplier; }
	double PriceCeiling() const { return BasePrice * GetRiskBandParams(RiskBand).CeilingMultiplier; }

	/** GDD 8.1 risk-band table. Single source of truth for the economy core. */
	static FRiskBandParams GetRiskBandParams(ERiskBand Band)
	{
		FRiskBandParams P;
		switch (Band)
		{
		case ERiskBand::Stable:   P.Volatility = 0.03; P.FloorMultiplier = 0.85; P.CeilingMultiplier = 1.20; P.MeanReversionPull = 0.25; break;
		case ERiskBand::Reactive: P.Volatility = 0.07; P.FloorMultiplier = 0.70; P.CeilingMultiplier = 1.50; P.MeanReversionPull = 0.15; break;
		case ERiskBand::Volatile: P.Volatility = 0.14; P.FloorMultiplier = 0.50; P.CeilingMultiplier = 2.20; P.MeanReversionPull = 0.08; break;
		case ERiskBand::Crisis:   P.Volatility = 0.25; P.FloorMultiplier = 0.40; P.CeilingMultiplier = 6.00; P.MeanReversionPull = 0.04; break;
		}
		return P;
	}

	/** Box Unit volume for a container type (GDD 5.2). */
	static float ContainerVolumeBU(EContainerType Type)
	{
		switch (Type)
		{
		case EContainerType::BoxS:       return 0.5f;
		case EContainerType::BoxM:       return 1.0f;
		case EContainerType::BoxL:       return 2.0f;
		case EContainerType::Pallet:     return 16.0f;
		case EContainerType::Crate:      return 4.0f;
		case EContainerType::ColdTote:   return 1.0f;
		case EContainerType::SecureCase: return 0.5f;
		}
		return 0.f;
	}
};
