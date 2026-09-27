// Copyright DEADLINE. All Rights Reserved.

#include "Events/EventSubsystem.h"

#include "Core/DeadlineSettings.h"
#include "Core/SaveSubsystem.h"
#include "Core/TimeSubsystem.h"
#include "Data/EventRow.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace
{
	bool IsTurkish()
	{
		return FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() == TEXT("tr");
	}

	/** TR when the game runs in Turkish and the row has it, EN otherwise --
	    the same rule as UProductCatalogSubsystem::GetDisplayName. */
	const FString& PickText(const FString& TR, const FString& EN)
	{
		return (IsTurkish() && !TR.IsEmpty()) ? TR : EN;
	}

	const FEventRow* FindRow(FName EventID)
	{
		const UDataTable* Table = UDeadlineSettings::Get().EventTable.LoadSynchronous();
		return Table ? Table->FindRow<FEventRow>(EventID, TEXT("UEventSubsystem"), false) : nullptr;
	}
}

void UEventSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UProductCatalogSubsystem::StaticClass());
	Collection.InitializeDependency(UTimeSubsystem::StaticClass());
	Super::Initialize(Collection);

	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnDayChanged.AddDynamic(this, &UEventSubsystem::HandleDayChanged);
	}
	if (UProductCatalogSubsystem* Catalogue = GetCatalogue())
	{
		Catalogue->OnCatalogueReloaded.AddDynamic(this, &UEventSubsystem::HandleCatalogueReloaded);
	}
	LastAnnouncedDay = GetToday();
}

void UEventSubsystem::Deinitialize()
{
	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnDayChanged.RemoveDynamic(this, &UEventSubsystem::HandleDayChanged);
	}
	if (UProductCatalogSubsystem* Catalogue = GetCatalogue())
	{
		Catalogue->OnCatalogueReloaded.RemoveDynamic(this, &UEventSubsystem::HandleCatalogueReloaded);
	}
	Super::Deinitialize();
}

// --- Calendar -----------------------------------------------------------------

const FEventCalendar& UEventSubsystem::GetCalendarThrough(int32 Day) const
{
	const int32 Seed = GetRunSeed();
	if (!bCalendarReady || CalendarSeed != Seed)
	{
		TArray<FEventTemplate> Templates;
		if (const UDataTable* Table = UDeadlineSettings::Get().EventTable.LoadSynchronous())
		{
			for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
			{
				Templates.Add(FEventTemplate::FromRow(Pair.Key, *reinterpret_cast<const FEventRow*>(Pair.Value)));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Deadline] DT_Events is not set in Project Settings > Deadline; no events."));
		}
		Calendar.Reset(Seed, MoveTemp(Templates), FEventCalendarParams::FromSettings());
		CalendarSeed = Seed;
		bCalendarReady = true;
	}
	Calendar.EnsureRolledThrough(Day);
	return Calendar;
}

void UEventSubsystem::GatherEntries(int32 Day, TArray<const FScheduledEvent*>& Out) const
{
	Out.Reset();
	for (const FScheduledEvent& E : GetCalendarThrough(Day).GetEntries())
	{
		Out.Add(&E);
	}
	for (const FScheduledEvent& E : ForcedEvents)
	{
		Out.Add(&E);
	}
}

TArray<FScheduledEvent> UEventSubsystem::GetCalendar(int32 FromDay, int32 ToDay) const
{
	TArray<const FScheduledEvent*> All;
	GatherEntries(ToDay, All);
	TArray<FScheduledEvent> Result;
	for (const FScheduledEvent* E : All)
	{
		if (E->SignalDay >= FromDay && E->SignalDay <= ToDay)
		{
			Result.Add(*E);
		}
	}
	Result.Sort([](const FScheduledEvent& A, const FScheduledEvent& B) { return A.SignalDay < B.SignalDay; });
	return Result;
}

// --- Player-facing views -------------------------------------------------------

TArray<FEventSignal> UEventSubsystem::GetSignals(int32 Day) const
{
	TArray<const FScheduledEvent*> All;
	GatherEntries(Day, All);
	TArray<FEventSignal> Result;
	for (const FScheduledEvent* E : All)
	{
		if (E->IsSignalledOn(Day))
		{
			Result.Add(MakeSignal(*E));
		}
	}
	return Result;
}

TArray<FActiveEvent> UEventSubsystem::GetActiveEvents(int32 Day) const
{
	TArray<const FScheduledEvent*> All;
	GatherEntries(Day, All);
	TArray<FActiveEvent> Result;
	for (const FScheduledEvent* E : All)
	{
		if (E->IsActiveOn(Day))
		{
			Result.Add(MakeActive(*E));
		}
	}
	return Result;
}

TArray<FEventSignal> UEventSubsystem::GetSignalsToday() const
{
	return GetSignals(GetToday());
}

TArray<FActiveEvent> UEventSubsystem::GetActiveEventsToday() const
{
	return GetActiveEvents(GetToday());
}

FEventSignal UEventSubsystem::MakeSignal(const FScheduledEvent& Entry) const
{
	FEventSignal Signal;
	Signal.EventID = Entry.EventID;
	Signal.Confidence = Entry.Confidence;
	Signal.SignalDay = Entry.SignalDay;
	Signal.ExpectedDay = Entry.StartDay;
	if (const FEventRow* Row = FindRow(Entry.EventID))
	{
		Signal.Name = PickText(Row->NameTR, Row->NameEN);
		Signal.Text = PickText(Row->SignalTR, Row->SignalEN);
		Signal.RouteCostMultiplier = Row->RouteCostMultiplier;
	}
	Signal.AffectedProducts = GetAffectedProducts(Entry.EventID);
	return Signal;
}

FActiveEvent UEventSubsystem::MakeActive(const FScheduledEvent& Entry) const
{
	FActiveEvent Active;
	Active.EventID = Entry.EventID;
	Active.StartDay = Entry.StartDay;
	Active.EndDay = Entry.EndDay;
	Active.Impact = Entry.Impact;
	Active.bForced = Entry.bForced;
	if (const FEventRow* Row = FindRow(Entry.EventID))
	{
		Active.Name = PickText(Row->NameTR, Row->NameEN);
		Active.Headline = PickText(Row->HeadlineTR, Row->HeadlineEN);
		Active.RouteCostMultiplier = Row->RouteCostMultiplier;
	}
	Active.AffectedProducts = GetAffectedProducts(Entry.EventID);
	return Active;
}

FString UEventSubsystem::GetEventName(FName EventID) const
{
	const FEventRow* Row = FindRow(EventID);
	return Row ? PickText(Row->NameTR, Row->NameEN) : EventID.ToString();
}

// --- Simulation hooks --------------------------------------------------------

const TArray<FName>& UEventSubsystem::GetProductTags(FName ProductID) const
{
	if (const TArray<FName>* Cached = ProductTags.Find(ProductID))
	{
		return *Cached;
	}
	TArray<FName> Tags;
	if (const UProductCatalogSubsystem* Catalogue = GetCatalogue())
	{
		if (const FProductRow* Row = Catalogue->FindProduct(ProductID))
		{
			Row->GetEventTags(Tags);
		}
	}
	return ProductTags.Add(ProductID, MoveTemp(Tags));
}

bool UEventSubsystem::TemplateTouchesProduct(const FEventTemplate& Template, FName ProductID) const
{
	const TArray<FName>& Tags = GetProductTags(ProductID);
	for (const FName& Tag : Template.Tags)
	{
		if (Tags.Contains(Tag))
		{
			return true;
		}
	}
	return false;
}

TArray<FName> UEventSubsystem::GetAffectedProducts(FName EventID) const
{
	TArray<FName> Result;
	const FEventTemplate* Template = GetCalendarThrough(0).FindTemplate(EventID);
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();
	if (!Template || !Catalogue || Template->Tags.Num() == 0)
	{
		return Result;
	}
	for (const FName& ProductID : Catalogue->GetAllProductIDs())
	{
		if (TemplateTouchesProduct(*Template, ProductID))
		{
			Result.Add(ProductID);
		}
	}
	return Result;
}

float UEventSubsystem::GetPriceTarget(FName ProductID, int32 Day) const
{
	TArray<const FScheduledEvent*> All;
	GatherEntries(Day, All);
	double Target = 0.0;
	for (const FScheduledEvent* E : All)
	{
		if (!E->IsActiveOn(Day))
		{
			continue;
		}
		const FEventTemplate* Template = Calendar.FindTemplate(E->EventID);
		if (Template && TemplateTouchesProduct(*Template, ProductID))
		{
			Target += E->Impact;
		}
	}
	const float Cap = UDeadlineSettings::Get().MaxEventTarget;
	return FMath::Clamp(static_cast<float>(Target), -Cap, Cap);
}

float UEventSubsystem::GetRouteCostMultiplier(int32 Day) const
{
	TArray<const FScheduledEvent*> All;
	GatherEntries(Day, All);
	float Multiplier = 1.f;
	for (const FScheduledEvent* E : All)
	{
		if (!E->IsActiveOn(Day))
		{
			continue;
		}
		if (const FEventTemplate* Template = Calendar.FindTemplate(E->EventID))
		{
			Multiplier *= Template->RouteCostMultiplier;
		}
	}
	return Multiplier;
}

// --- Changes ----------------------------------------------------------------

bool UEventSubsystem::ForceEvent(FName EventID, float Impact, int32 Duration)
{
	const int32 Today = GetToday();
	const FEventTemplate* Template = GetCalendarThrough(Today).FindTemplate(EventID);
	if (!Template)
	{
		return false;
	}

	FScheduledEvent Entry;
	Entry.EventID = EventID;
	Entry.SignalDay = Today;
	Entry.StartDay = Today;
	Entry.EndDay = Today + (Duration >= 1 ? Duration : FMath::RandRange(Template->DurationMin, Template->DurationMax));
	Entry.Impact = Impact >= 0.f ? Impact : FMath::FRandRange(Template->ImpactMin, Template->ImpactMax);
	Entry.Confidence = 1.f;
	Entry.bHappens = true;
	Entry.bForced = true;
	ForcedEvents.Add(Entry);

	OnCalendarChanged.Broadcast();
	OnEventStarted.Broadcast(MakeActive(Entry));
	return true;
}

void UEventSubsystem::ResetAll()
{
	ForcedEvents.Reset();
	ProductTags.Reset();
	LastAnnouncedDay = GetToday();
	Invalidate();
}

void UEventSubsystem::Invalidate()
{
	bCalendarReady = false;
	OnCalendarChanged.Broadcast();
}

void UEventSubsystem::HandleCatalogueReloaded()
{
	// Tags may have changed. The calendar itself only depends on DT_Events,
	// but the market rebuilds on a catalogue reload anyway.
	ProductTags.Reset();
}

void UEventSubsystem::HandleDayChanged(int32 NewDay)
{
	// Report every day between the last one announced and today, in order, so
	// a long trip or a time-skip does not swallow what happened meanwhile.
	for (int32 Day = LastAnnouncedDay + 1; Day <= NewDay; ++Day)
	{
		TArray<const FScheduledEvent*> All;
		GatherEntries(Day, All);

		for (const FScheduledEvent* E : All)
		{
			if (E->bHappens && E->EndDay == Day)
			{
				OnEventEnded.Broadcast(E->EventID);
			}
		}
		for (const FScheduledEvent* E : All)
		{
			// Forced events announce themselves when forced.
			if (E->StartDay == Day && !E->bForced)
			{
				OnSignalResolved.Broadcast(MakeSignal(*E), E->bHappens);
				if (E->bHappens)
				{
					OnEventStarted.Broadcast(MakeActive(*E));
				}
			}
		}
		for (const FScheduledEvent* E : All)
		{
			if (E->SignalDay == Day && !E->bForced)
			{
				OnEventSignalled.Broadcast(MakeSignal(*E));
			}
		}
	}
	LastAnnouncedDay = FMath::Max(LastAnnouncedDay, NewDay);
}

// --- Lookups ----------------------------------------------------------------

UProductCatalogSubsystem* UEventSubsystem::GetCatalogue() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
}

UTimeSubsystem* UEventSubsystem::GetTime() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
}

int32 UEventSubsystem::GetRunSeed() const
{
	const UGameInstance* GI = GetGameInstance();
	const USaveSubsystem* Save = GI ? GI->GetSubsystem<USaveSubsystem>() : nullptr;
	return Save ? Save->GetGameSeed() : 0;
}

int32 UEventSubsystem::GetToday() const
{
	const UTimeSubsystem* Time = GetTime();
	return Time ? Time->GetDay() : 0;
}
