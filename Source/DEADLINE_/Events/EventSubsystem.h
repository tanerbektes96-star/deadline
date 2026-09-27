// Copyright DEADLINE. All Rights Reserved.
//
// World events (GDD 13) and the signals that come before them (GDD 4, step 1).
//
// The calendar itself is FEventCalendar: rolled from the run seed, never
// saved, rolled lazily up to whatever day someone asks about. This subsystem
// is the game-facing side of it:
//   - UMarketSubsystem asks GetPriceTarget() for each product and day while it
//     catches a price up, and the event pushes that price (MarketModel.h).
//   - UTravelSubsystem asks GetRouteCostMultiplier() when it quotes a trip.
//   - The news layer, the forecast board and the day summary (Month 4, next)
//     read GetSignals() / GetActiveEvents() and listen to the delegates.
//
// What the player may see is kept apart from the truth on purpose.
// FEventSignal carries the confidence but not the answer; only FScheduledEvent
// (dev cheats and tests) knows whether a signal will come true.
//
// No Tick (CLAUDE.md tick rules): the delegates fire from the clock's
// OnDayChanged, and everything else is computed when asked.

#pragma once

#include "CoreMinimal.h"
#include "Events/EventCalendar.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EventSubsystem.generated.h"

class UProductCatalogSubsystem;
class UTimeSubsystem;

/** A signal as the player sees it: what might happen, how sure the source
    is, and when. Never whether it will. */
USTRUCT(BlueprintType)
struct FEventSignal
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FName EventID;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FString Name;

	/** The rumour or forecast text. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FString Text;

	/** 0..1. Honest: signals shown at 0.7 come true 70% of the time. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	float Confidence = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	int32 SignalDay = 0;

	/** When it would start. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	int32 ExpectedDay = 0;

	/** Products it would push, from the tag match. Empty for route events. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	TArray<FName> AffectedProducts;

	/** > 1 when it would make trips dearer. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	float RouteCostMultiplier = 1.f;
};

/** An event that is happening. */
USTRUCT(BlueprintType)
struct FActiveEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FName EventID;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	FString Headline;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	int32 StartDay = 0;

	/** First day it is over. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	int32 EndDay = 0;

	/** Price push, fraction of base price. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	float Impact = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	float RouteCostMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Event")
	TArray<FName> AffectedProducts;

	/** Came in through a cheat, with no signal before it. */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	bool bForced = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEventSignalled, const FEventSignal&, Signal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEventStarted, const FActiveEvent&, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEventEnded, FName, EventID);
/** A signal reached its day: it either started or was found to be false.
    The hook for the forecast result card (GDD 4, step 6). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSignalResolved, const FEventSignal&, Signal, bool, bCameTrue);
/** The calendar changed under the market (a forced event): cached prices from
    the start day on are stale. */
DECLARE_MULTICAST_DELEGATE(FOnEventCalendarChanged);

UCLASS()
class DEADLINE__API UEventSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Events")
	FOnEventSignalled OnEventSignalled;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Events")
	FOnEventStarted OnEventStarted;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Events")
	FOnEventEnded OnEventEnded;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Events")
	FOnSignalResolved OnSignalResolved;

	FOnEventCalendarChanged OnCalendarChanged;

	// --- What the player can know ----------------------------------------

	/** Signals out on Day that have not resolved yet. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Events")
	TArray<FEventSignal> GetSignals(int32 Day) const;

	/** Events in effect on Day. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Events")
	TArray<FActiveEvent> GetActiveEvents(int32 Day) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Events")
	TArray<FEventSignal> GetSignalsToday() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Events")
	TArray<FActiveEvent> GetActiveEventsToday() const;

	// --- What the simulation reads --------------------------------------

	/** Summed targets of the events pushing ProductID on Day, as a fraction
	    of base price, capped at MaxEventTarget. 0 = no event. */
	float GetPriceTarget(FName ProductID, int32 Day) const;

	/** Product of the route multipliers in effect on Day. 1 = ordinary. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Events")
	float GetRouteCostMultiplier(int32 Day) const;

	/** Products whose tags match an event, for signals and headlines. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Events")
	TArray<FName> GetAffectedProducts(FName EventID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Events")
	FString GetEventName(FName EventID) const;

	/** The price push an event of this kind can have, from DT_Events: what
	    analysts would quote. Not the value the calendar rolled. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Deadline|Events")
	bool GetImpactRange(FName EventID, float& OutMin, float& OutMax) const;

	// --- Dev and tests ----------------------------------------------------

	/** Every entry with SignalDay in [FromDay, ToDay], truth included, plus
	    forced events in that range. Cheats and tests only: never show this to
	    the player, it gives the forecast away. */
	TArray<FScheduledEvent> GetCalendar(int32 FromDay, int32 ToDay) const;

	/** Start an event today with no signal before it. Impact < 0 and
	    Duration < 1 roll from the table like a real one would. Not saved:
	    a load or a new game drops it. Returns false for an unknown ID. */
	bool ForceEvent(FName EventID, float Impact = -1.f, int32 Duration = -1);

	/** New game or load: forget forced events; the calendar re-rolls from the
	    new seed on next use. */
	void ResetAll();

private:
	/** Brings the calendar to the current seed and table, rolled through Day. */
	const FEventCalendar& GetCalendarThrough(int32 Day) const;

	/** Rolled entries and forced ones, together. */
	void GatherEntries(int32 Day, TArray<const FScheduledEvent*>& Out) const;

	FEventSignal MakeSignal(const FScheduledEvent& Entry) const;
	FActiveEvent MakeActive(const FScheduledEvent& Entry) const;

	/** Tags of a product, cached; the catalogue splits them from a string. */
	const TArray<FName>& GetProductTags(FName ProductID) const;
	bool TemplateTouchesProduct(const FEventTemplate& Template, FName ProductID) const;

	UFUNCTION()
	void HandleDayChanged(int32 NewDay);

	UFUNCTION()
	void HandleCatalogueReloaded();

	void Invalidate();

	UProductCatalogSubsystem* GetCatalogue() const;
	UTimeSubsystem* GetTime() const;
	int32 GetRunSeed() const;
	int32 GetToday() const;

	/** Lazily rolled, like the market: const queries fill it in. */
	mutable FEventCalendar Calendar;
	mutable bool bCalendarReady = false;
	mutable int32 CalendarSeed = 0;

	mutable TMap<FName, TArray<FName>> ProductTags;

	TArray<FScheduledEvent> ForcedEvents;

	/** Last day the delegates were fired for, so a jump of several days
	    reports each day once. */
	int32 LastAnnouncedDay = 0;
};
