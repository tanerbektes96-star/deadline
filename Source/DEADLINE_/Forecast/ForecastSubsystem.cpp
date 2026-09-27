// Copyright DEADLINE. All Rights Reserved.

#include "Forecast/ForecastSubsystem.h"

#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Economy/EconomySubsystem.h"
#include "Economy/MarketSubsystem.h"
#include "Engine/GameInstance.h"
#include "Events/EventSubsystem.h"
#include "Fleet/FleetSubsystem.h"
#include "Inventory/InventorySubsystem.h"

void UForecastSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UProductCatalogSubsystem::StaticClass());
	Collection.InitializeDependency(UTimeSubsystem::StaticClass());
	Collection.InitializeDependency(UEventSubsystem::StaticClass());
	Collection.InitializeDependency(UMarketSubsystem::StaticClass());
	Collection.InitializeDependency(UEconomySubsystem::StaticClass());
	Collection.InitializeDependency(UInventorySubsystem::StaticClass());
	Collection.InitializeDependency(UFleetSubsystem::StaticClass());
	Super::Initialize(Collection);

	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnDayChanged.AddDynamic(this, &UForecastSubsystem::HandleDayChanged);
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
		{
			Inventory->OnStockChanged.AddDynamic(this, &UForecastSubsystem::HandleStockChanged);
		}
		if (UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			Fleet->OnCargoChanged.AddDynamic(this, &UForecastSubsystem::HandleStockChanged);
		}
	}
}

void UForecastSubsystem::Deinitialize()
{
	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnDayChanged.RemoveDynamic(this, &UForecastSubsystem::HandleDayChanged);
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
		{
			Inventory->OnStockChanged.RemoveDynamic(this, &UForecastSubsystem::HandleStockChanged);
		}
		if (UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			Fleet->OnCargoChanged.RemoveDynamic(this, &UForecastSubsystem::HandleStockChanged);
		}
	}
	Super::Deinitialize();
}

// --- Board ---------------------------------------------------------------------

TArray<FForecastEntry> UForecastSubsystem::GetBoard() const
{
	TArray<FForecastEntry> Board;
	const UEventSubsystem* Events = GetEvents();
	const UTimeSubsystem* Time = GetTime();
	if (!Events || !Time)
	{
		return Board;
	}
	const int32 Today = Time->GetDay();

	TArray<FEventSignal> Signals = Events->GetSignals(Today);
	Signals.Sort([](const FEventSignal& A, const FEventSignal& B) { return A.ExpectedDay < B.ExpectedDay; });
	for (const FEventSignal& S : Signals)
	{
		FForecastEntry& Entry = Board.AddDefaulted_GetRef();
		Entry.EventID = S.EventID;
		Entry.Status = EForecastStatus::Signal;
		Entry.Name = S.Name;
		Entry.Text = S.Text;
		Entry.Confidence = S.Confidence;
		Entry.Day = S.ExpectedDay;
		Entry.DaysAway = S.ExpectedDay - Today;
		Entry.RouteCostMultiplier = S.RouteCostMultiplier;
		Events->GetImpactRange(S.EventID, Entry.ImpactMin, Entry.ImpactMax);
		FillProducts(Entry, S.AffectedProducts, /*EventStartDay=*/INDEX_NONE);
	}

	for (const FActiveEvent& E : Events->GetActiveEvents(Today))
	{
		FForecastEntry& Entry = Board.AddDefaulted_GetRef();
		Entry.EventID = E.EventID;
		Entry.Status = EForecastStatus::Active;
		Entry.Name = E.Name;
		Entry.Text = E.Headline;
		Entry.Confidence = 1.f;
		Entry.Day = E.EndDay - 1;
		Entry.DaysAway = Entry.Day - Today;
		Entry.RouteCostMultiplier = E.RouteCostMultiplier;
		Events->GetImpactRange(E.EventID, Entry.ImpactMin, Entry.ImpactMax);
		FillProducts(Entry, E.AffectedProducts, E.StartDay);
	}
	return Board;
}

void UForecastSubsystem::FillProducts(FForecastEntry& Entry, const TArray<FName>& ProductIDs, int32 EventStartDay) const
{
	const UEconomySubsystem* Economy = GetEconomy();
	const UMarketSubsystem* Market = GetMarket();
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();

	for (const FName& ID : ProductIDs)
	{
		FForecastProductLine& Line = Entry.Products.AddDefaulted_GetRef();
		Line.ProductID = ID;
		Line.Name = Catalogue ? Catalogue->GetDisplayName(ID) : ID.ToString();
		Line.Price = Market ? Market->GetPrice(ID) : 0.f;
		if (Market && EventStartDay != INDEX_NONE)
		{
			const float Before = Market->GetPriceOnDay(ID, FMath::Max(0, EventStartDay - 1));
			Line.ChangePercent = Before > 0.f ? (Line.Price / Before - 1.f) * 100.f : 0.f;
		}
		Line.HeldContainers = Economy ? Economy->GetHeldContainers(ID) : 0.f;
		Line.HeldValue = Economy ? Economy->GetStockValueOf(ID) : 0.f;
		Entry.Exposure += Line.HeldValue;
	}

	// What you hold first, then the dearest: the lines that matter to you on top.
	Entry.Products.Sort([](const FForecastProductLine& A, const FForecastProductLine& B)
	{
		if ((A.HeldValue > 0.f) != (B.HeldValue > 0.f))
		{
			return A.HeldValue > 0.f;
		}
		return A.HeldValue != B.HeldValue ? A.HeldValue > B.HeldValue : A.Price > B.Price;
	});

	Entry.GainIfHappensMin = Entry.Exposure * Entry.ImpactMin;
	Entry.GainIfHappensMax = Entry.Exposure * Entry.ImpactMax;
}

float UForecastSubsystem::GetTotalExposure() const
{
	TSet<FName> Counted;
	float Total = 0.f;
	for (const FForecastEntry& Entry : GetBoard())
	{
		for (const FForecastProductLine& Line : Entry.Products)
		{
			bool bAlready = false;
			Counted.Add(Line.ProductID, &bAlready);
			if (!bAlready)
			{
				Total += Line.HeldValue;
			}
		}
	}
	return Total;
}

// --- Change notifications ----------------------------------------------------

void UForecastSubsystem::HandleDayChanged(int32 NewDay)
{
	OnBoardChanged.Broadcast();
}

void UForecastSubsystem::HandleStockChanged(FName ProductID)
{
	OnBoardChanged.Broadcast();
}

// --- Lookups ----------------------------------------------------------------

UEventSubsystem* UForecastSubsystem::GetEvents() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEventSubsystem>() : nullptr;
}

UEconomySubsystem* UForecastSubsystem::GetEconomy() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEconomySubsystem>() : nullptr;
}

UMarketSubsystem* UForecastSubsystem::GetMarket() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMarketSubsystem>() : nullptr;
}

UProductCatalogSubsystem* UForecastSubsystem::GetCatalogue() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
}

UTimeSubsystem* UForecastSubsystem::GetTime() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
}
