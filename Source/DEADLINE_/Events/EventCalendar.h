// Copyright DEADLINE. All Rights Reserved.
//
// The seeded event calendar (GDD 13) as plain data, with no UObject and no
// world, for the same reason MarketModel.h is kept apart: the automation tests
// have to be able to roll a hundred calendars without launching a game.
//
// The calendar is a pure function of (run seed, event table, tuning). Nothing
// about it is saved: load a game and the same seed rolls the same events on
// the same days, which is what lets the market stay lazily evaluated -- a
// product's price on day 40 depends on the events of days 1..40, and those can
// be rolled at any time without having watched them happen.
//
// Every entry starts life as a signal (GDD 4, step 1: "sinyaller üretilir;
// bazıları doğru, bazıları yanıltıcı"). The signal carries a confidence, and
// the entry comes true with exactly that probability, so the number the player
// is shown is honest: across a run, 70% signals come true 70% of the time.
// When a forecast goes wrong, it was the risk the player misjudged, not a
// hidden coin that was rigged against them.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "EventCalendar.generated.h"

struct FEventRow;

/** One entry in the calendar: a signal, and the event it may turn into. */
USTRUCT(BlueprintType)
struct DEADLINE__API FScheduledEvent
{
	GENERATED_BODY()

	/** DT_Events row. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FName EventID;

	/** First day the signal is out. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	int32 SignalDay = 0;

	/** First day the event is in effect, if it happens. For a false rumour,
	    the day it is found out. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	int32 StartDay = 0;

	/** First day it is no longer in effect. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	int32 EndDay = 0;

	/** Price push as a fraction of base price (0.3 = drags prices toward +30%). */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	float Impact = 0.f;

	/** The confidence shown with the signal, 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	float Confidence = 0.f;

	/** Whether it comes true. The answer to the forecast: nothing shown to the
	    player before StartDay may read this. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	bool bHappens = false;

	/** Added by a cheat rather than rolled. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	bool bForced = false;

	bool IsSignalledOn(int32 Day) const { return Day >= SignalDay && Day < StartDay; }
	bool IsActiveOn(int32 Day) const { return bHappens && Day >= StartDay && Day < EndDay; }

	/** Still taking up a slot on Day: signalled, or happening. A rumour stops
	    taking one the day it is found out. */
	bool IsInFlightOn(int32 Day) const { return Day >= SignalDay && Day < (bHappens ? EndDay : StartDay); }
};

/** The parts of an FEventRow the calendar rolls from. */
struct DEADLINE__API FEventTemplate
{
	FName EventID;
	TArray<FName> Tags;
	float ImpactMin = 0.f;
	float ImpactMax = 0.f;
	int32 DurationMin = 1;
	int32 DurationMax = 1;
	float RouteCostMultiplier = 1.f;
	float Weight = 1.f;

	static FEventTemplate FromRow(FName EventID, const FEventRow& Row);
};

/** Tuning, normally read from UDeadlineSettings. */
struct DEADLINE__API FEventCalendarParams
{
	float DailyChance = 0.4f;
	int32 MaxConcurrent = 3;
	int32 LeadDaysMin = 1;
	int32 LeadDaysMax = 3;
	float ConfidenceMin = 0.55f;
	float ConfidenceMax = 0.95f;

	static FEventCalendarParams FromSettings();
};

class DEADLINE__API FEventCalendar
{
public:
	/** Stream name, part of the seed derivation (DeadlineSeed::Derive). */
	static const TCHAR* EventStreamKey;

	/** Draws made every day whether or not anything is added, so that
	    changing a tuning number does not shift every later day's rolls. */
	static constexpr int32 DrawsPerDay = 7;

	/** Start over for a run. Templates are sorted by ID, so the calendar does
	    not depend on the order rows happen to sit in the table. */
	void Reset(int32 BaseSeed, TArray<FEventTemplate> InTemplates, const FEventCalendarParams& InParams);

	/** Roll every day up to and including Day. Rolled days are never rolled
	    again, so asking for day 60 and then day 120 gives the same calendar
	    as asking for day 120 straight away. */
	void EnsureRolledThrough(int32 Day);

	/** Entries in the order they were rolled (by SignalDay). */
	const TArray<FScheduledEvent>& GetEntries() const { return Entries; }

	int32 GetRolledThrough() const { return RolledThrough; }

	const FEventTemplate* FindTemplate(FName EventID) const;
	const TArray<FEventTemplate>& GetTemplates() const { return Templates; }

private:
	void RollDay(int32 Day);

	TArray<FEventTemplate> Templates;
	FEventCalendarParams Params;
	FRandomStream Stream;
	TArray<FScheduledEvent> Entries;
	int32 RolledThrough = -1;
};
