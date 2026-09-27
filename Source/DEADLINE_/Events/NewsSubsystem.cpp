// Copyright DEADLINE. All Rights Reserved.

#include "Events/NewsSubsystem.h"

#include "Core/DeadlineLocale.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Economy/MarketSubsystem.h"
#include "Engine/GameInstance.h"
#include "Events/EventSubsystem.h"

void UNewsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UProductCatalogSubsystem::StaticClass());
	Collection.InitializeDependency(UTimeSubsystem::StaticClass());
	Collection.InitializeDependency(UEventSubsystem::StaticClass());
	Collection.InitializeDependency(UMarketSubsystem::StaticClass());
	Super::Initialize(Collection);

	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnHourChanged.AddDynamic(this, &UNewsSubsystem::HandleHourChanged);
	}
}

void UNewsSubsystem::Deinitialize()
{
	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnHourChanged.RemoveDynamic(this, &UNewsSubsystem::HandleHourChanged);
	}
	Super::Deinitialize();
}

// --- Building ------------------------------------------------------------------

FNewsBulletin UNewsSubsystem::BuildBulletin(int32 Day, int32 Hour) const
{
	FNewsBulletin Bulletin;
	Bulletin.Day = Day;
	Bulletin.Hour = Hour;
	if (Hour == MorningHour)
	{
		BuildMorning(Day, Bulletin.Items);
	}
	else if (Hour == MiddayHour)
	{
		BuildMidday(Day, Bulletin.Items);
	}
	return Bulletin;
}

void UNewsSubsystem::BuildMorning(int32 Day, TArray<FNewsItem>& Out) const
{
	const UEventSubsystem* Events = GetEvents();
	if (!Events)
	{
		return;
	}
	const TArray<FActiveEvent> Today = Events->GetActiveEvents(Day);

	// Started today.
	for (const FActiveEvent& E : Today)
	{
		if (E.StartDay != Day)
		{
			continue;
		}
		FNewsItem& Item = Out.AddDefaulted_GetRef();
		Item.Kind = ENewsKind::Headline;
		Item.EventID = E.EventID;
		Item.Title = E.Name;
		Item.Text = E.Headline;
		Item.RelatedDay = E.EndDay - 1;
		Item.Products = E.AffectedProducts;
	}

	if (Day > 0)
	{
		// Due today and did not start: the rumour was wrong. Worked out from
		// what the player could see -- yesterday's signal, today's events --
		// not from the calendar's answer.
		for (const FEventSignal& S : Events->GetSignals(Day - 1))
		{
			if (S.ExpectedDay != Day)
			{
				continue;
			}
			const bool bStarted = Today.ContainsByPredicate([&S, Day](const FActiveEvent& E)
			{
				return E.EventID == S.EventID && E.StartDay == Day;
			});
			if (bStarted)
			{
				continue;
			}
			FNewsItem& Item = Out.AddDefaulted_GetRef();
			Item.Kind = ENewsKind::Denied;
			Item.EventID = S.EventID;
			Item.Title = S.Name;
			Item.Text = DL_PRINTF("%s beklentisi gerçekleşmedi; söylenti yanlış çıktı.", "The expected %s never came; the rumour was wrong.", *S.Name);
			Item.Confidence = S.Confidence;
			Item.Products = S.AffectedProducts;
		}

		// Ended yesterday evening.
		for (const FActiveEvent& E : Events->GetActiveEvents(Day - 1))
		{
			if (E.EndDay != Day)
			{
				continue;
			}
			FNewsItem& Item = Out.AddDefaulted_GetRef();
			Item.Kind = ENewsKind::Ended;
			Item.EventID = E.EventID;
			Item.Title = E.Name;
			Item.Text = DL_PRINTF("%s sona erdi; fiyatlar normale dönüyor.", "%s is over; prices are settling back.", *E.Name);
			Item.Products = E.AffectedProducts;
		}
	}

	// New rumours.
	for (const FEventSignal& S : Events->GetSignals(Day))
	{
		if (S.SignalDay != Day)
		{
			continue;
		}
		FNewsItem& Item = Out.AddDefaulted_GetRef();
		Item.Kind = ENewsKind::Rumour;
		Item.EventID = S.EventID;
		Item.Title = S.Name;
		Item.Text = S.Text;
		Item.Confidence = S.Confidence;
		Item.RelatedDay = S.ExpectedDay;
		Item.Products = S.AffectedProducts;
	}
}

void UNewsSubsystem::BuildMidday(int32 Day, TArray<FNewsItem>& Out) const
{
	const UEventSubsystem* Events = GetEvents();
	const UMarketSubsystem* Market = GetMarket();
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	if (!Events || !Market)
	{
		return;
	}

	for (const FActiveEvent& E : Events->GetActiveEvents(Day))
	{
		FNewsItem Item;
		Item.Kind = ENewsKind::Update;
		Item.EventID = E.EventID;
		Item.Title = E.Name;

		// The biggest movers since the day before it began, largest first.
		struct FMove { FName ID; float Percent; };
		TArray<FMove> Moves;
		const int32 Before = FMath::Max(0, E.StartDay - 1);
		for (const FName& ID : E.AffectedProducts)
		{
			const float Then = Market->GetPriceOnDay(ID, Before);
			const float Now = Market->GetPriceOnDay(ID, Day);
			if (Then > 0.f)
			{
				Moves.Add({ ID, (Now / Then - 1.f) * 100.f });
			}
		}
		Moves.Sort([](const FMove& A, const FMove& B) { return FMath::Abs(A.Percent) > FMath::Abs(B.Percent); });

		TArray<FString> Parts;
		for (int32 i = 0; i < Moves.Num() && i < 3; ++i)
		{
			const FString Name = Catalogue ? Catalogue->GetDisplayName(Moves[i].ID) : Moves[i].ID.ToString();
			Parts.Add(FString::Printf(TEXT("%s %+.0f%%"), *Name, Moves[i].Percent));
			Item.Products.Add(Moves[i].ID);
		}
		if (E.RouteCostMultiplier > 1.f + KINDA_SMALL_NUMBER)
		{
			Parts.Add(DL_PRINTF("rotalar %+.0f%%", "routes %+.0f%%",
				(E.RouteCostMultiplier - 1.f) * 100.f));
		}
		if (Parts.Num() == 0)
		{
			continue;
		}
		Item.Text = FString::Printf(TEXT("%s: %s"), *E.Name, *FString::Join(Parts, TEXT(", ")));
		Item.RelatedDay = E.EndDay - 1;
		Out.Add(MoveTemp(Item));
	}
}

// --- Publishing ---------------------------------------------------------------

bool UNewsSubsystem::IsPublished(int32 Day, int32 Hour) const
{
	const UTimeSubsystem* Time = GetTime();
	return Time && Time->GetTotalMinutes() >= (static_cast<double>(Day) * 24.0 + Hour) * 60.0;
}

void UNewsSubsystem::HandleHourChanged(int32 Day, int32 Hour)
{
	if (Hour != MorningHour && Hour != MiddayHour)
	{
		return;
	}
	const FNewsBulletin Bulletin = BuildBulletin(Day, Hour);
	if (Bulletin.Items.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline] News: day %d %02d:00, %d items."), Day, Hour, Bulletin.Items.Num());
		OnBulletinPublished.Broadcast(Bulletin);
	}
}

TArray<FNewsBulletin> UNewsSubsystem::GetNewsLog(int32 MaxDays) const
{
	TArray<FNewsBulletin> Log;
	const UTimeSubsystem* Time = GetTime();
	if (!Time)
	{
		return Log;
	}
	const int32 Today = Time->GetDay();
	for (int32 Day = Today; Day >= 0 && Day > Today - FMath::Max(1, MaxDays); --Day)
	{
		for (const int32 Hour : { MiddayHour, MorningHour })
		{
			if (!IsPublished(Day, Hour))
			{
				continue;
			}
			FNewsBulletin Bulletin = BuildBulletin(Day, Hour);
			if (Bulletin.Items.Num() > 0)
			{
				Log.Add(MoveTemp(Bulletin));
			}
		}
	}
	return Log;
}

FNewsBulletin UNewsSubsystem::GetLatestBulletin() const
{
	const TArray<FNewsBulletin> Log = GetNewsLog(30);
	if (Log.Num() > 0)
	{
		return Log[0];
	}
	FNewsBulletin None;
	None.Day = -1;
	return None;
}

// --- Lookups ----------------------------------------------------------------

UEventSubsystem* UNewsSubsystem::GetEvents() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEventSubsystem>() : nullptr;
}

UMarketSubsystem* UNewsSubsystem::GetMarket() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMarketSubsystem>() : nullptr;
}

UProductCatalogSubsystem* UNewsSubsystem::GetCatalogue() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
}

UTimeSubsystem* UNewsSubsystem::GetTime() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
}
