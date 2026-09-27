// Copyright DEADLINE. All Rights Reserved.
//
// The news layer (GDD 4 step 1, GDD 16, roadmap Month 4): the first place the
// player hears about what the event calendar is doing.
//
//   08:00  Morning bulletin -- events starting today (headlines), rumours
//          that came to nothing, events that ended, and today's new rumours
//          with the confidence of their source.
//   12:00  Midday update -- for every event running, how the prices it
//          pushes have moved since it began.
//
// A bulletin is built only from what the player is allowed to know: the
// truth-free views UEventSubsystem gives out (FEventSignal, FActiveEvent) and
// the market's own prices. A rumour reads the same whether or not it will
// come true; the answer arrives in the bulletin of the day it was due.
//
// Like the calendar it is a pure function of seed and time, so nothing here
// is saved: after a load the log is rebuilt from the days already played.
//
// Presentation is not here. The greybox HUD shows each bulletin as it lands;
// the news screen and the forecast board (Month 4, next) read GetNewsLog().
// OnBulletinPublished is also the hook for a radio voice-over later.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NewsSubsystem.generated.h"

class UEventSubsystem;
class UMarketSubsystem;
class UProductCatalogSubsystem;
class UTimeSubsystem;

UENUM(BlueprintType)
enum class ENewsKind : uint8
{
	/** A signal: something may happen. Carries a confidence. */
	Rumour,
	/** An event has started. */
	Headline,
	/** A rumour's day came and nothing happened. */
	Denied,
	/** An event is over. */
	Ended,
	/** Midday: how an ongoing event has moved its prices. */
	Update
};

USTRUCT(BlueprintType)
struct FNewsItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "News")
	ENewsKind Kind = ENewsKind::Rumour;

	UPROPERTY(BlueprintReadOnly, Category = "News")
	FName EventID;

	/** Event name, in the game's language. */
	UPROPERTY(BlueprintReadOnly, Category = "News")
	FString Title;

	/** The sentence to show. */
	UPROPERTY(BlueprintReadOnly, Category = "News")
	FString Text;

	/** Rumours only: the source's confidence, 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "News")
	float Confidence = 0.f;

	/** Rumours: the day it is expected. Headlines: the last day it runs. */
	UPROPERTY(BlueprintReadOnly, Category = "News")
	int32 RelatedDay = 0;

	/** Products concerned. */
	UPROPERTY(BlueprintReadOnly, Category = "News")
	TArray<FName> Products;
};

USTRUCT(BlueprintType)
struct FNewsBulletin
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "News")
	int32 Day = 0;

	UPROPERTY(BlueprintReadOnly, Category = "News")
	int32 Hour = 0;

	UPROPERTY(BlueprintReadOnly, Category = "News")
	TArray<FNewsItem> Items;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBulletinPublished, const FNewsBulletin&, Bulletin);

UCLASS()
class DEADLINE__API UNewsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** GDD 16: "08:00 Haber bülteni", "12:00 Öğle haber güncellemesi". */
	static constexpr int32 MorningHour = 8;
	static constexpr int32 MiddayHour = 12;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Fired when a bulletin with at least one item goes out. */
	UPROPERTY(BlueprintAssignable, Category = "Deadline|News")
	FOnBulletinPublished OnBulletinPublished;

	/** Bulletins already out, newest first, from the last MaxDays days. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|News")
	TArray<FNewsBulletin> GetNewsLog(int32 MaxDays = 7) const;

	/** The most recent bulletin that had anything in it. Day -1 if none. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|News")
	FNewsBulletin GetLatestBulletin() const;

	/** Build one bulletin. Hour must be MorningHour or MiddayHour; anything
	    else is empty. Does not check that the time has come -- the log and
	    the delegate do that -- so tests can inspect any day. */
	FNewsBulletin BuildBulletin(int32 Day, int32 Hour) const;

private:
	void BuildMorning(int32 Day, TArray<FNewsItem>& Out) const;
	void BuildMidday(int32 Day, TArray<FNewsItem>& Out) const;

	UFUNCTION()
	void HandleHourChanged(int32 Day, int32 Hour);

	/** Has this bulletin's time come? */
	bool IsPublished(int32 Day, int32 Hour) const;

	UEventSubsystem* GetEvents() const;
	UMarketSubsystem* GetMarket() const;
	UProductCatalogSubsystem* GetCatalogue() const;
	UTimeSubsystem* GetTime() const;
};
