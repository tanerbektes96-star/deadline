// Copyright DEADLINE. All Rights Reserved.

#include "Economy/MarketSubsystem.h"

#include "Core/SaveSubsystem.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Economy/MarketModel.h"
#include "Engine/GameInstance.h"

void UMarketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UProductCatalogSubsystem::StaticClass());
	Collection.InitializeDependency(UTimeSubsystem::StaticClass());
	Super::Initialize(Collection);

	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnDayChanged.AddDynamic(this, &UMarketSubsystem::HandleDayChanged);
	}
	if (UProductCatalogSubsystem* Catalogue = GetCatalogue())
	{
		Catalogue->OnCatalogueReloaded.AddDynamic(this, &UMarketSubsystem::HandleCatalogueReloaded);
	}
}

void UMarketSubsystem::Deinitialize()
{
	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnDayChanged.RemoveDynamic(this, &UMarketSubsystem::HandleDayChanged);
	}
	if (UProductCatalogSubsystem* Catalogue = GetCatalogue())
	{
		Catalogue->OnCatalogueReloaded.RemoveDynamic(this, &UMarketSubsystem::HandleCatalogueReloaded);
	}
	States.Reset();
	Super::Deinitialize();
}

UProductCatalogSubsystem* UMarketSubsystem::GetCatalogue() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
}

UTimeSubsystem* UMarketSubsystem::GetTime() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
}

int32 UMarketSubsystem::GetRunSeed() const
{
	const UGameInstance* GI = GetGameInstance();
	const USaveSubsystem* Save = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
	return Save ? Save->GetGameSeed() : 0;
}

int32 UMarketSubsystem::GetMarketDay() const
{
	const UTimeSubsystem* Time = GetTime();
	return Time ? Time->GetDay() : 0;
}

void UMarketSubsystem::ResetAll()
{
	States.Reset();
	bSeedCached = false;
	CachedSeed = 0;
}

void UMarketSubsystem::HandleDayChanged(int32 NewDay)
{
	// Deliberately does NOT walk the catalogue. Prices are lazy: the day
	// rolling over only means the cached numbers are stale, and each product
	// works out its own new price the next time something reads it.
	OnMarketDayAdvanced.Broadcast(NewDay);
}

void UMarketSubsystem::HandleCatalogueReloaded()
{
	// Cached series were built from the old base prices and bands, and the
	// cache also holds no pointers into the table, so dropping it is enough.
	States.Reset();
	OnMarketDayAdvanced.Broadcast(GetMarketDay());
}

// --- State ------------------------------------------------------------------

UMarketSubsystem::FProductMarketState* UMarketSubsystem::FindOrCreateState(FName ProductID) const
{
	const int32 Seed = GetRunSeed();
	if (!bSeedCached || CachedSeed != Seed)
	{
		// New game or a load brought a different run seed. Everything cached
		// belongs to the old run.
		States.Reset();
		CachedSeed = Seed;
		bSeedCached = true;
	}

	if (FProductMarketState* Existing = States.Find(ProductID))
	{
		return Existing;
	}

	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (!Row)
	{
		return nullptr;
	}

	FProductMarketState NewState;
	NewState.BasePrice = static_cast<double>(Row->BasePrice);
	NewState.NormalPrice = NewState.BasePrice;
	NewState.Price = NewState.BasePrice;
	NewState.Band = Row->RiskBand;
	NewState.LastDay = 0;
	NewState.Stream = FMarketModel::MakeProductStream(Seed, ProductID);
	NewState.History.Add(static_cast<float>(NewState.BasePrice));

	return &States.Add(ProductID, MoveTemp(NewState));
}

void UMarketSubsystem::CatchUp(FProductMarketState& State, int32 TargetDay) const
{
	if (TargetDay <= State.LastDay)
	{
		return;
	}

	for (int32 Day = State.LastDay + 1; Day <= TargetDay; ++Day)
	{
		State.Price = FMarketModel::AdvanceOneDay(
			State.Price,
			State.NormalPrice,
			State.BasePrice,
			State.Band,
			// Month 4 wires the seeded event calendar in here (GDD 13).
			/*EventImpulse=*/0.0,
			// Month 7 wires hoarding pressure in here (GDD 7.8).
			/*PlayerPressure=*/0.0,
			State.Stream);
		State.History.Add(static_cast<float>(State.Price));
	}
	State.LastDay = TargetDay;
}

UMarketSubsystem::FProductMarketState* UMarketSubsystem::GetCurrentState(FName ProductID) const
{
	FProductMarketState* State = FindOrCreateState(ProductID);
	if (State)
	{
		CatchUp(*State, GetMarketDay());
	}
	return State;
}

// --- Queries ----------------------------------------------------------------

float UMarketSubsystem::GetPrice(FName ProductID) const
{
	const FProductMarketState* State = GetCurrentState(ProductID);
	return State ? static_cast<float>(State->Price) : 0.f;
}

float UMarketSubsystem::GetBasePrice(FName ProductID) const
{
	const FProductMarketState* State = FindOrCreateState(ProductID);
	return State ? static_cast<float>(State->BasePrice) : 0.f;
}

float UMarketSubsystem::GetPriceMultiple(FName ProductID) const
{
	const FProductMarketState* State = GetCurrentState(ProductID);
	if (!State || State->BasePrice <= 0.0)
	{
		return 1.f;
	}
	return static_cast<float>(State->Price / State->BasePrice);
}

float UMarketSubsystem::GetPriceOnDay(FName ProductID, int32 Day) const
{
	const FProductMarketState* State = GetCurrentState(ProductID);
	if (!State || State->History.Num() == 0)
	{
		return 0.f;
	}
	const int32 Index = FMath::Clamp(Day, 0, State->History.Num() - 1);
	return State->History[Index];
}

TArray<float> UMarketSubsystem::GetPriceHistory(FName ProductID, int32 MaxDays) const
{
	TArray<float> Result;
	const FProductMarketState* State = GetCurrentState(ProductID);
	if (!State || MaxDays <= 0)
	{
		return Result;
	}

	const int32 Count = FMath::Min(MaxDays, State->History.Num());
	const int32 First = State->History.Num() - Count;
	Result.Reserve(Count);
	for (int32 Index = First; Index < State->History.Num(); ++Index)
	{
		Result.Add(State->History[Index]);
	}
	return Result;
}

EPriceTrend UMarketSubsystem::GetTrend(FName ProductID) const
{
	const FProductMarketState* State = GetCurrentState(ProductID);
	if (!State || State->History.Num() < 2)
	{
		return EPriceTrend::Flat;
	}
	const float Today = State->History.Last();
	const float Yesterday = State->History[State->History.Num() - 2];
	// A tenth of a percent. Below that the arrow would flicker on noise.
	const float Threshold = FMath::Max(KINDA_SMALL_NUMBER, Yesterday * 0.001f);
	if (Today > Yesterday + Threshold)
	{
		return EPriceTrend::Rising;
	}
	if (Today < Yesterday - Threshold)
	{
		return EPriceTrend::Falling;
	}
	return EPriceTrend::Flat;
}

FMarketQuote UMarketSubsystem::GetQuote(FName ProductID) const
{
	FMarketQuote Quote;
	Quote.ProductID = ProductID;

	const FProductMarketState* State = GetCurrentState(ProductID);
	if (!State)
	{
		return Quote;
	}

	Quote.Price = static_cast<float>(State->Price);
	Quote.BasePrice = static_cast<float>(State->BasePrice);
	Quote.Multiple = (State->BasePrice > 0.0)
		? static_cast<float>(State->Price / State->BasePrice)
		: 1.f;
	Quote.RiskBand = State->Band;

	if (State->History.Num() >= 2)
	{
		const float Yesterday = State->History[State->History.Num() - 2];
		if (Yesterday > 0.f)
		{
			Quote.DayChangePercent = (Quote.Price - Yesterday) / Yesterday * 100.f;
		}
	}
	Quote.Trend = GetTrend(ProductID);
	return Quote;
}

TArray<FMarketQuote> UMarketSubsystem::GetAllQuotes() const
{
	TArray<FMarketQuote> Quotes;
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	if (!Catalogue)
	{
		return Quotes;
	}

	const TArray<FName> IDs = Catalogue->GetAllProductIDs();
	Quotes.Reserve(IDs.Num());
	for (const FName& ID : IDs)
	{
		Quotes.Add(GetQuote(ID));
	}
	return Quotes;
}

TArray<double> UMarketSubsystem::PeekSeries(FName ProductID, int32 Days) const
{
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	const FProductRow* Row = Catalogue ? Catalogue->FindProduct(ProductID) : nullptr;
	if (!Row)
	{
		return TArray<double>();
	}
	return FMarketModel::SimulateSeries(GetRunSeed(), ProductID,
		static_cast<double>(Row->BasePrice), Row->RiskBand, Days);
}
