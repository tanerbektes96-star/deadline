// Copyright DEADLINE. All Rights Reserved.

#include "Events/EventCalendar.h"

#include "Core/DeadlineSettings.h"
#include "Data/EventRow.h"
#include "Economy/MarketModel.h"

const TCHAR* FEventCalendar::EventStreamKey = TEXT("EventStream");

FEventTemplate FEventTemplate::FromRow(FName InEventID, const FEventRow& Row)
{
	FEventTemplate T;
	T.EventID = InEventID;
	Row.GetTags(T.Tags);
	T.ImpactMin = FMath::Min(Row.ImpactMin, Row.ImpactMax);
	T.ImpactMax = FMath::Max(Row.ImpactMin, Row.ImpactMax);
	T.DurationMin = FMath::Max(1, FMath::Min(Row.DurationMin, Row.DurationMax));
	T.DurationMax = FMath::Max(T.DurationMin, Row.DurationMax);
	T.RouteCostMultiplier = FMath::Max(0.f, Row.RouteCostMultiplier);
	T.Weight = FMath::Max(0.f, Row.Weight);
	return T;
}

FEventCalendarParams FEventCalendarParams::FromSettings()
{
	const UDeadlineSettings& S = UDeadlineSettings::Get();
	FEventCalendarParams P;
	P.DailyChance = FMath::Clamp(S.EventDailyChance, 0.f, 1.f);
	P.MaxConcurrent = FMath::Max(1, S.MaxConcurrentEvents);
	P.LeadDaysMin = FMath::Max(1, FMath::Min(S.SignalLeadDaysMin, S.SignalLeadDaysMax));
	P.LeadDaysMax = FMath::Max(P.LeadDaysMin, S.SignalLeadDaysMax);
	P.ConfidenceMin = FMath::Clamp(FMath::Min(S.SignalConfidenceMin, S.SignalConfidenceMax), 0.f, 1.f);
	P.ConfidenceMax = FMath::Clamp(FMath::Max(S.SignalConfidenceMin, S.SignalConfidenceMax), 0.f, 1.f);
	return P;
}

void FEventCalendar::Reset(int32 BaseSeed, TArray<FEventTemplate> InTemplates, const FEventCalendarParams& InParams)
{
	Templates = MoveTemp(InTemplates);
	Templates.Sort([](const FEventTemplate& A, const FEventTemplate& B)
	{
		return A.EventID.LexicalLess(B.EventID);
	});
	Params = InParams;
	Stream.Initialize(DeadlineSeed::Derive(BaseSeed, EventStreamKey));
	Entries.Reset();
	RolledThrough = -1;
}

void FEventCalendar::EnsureRolledThrough(int32 Day)
{
	while (RolledThrough < Day)
	{
		RollDay(RolledThrough + 1);
		++RolledThrough;
	}
}

const FEventTemplate* FEventCalendar::FindTemplate(FName EventID) const
{
	return Templates.FindByPredicate([EventID](const FEventTemplate& T) { return T.EventID == EventID; });
}

void FEventCalendar::RollDay(int32 Day)
{
	// Always the same draws in the same order, used or not.
	const float RollChance = Stream.GetFraction();
	const float RollPick = Stream.GetFraction();
	const float RollImpact = Stream.GetFraction();
	const float RollDuration = Stream.GetFraction();
	const float RollLead = Stream.GetFraction();
	const float RollConfidence = Stream.GetFraction();
	const float RollTruth = Stream.GetFraction();
	static_assert(DrawsPerDay == 7, "Keep DrawsPerDay in step with the draws above.");

	if (RollChance >= Params.DailyChance || Templates.Num() == 0)
	{
		return;
	}

	int32 InFlight = 0;
	TSet<FName> Busy;
	for (const FScheduledEvent& E : Entries)
	{
		if (E.IsInFlightOn(Day))
		{
			++InFlight;
			Busy.Add(E.EventID);
		}
	}
	if (InFlight >= Params.MaxConcurrent)
	{
		return;
	}

	// Weighted pick among the kinds not already in flight: two overlapping
	// heatwaves would read as a bug, not as weather.
	float TotalWeight = 0.f;
	for (const FEventTemplate& T : Templates)
	{
		if (!Busy.Contains(T.EventID))
		{
			TotalWeight += T.Weight;
		}
	}
	if (TotalWeight <= 0.f)
	{
		return;
	}
	const FEventTemplate* Picked = nullptr;
	float Cursor = RollPick * TotalWeight;
	for (const FEventTemplate& T : Templates)
	{
		if (Busy.Contains(T.EventID) || T.Weight <= 0.f)
		{
			continue;
		}
		Picked = &T;
		Cursor -= T.Weight;
		if (Cursor < 0.f)
		{
			break;
		}
	}
	check(Picked);

	const int32 Lead = Params.LeadDaysMin
		+ FMath::Min(FMath::FloorToInt(RollLead * (Params.LeadDaysMax - Params.LeadDaysMin + 1)),
			Params.LeadDaysMax - Params.LeadDaysMin);
	const int32 Duration = Picked->DurationMin
		+ FMath::Min(FMath::FloorToInt(RollDuration * (Picked->DurationMax - Picked->DurationMin + 1)),
			Picked->DurationMax - Picked->DurationMin);

	FScheduledEvent Entry;
	Entry.EventID = Picked->EventID;
	Entry.SignalDay = Day;
	Entry.StartDay = Day + Lead;
	Entry.EndDay = Entry.StartDay + Duration;
	Entry.Impact = FMath::Lerp(Picked->ImpactMin, Picked->ImpactMax, RollImpact);
	Entry.Confidence = FMath::Lerp(Params.ConfidenceMin, Params.ConfidenceMax, RollConfidence);
	// Calibrated by construction: P(happens) == the confidence on the signal.
	Entry.bHappens = RollTruth < Entry.Confidence;
	Entries.Add(Entry);
}
