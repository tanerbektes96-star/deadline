// Copyright DEADLINE. All Rights Reserved.

#include "Core/TimeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

float UTimeSubsystem::SpeedMultiplier(EGameSpeed Speed)
{
	switch (Speed)
	{
	case EGameSpeed::Paused:  return 0.f;
	case EGameSpeed::Normal:  return 1.f;
	case EGameSpeed::Fast:    return 2.f;
	case EGameSpeed::Fastest: return 4.f;
	}
	return 1.f;
}

void UTimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LastRealSeconds = FPlatformTime::Seconds();
	LastBroadcastDay = 0;

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				DayPollTimer, this, &UTimeSubsystem::PollDayRollover, 1.0f, /*bLoop=*/true);
		}
	}
}

void UTimeSubsystem::Deinitialize()
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().ClearTimer(DayPollTimer);
		}
	}
	Super::Deinitialize();
}

double UTimeSubsystem::GetTotalMinutes() const
{
	// At 1x, one real second is one game minute (GDD 16).
	const double Elapsed = FPlatformTime::Seconds() - LastRealSeconds;
	return BankedMinutes + Elapsed * SpeedMultiplier(GameSpeed);
}

int32 UTimeSubsystem::GetDay() const
{
	return FMath::FloorToInt(GetTotalMinutes() / static_cast<double>(MinutesPerDay));
}

double UTimeSubsystem::GetMinuteOfDay() const
{
	return FMath::Fmod(GetTotalMinutes(), static_cast<double>(MinutesPerDay));
}

FString UTimeSubsystem::GetClockString() const
{
	return FString::Printf(TEXT("Day %d - %02d:%02d"), GetDay(), GetHour(), GetMinute());
}

bool UTimeSubsystem::IsNight() const
{
	const int32 Hour = GetHour();
	return Hour >= 22 || Hour < 5;
}

void UTimeSubsystem::Flush()
{
	const double Now = FPlatformTime::Seconds();
	BankedMinutes += (Now - LastRealSeconds) * SpeedMultiplier(GameSpeed);
	LastRealSeconds = Now;
}

void UTimeSubsystem::SetGameSpeed(EGameSpeed NewSpeed)
{
	if (NewSpeed == GameSpeed)
	{
		return;
	}
	Flush();                 // bank the old segment before the rate changes
	GameSpeed = NewSpeed;
	OnGameSpeedChanged.Broadcast(GameSpeed);
}

void UTimeSubsystem::AdvanceMinutes(int32 Minutes)
{
	if (Minutes == 0)
	{
		return;
	}
	Flush();
	BankedMinutes += Minutes;
	PollDayRollover();
}

void UTimeSubsystem::PollDayRollover()
{
	const int32 Day = GetDay();
	if (Day != LastBroadcastDay)
	{
		LastBroadcastDay = Day;
		OnDayChanged.Broadcast(Day);
	}
}

void UTimeSubsystem::ResetAll()
{
	BankedMinutes = 0.0;
	LastRealSeconds = FPlatformTime::Seconds();
	LastBroadcastDay = 0;
	GameSpeed = EGameSpeed::Normal;
}

void UTimeSubsystem::RestoreTotalMinutes(double InTotalMinutes)
{
	BankedMinutes = InTotalMinutes;
	LastRealSeconds = FPlatformTime::Seconds();
	LastBroadcastDay = GetDay();
}
