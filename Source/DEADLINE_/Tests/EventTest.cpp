// Copyright DEADLINE. All Rights Reserved.
//
// World events (GDD 13). The calendar is invisible until it goes wrong, and
// when it goes wrong the symptom is "the market feels rigged", which nobody
// can debug from a playtest. So each promise the design makes is checked here:
//
//   EventData      every row is usable and every price event reaches products
//   Calendar       same seed -> same calendar however it is rolled; different
//                  seeds -> different calendars (the Month 4 gate test's
//                  "at least 6 events play out differently on a replay")
//   Calibration    a signal shown at 70% comes true 70% of the time
//   PriceImpulse   the price maths, and that an event-free series is still
//                  exactly the Month 2 series the parity test knows
//   Live           a forced event reaches prices and routes through the
//                  real subsystems
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "C:\Users\PC\Desktop\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.Events;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log

#include "CoreMinimal.h"
#include "Algo/Reverse.h"
#include "Core/DeadlineSettings.h"
#include "Core/SaveSubsystem.h"
#include "Core/TimeSubsystem.h"
#include "Data/EventRow.h"
#include "Data/ProductRow.h"
#include "Economy/MarketModel.h"
#include "Economy/MarketSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Events/EventCalendar.h"
#include "Events/EventSubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool LoadTemplates(FAutomationTestBase& Test, TArray<FEventTemplate>& Out)
	{
		const UDataTable* Table = UDeadlineSettings::Get().EventTable.LoadSynchronous();
		if (!Table)
		{
			Test.AddError(TEXT("DT_Events is not set in Project Settings > Deadline, or failed to load."));
			return false;
		}
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Out.Add(FEventTemplate::FromRow(Pair.Key, *reinterpret_cast<const FEventRow*>(Pair.Value)));
		}
		return Out.Num() > 0;
	}

	FEventCalendar Roll(const TArray<FEventTemplate>& Templates, int32 Seed, int32 Days)
	{
		FEventCalendar Calendar;
		Calendar.Reset(Seed, Templates, FEventCalendarParams::FromSettings());
		Calendar.EnsureRolledThrough(Days);
		return Calendar;
	}

	bool SameEntries(const TArray<FScheduledEvent>& A, const TArray<FScheduledEvent>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 i = 0; i < A.Num(); ++i)
		{
			const FScheduledEvent& X = A[i];
			const FScheduledEvent& Y = B[i];
			if (X.EventID != Y.EventID || X.SignalDay != Y.SignalDay || X.StartDay != Y.StartDay
				|| X.EndDay != Y.EndDay || X.Impact != Y.Impact || X.Confidence != Y.Confidence
				|| X.bHappens != Y.bHappens)
			{
				return false;
			}
		}
		return true;
	}

	UGameInstance* MakeInstance(int32 Seed)
	{
		UGameInstance* GI = NewObject<UGameInstance>(GEngine);
		if (GI)
		{
			GI->InitializeStandalone();
			if (USaveSubsystem* Save = GI->GetSubsystem<USaveSubsystem>())
			{
				Save->StartNewGame(Seed);
			}
		}
		return GI;
	}
}

// --- Data ---------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineEventDataTest,
	"Deadline.Events.EventData",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineEventDataTest::RunTest(const FString& Parameters)
{
	const UDataTable* Events = UDeadlineSettings::Get().EventTable.LoadSynchronous();
	const UDataTable* Products = UDeadlineSettings::Get().ProductTable.LoadSynchronous();
	if (!Events || !Products)
	{
		AddError(TEXT("DT_Events or DT_Products failed to load."));
		return false;
	}

	// Month 4 ships 12 of the 20 GDD events (roadmap: "12 olay").
	TestEqual(TEXT("Event count"), Events->GetRowMap().Num(), 12);

	TSet<FName> ProductTags;
	for (const TPair<FName, uint8*>& Pair : Products->GetRowMap())
	{
		TArray<FName> Tags;
		reinterpret_cast<const FProductRow*>(Pair.Value)->GetEventTags(Tags);
		ProductTags.Append(Tags);
	}

	for (const TPair<FName, uint8*>& Pair : Events->GetRowMap())
	{
		const FString ID = Pair.Key.ToString();
		const FEventRow& Row = *reinterpret_cast<const FEventRow*>(Pair.Value);

		TestFalse(*FString::Printf(TEXT("%s has names"), *ID), Row.NameTR.IsEmpty() || Row.NameEN.IsEmpty());
		TestFalse(*FString::Printf(TEXT("%s has signal text"), *ID), Row.SignalTR.IsEmpty() || Row.SignalEN.IsEmpty());
		TestFalse(*FString::Printf(TEXT("%s has a headline"), *ID), Row.HeadlineTR.IsEmpty() || Row.HeadlineEN.IsEmpty());
		TestTrue(*FString::Printf(TEXT("%s impact range"), *ID), Row.ImpactMin >= 0.f && Row.ImpactMin <= Row.ImpactMax);
		TestTrue(*FString::Printf(TEXT("%s duration range"), *ID), Row.DurationMin >= 1 && Row.DurationMin <= Row.DurationMax);
		TestTrue(*FString::Printf(TEXT("%s weight"), *ID), Row.Weight > 0.f);

		TArray<FName> Tags;
		Row.GetTags(Tags);
		const bool bRoute = !FMath::IsNearlyEqual(Row.RouteCostMultiplier, 1.f);
		TestTrue(*FString::Printf(TEXT("%s does something (tags or a route multiplier)"), *ID), Tags.Num() > 0 || bRoute);
		for (const FName& Tag : Tags)
		{
			// A tag no product carries is a silent no-op: almost always a typo.
			TestTrue(*FString::Printf(TEXT("%s tag '%s' matches a product"), *ID, *Tag.ToString()), ProductTags.Contains(Tag));
		}
		if (Tags.Num() > 0)
		{
			TestTrue(*FString::Printf(TEXT("%s pushes prices"), *ID), Row.ImpactMax > 0.f);
		}
	}
	return true;
}

// --- Calendar ---------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineEventCalendarTest,
	"Deadline.Events.Calendar",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineEventCalendarTest::RunTest(const FString& Parameters)
{
	TArray<FEventTemplate> Templates;
	if (!LoadTemplates(*this, Templates))
	{
		return false;
	}

	// Same seed, rolled in one go or in two steps: identical. This is what
	// lets the market catch a price up lazily and still be right.
	const FEventCalendar Whole = Roll(Templates, 1234, 200);
	FEventCalendar Stepped;
	Stepped.Reset(1234, Templates, FEventCalendarParams::FromSettings());
	Stepped.EnsureRolledThrough(60);
	Stepped.EnsureRolledThrough(200);
	TestTrue(TEXT("Rolling 0-60 then 61-200 equals rolling 0-200"), SameEntries(Whole.GetEntries(), Stepped.GetEntries()));

	// Table order must not matter.
	TArray<FEventTemplate> Reversed = Templates;
	Algo::Reverse(Reversed);
	TestTrue(TEXT("Row order does not change the calendar"),
		SameEntries(Whole.GetEntries(), Roll(Reversed, 1234, 200).GetEntries()));

	TestTrue(TEXT("200 days has a reasonable number of signals"),
		Whole.GetEntries().Num() >= 30 && Whole.GetEntries().Num() <= 120);

	// Different seeds give different runs, with enough variety to feel it:
	// the Month 4 gate asks for at least 6 events that play out differently.
	TSet<FName> Kinds;
	for (const FScheduledEvent& E : Whole.GetEntries())
	{
		if (E.bHappens)
		{
			Kinds.Add(E.EventID);
		}
	}
	TestTrue(*FString::Printf(TEXT("At least 6 kinds happen in 200 days (got %d)"), Kinds.Num()), Kinds.Num() >= 6);

	int32 Identical = 0;
	for (int32 Seed = 2; Seed <= 20; ++Seed)
	{
		Identical += SameEntries(Roll(Templates, Seed, 60).GetEntries(), Roll(Templates, Seed + 1000, 60).GetEntries()) ? 1 : 0;
	}
	TestEqual(TEXT("Different seeds never give the same 60 days"), Identical, 0);

	// Structural rules, over many runs.
	const FEventCalendarParams Params = FEventCalendarParams::FromSettings();
	int32 Violations = 0;
	for (int32 Seed = 1; Seed <= 50; ++Seed)
	{
		const FEventCalendar Cal = Roll(Templates, Seed, 200);
		for (int32 Day = 0; Day <= 200; ++Day)
		{
			int32 InFlight = 0;
			TSet<FName> Seen;
			for (const FScheduledEvent& E : Cal.GetEntries())
			{
				if (E.IsInFlightOn(Day))
				{
					++InFlight;
					bool bDuplicate = false;
					Seen.Add(E.EventID, &bDuplicate);
					Violations += bDuplicate ? 1 : 0;
				}
			}
			Violations += InFlight > Params.MaxConcurrent ? 1 : 0;
		}
		for (const FScheduledEvent& E : Cal.GetEntries())
		{
			const FEventTemplate* T = Cal.FindTemplate(E.EventID);
			const int32 Lead = E.StartDay - E.SignalDay;
			const int32 Length = E.EndDay - E.StartDay;
			Violations += (Lead < Params.LeadDaysMin || Lead > Params.LeadDaysMax) ? 1 : 0;
			Violations += (!T || Length < T->DurationMin || Length > T->DurationMax) ? 1 : 0;
			Violations += (!T || E.Impact < T->ImpactMin - KINDA_SMALL_NUMBER || E.Impact > T->ImpactMax + KINDA_SMALL_NUMBER) ? 1 : 0;
		}
	}
	TestEqual(TEXT("No overlap of one kind, no overflow, leads/lengths/impacts in range"), Violations, 0);
	return true;
}

// --- Calibration ------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineEventCalibrationTest,
	"Deadline.Events.Calibration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineEventCalibrationTest::RunTest(const FString& Parameters)
{
	TArray<FEventTemplate> Templates;
	if (!LoadTemplates(*this, Templates))
	{
		return false;
	}

	// Four confidence buckets; in each, the share that comes true should match
	// the mean confidence shown. Thousands of signals, so the tolerance is tight.
	constexpr int32 Buckets = 4;
	const FEventCalendarParams Params = FEventCalendarParams::FromSettings();
	double SumConf[Buckets] = {};
	int32 Count[Buckets] = {};
	int32 Hits[Buckets] = {};
	int32 Total = 0, False = 0;

	for (int32 Seed = 1; Seed <= 300; ++Seed)
	{
		for (const FScheduledEvent& E : Roll(Templates, Seed, 200).GetEntries())
		{
			const float Span = FMath::Max(Params.ConfidenceMax - Params.ConfidenceMin, KINDA_SMALL_NUMBER);
			const int32 B = FMath::Clamp(FMath::FloorToInt((E.Confidence - Params.ConfidenceMin) / Span * Buckets), 0, Buckets - 1);
			SumConf[B] += E.Confidence;
			Count[B] += 1;
			Hits[B] += E.bHappens ? 1 : 0;
			++Total;
			False += E.bHappens ? 0 : 1;
		}
	}

	for (int32 B = 0; B < Buckets; ++B)
	{
		if (Count[B] < 200)
		{
			AddError(FString::Printf(TEXT("Bucket %d has only %d signals"), B, Count[B]));
			continue;
		}
		const double Shown = SumConf[B] / Count[B];
		const double Actual = static_cast<double>(Hits[B]) / Count[B];
		TestTrue(*FString::Printf(TEXT("Bucket %d: shown %.0f%%, came true %.0f%% (n=%d)"),
			B, Shown * 100.0, Actual * 100.0, Count[B]), FMath::Abs(Shown - Actual) < 0.04);
	}

	// The design asks for about a quarter false rumours.
	const double FalseShare = static_cast<double>(False) / FMath::Max(1, Total);
	const double Expected = 1.0 - 0.5 * (Params.ConfidenceMin + Params.ConfidenceMax);
	TestTrue(*FString::Printf(TEXT("False rumours %.1f%% (expected %.1f%%)"), FalseShare * 100.0, Expected * 100.0),
		FMath::Abs(FalseShare - Expected) < 0.02);
	return true;
}

// --- Price maths --------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineEventPriceImpulseTest,
	"Deadline.Events.PriceImpulse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineEventPriceImpulseTest::RunTest(const FString& Parameters)
{
	// Without noise, one event day moves the price Rate of the way to target,
	// with mean reversion cancelled: 100 -> 100 + 0.45 * (150 - 100) = 122.5.
	const double Old = 100.0, Base = 100.0, Rate = 0.45;
	const ERiskBand Band = ERiskBand::Reactive;
	const double Pull = FProductRow::GetRiskBandParams(Band).MeanReversionPull;
	const double Impulse = FMarketModel::EventImpulse(Old, Base, Base, Band, 0.5, Rate);
	TestEqual(TEXT("One noise-free event day"), Old + Pull * (Base - Old) + Impulse * Base, 122.5, 1e-9);

	const double Above = 130.0;
	const double Impulse2 = FMarketModel::EventImpulse(Above, Base, Base, Band, 0.5, Rate);
	TestEqual(TEXT("Mean reversion is cancelled above base too"),
		Above + Pull * (Base - Above) + Impulse2 * Base, Above + Rate * (150.0 - Above), 1e-9);
	TestEqual(TEXT("No event, no impulse"), FMarketModel::EventImpulse(Above, Base, Base, Band, 0.0, Rate), 0.0);

	// An event-free series with events wired in is exactly the old series,
	// so the parity test against EconomyPrototype still means what it did.
	const FName Product(TEXT("E08"));
	const TArray<double> Plain = FMarketModel::SimulateSeries(77, Product, 450.0, ERiskBand::Volatile, 120);
	const TArray<double> Wired = FMarketModel::SimulateSeriesWithEvents(77, Product, 450.0, ERiskBand::Volatile, 120, Rate,
		[](int32) { return 0.0; });
	bool bSame = Plain.Num() == Wired.Num();
	for (int32 i = 0; bSame && i < Plain.Num(); ++i)
	{
		bSame = Plain[i] == Wired[i];
	}
	TestTrue(TEXT("Zero targets reproduce SimulateSeries bit for bit"), bSame);

	// A +80% event on days 20-26: untouched before, well up during, and the
	// pull brings it most of the way back afterwards. Same stream, so the
	// noise is identical and the difference is the event alone.
	const TArray<double> WithEvent = FMarketModel::SimulateSeriesWithEvents(77, Product, 450.0, ERiskBand::Volatile, 120, Rate,
		[](int32 Day) { return (Day >= 20 && Day < 27) ? 0.8 : 0.0; });
	TestEqual(TEXT("Day 19 untouched"), WithEvent[19], Plain[19]);
	const double Gap26 = WithEvent[26] - Plain[26];
	const double Gap60 = WithEvent[60] - Plain[60];
	TestTrue(*FString::Printf(TEXT("Day 26 lifted by the event (+$%.0f)"), Gap26), Gap26 > 0.5 * 0.8 * 450.0);
	TestTrue(*FString::Printf(TEXT("Mostly gone by day 60 (+$%.0f)"), Gap60), FMath::Abs(Gap60) < 0.25 * Gap26);
	return true;
}

// --- Live subsystems -----------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineEventLiveTest,
	"Deadline.Events.Live",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineEventLiveTest::RunTest(const FString& Parameters)
{
	constexpr int32 Seed = 4242;
	UGameInstance* WithGI = MakeInstance(Seed);
	UGameInstance* WithoutGI = MakeInstance(Seed);
	if (!WithGI || !WithoutGI)
	{
		AddError(TEXT("Could not create game instances"));
		return false;
	}

	UEventSubsystem* Events = WithGI->GetSubsystem<UEventSubsystem>();
	UEventSubsystem* EventsRef = WithoutGI->GetSubsystem<UEventSubsystem>();
	UTimeSubsystem* Time = WithGI->GetSubsystem<UTimeSubsystem>();
	UTimeSubsystem* TimeRef = WithoutGI->GetSubsystem<UTimeSubsystem>();
	UMarketSubsystem* Market = WithGI->GetSubsystem<UMarketSubsystem>();
	UMarketSubsystem* MarketRef = WithoutGI->GetSubsystem<UMarketSubsystem>();
	if (!Events || !EventsRef || !Time || !TimeRef || !Market || !MarketRef)
	{
		AddError(TEXT("Missing subsystems"));
		return false;
	}

	// Day 5 in both, then a fuel hike forced in one of them.
	Time->AdvanceMinutes(5 * UTimeSubsystem::MinutesPerDay);
	TimeRef->AdvanceMinutes(5 * UTimeSubsystem::MinutesPerDay);
	const int32 Day = Time->GetDay();
	const float RouteBefore = Events->GetRouteCostMultiplier(Day);
	const float PriceBefore = Market->GetPrice(TEXT("G01"));
	TestEqual(TEXT("Same seed, same price before anything is forced"), PriceBefore, MarketRef->GetPrice(TEXT("G01")));

	TestFalse(TEXT("Unknown IDs are refused"), Events->ForceEvent(TEXT("E99")));
	TestTrue(TEXT("E16 forced"), Events->ForceEvent(TEXT("E16"), 0.3f, 3));

	TestEqual(TEXT("Routes x1.25 on top of whatever was running"),
		Events->GetRouteCostMultiplier(Day), RouteBefore * 1.25f, 1e-4f);
	TestTrue(TEXT("G01 (FuelSpike) is pushed"), Events->GetPriceTarget(TEXT("G01"), Day) >= 0.3f - 1e-4f);
	TestTrue(TEXT("Forced event is listed as active"),
		Events->GetActiveEventsToday().ContainsByPredicate([](const FActiveEvent& E) { return E.EventID == TEXT("E16") && E.bForced; }));

	// Two days in, the forced run's G01 sits above the untouched run's.
	Time->AdvanceMinutes(2 * UTimeSubsystem::MinutesPerDay);
	TimeRef->AdvanceMinutes(2 * UTimeSubsystem::MinutesPerDay);
	const float With = Market->GetPrice(TEXT("G01"));
	const float Without = MarketRef->GetPrice(TEXT("G01"));
	TestTrue(*FString::Printf(TEXT("G01 higher with the fuel hike ($%.2f vs $%.2f)"), With, Without), With > Without * 1.05f);

	// A product the event does not touch is unaffected.
	TestEqual(TEXT("S01 untouched"), Market->GetPrice(TEXT("S01")), MarketRef->GetPrice(TEXT("S01")));

	// A new game drops the forced event.
	WithGI->GetSubsystem<USaveSubsystem>()->StartNewGame(Seed);
	TestFalse(TEXT("New game forgets forced events"),
		Events->GetActiveEventsToday().ContainsByPredicate([](const FActiveEvent& E) { return E.bForced; }));

	WithGI->Shutdown();
	WithoutGI->Shutdown();
	return true;
}

#endif
