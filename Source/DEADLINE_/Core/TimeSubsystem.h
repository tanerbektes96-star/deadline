// Copyright DEADLINE. All Rights Reserved.
//
// The game clock. GDD 16: one game day is about 24 real minutes at 1x, so at
// 1x one real second equals one game minute.
//
// No Tick (CLAUDE.md). The clock is lazily evaluated: we store the minutes
// banked so far plus the real timestamp of the last speed change, and compute
// the current time only when something asks. A single 1 Hz timer exists purely
// to notice a day rollover so listeners can be told; it does no arithmetic
// anyone depends on.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimeSubsystem.generated.h"

/** GDD 16 speed settings. */
UENUM(BlueprintType)
enum class EGameSpeed : uint8
{
	Paused,
	Normal,   // 1x
	Fast,     // 2x
	Fastest   // 4x
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayChanged, int32, NewDay);
/** A whole game hour has been reached (e.g. Day 3, Hour 8 = 08:00). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHourChanged, int32, Day, int32, Hour);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameSpeedChanged, EGameSpeed, NewSpeed);

UCLASS()
class DEADLINE__API UTimeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MinutesPerDay = 1440;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Time")
	FOnDayChanged OnDayChanged;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Time")
	FOnGameSpeedChanged OnGameSpeedChanged;

	/** Fired once for every game hour reached, in order, including hours
	    jumped over by a trip or a time skip -- so the 08:00 bulletin
	    (GDD 16) is never lost to a long drive. A skip of more than two days
	    reports only the last 48 hours. Same 1 Hz poll as the day. */
	UPROPERTY(BlueprintAssignable, Category = "Deadline|Time")
	FOnHourChanged OnHourChanged;

	/** Total game minutes since the run started. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Time")
	double GetTotalMinutes() const;

	/** Day number, starting at 0. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Time")
	int32 GetDay() const;

	/** Minutes elapsed within the current day, 0..1439. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Time")
	double GetMinuteOfDay() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Time")
	int32 GetHour() const { return FMath::FloorToInt(GetMinuteOfDay() / 60.0); }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Time")
	int32 GetMinute() const { return FMath::FloorToInt(FMath::Fmod(GetMinuteOfDay(), 60.0)); }

	/** "Day 3 - 14:25", for the HUD. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Time")
	FString GetClockString() const;

	/** True between 22:00 and 05:00 — the grey window (GDD 16). Nothing uses
	    this until Month 7; it lives here because it is a property of the clock. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Time")
	bool IsNight() const;

	UFUNCTION(BlueprintCallable, Category = "Deadline|Time")
	void SetGameSpeed(EGameSpeed NewSpeed);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Time")
	EGameSpeed GetGameSpeed() const { return GameSpeed; }

	/** Dev cheat / event hook: jump the clock forward. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Time")
	void AdvanceMinutes(int32 Minutes);

	void ResetAll();
	void RestoreTotalMinutes(double InTotalMinutes);

	static float SpeedMultiplier(EGameSpeed Speed);

private:
	/** Fold elapsed real time into BankedMinutes and restamp. */
	void Flush();

	/** 1 Hz: only checks whether the day or the hour rolled over. */
	void PollDayRollover();

	/** Whole game hours since the run started. */
	int64 GetHourIndex() const;

	UPROPERTY()
	EGameSpeed GameSpeed = EGameSpeed::Normal;

	/** Minutes accumulated before the current speed segment began. */
	double BankedMinutes = 0.0;

	/** Real (unpaused) seconds at the last flush. */
	double LastRealSeconds = 0.0;

	int32 LastBroadcastDay = 0;

	int64 LastBroadcastHour = 0;

	FTimerHandle DayPollTimer;
};
