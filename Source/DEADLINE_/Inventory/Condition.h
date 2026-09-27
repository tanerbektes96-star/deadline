// Copyright DEADLINE. All Rights Reserved.
//
// Spoilage and obsolescence, as arithmetic. No UObject, no world, no clock —
// the same arrangement as MarketModel.h, so the rules can be tested without
// opening the game.
//
// These are two different things and the GDD says so twice (5.1, and the quick
// reference in CLAUDE.md):
//
//   ShelfLifeDays        goods ROT. Past this age they are gone: written off
//                        the ledger, worth nothing. 0 = never spoils.
//   ObsolescencePerDay   goods LOSE VALUE while staying perfectly usable. A
//                        graphics card six weeks old still works; nobody will
//                        pay last month's price for it.
//
// A product can have both (E01 is stocked food-grade and dated), one, or
// neither. Most of the catalogue has neither and pays nothing for this system.
//
// Reading ObsolescencePerDay: GDD 5.1 calls it "günlük değer kaybı yüzdesi" —
// a PERCENTAGE per day. DT_Products spreads 0.02 to 0.60, so the fastest-aging
// item on the list (E05) sheds 0.6% a day: about 17% a month, half its value in
// four months. Read as a raw fraction instead, 0.60 would mean 60% a day and a
// graphics card would be worthless inside a week — clearly not the intent.

#pragma once

#include "CoreMinimal.h"

namespace DeadlineCondition
{
	/** Minutes in a game day. Ages are computed from the clock's total minutes
	    so that a save/load, a travel jump and a speed change all agree. */
	static constexpr double MinutesPerDay = 1440.0;

	inline double DaysBetween(double FromMinute, double ToMinute)
	{
		return (ToMinute - FromMinute) / MinutesPerDay;
	}

	/**
	 * Fraction of its value goods keep after ageing for AgeDays.
	 *
	 * Compounding, not linear: a 0.6%/day item loses 0.6% of what it is worth
	 * today, not 0.6% of what it was worth new. Linear decay would cross zero
	 * on a fixed date, which is spoilage, and this is not spoilage.
	 *
	 * @param PercentPerDay  FProductRow::ObsolescencePerDay, in percent.
	 * @param Floor          the value goods never fall below, as a fraction.
	 *                       Old stock is cheap, never free: there is always
	 *                       somebody who will take a two-year-old monitor.
	 */
	inline float ObsolescenceMultiplier(float PercentPerDay, double AgeDays, float Floor)
	{
		if (PercentPerDay <= 0.f || AgeDays <= 0.0)
		{
			return 1.f;
		}
		const double Kept = FMath::Pow(1.0 - PercentPerDay / 100.0, AgeDays);
		return FMath::Clamp(static_cast<float>(Kept), FMath::Clamp(Floor, 0.f, 1.f), 1.f);
	}

	/**
	 * Has a batch acquired at AcquiredAtMinute passed its shelf life by NowMinute?
	 * @param ShelfLifeDays  FProductRow::ShelfLifeDays. 0 means never.
	 */
	inline bool HasSpoiled(int32 ShelfLifeDays, double AcquiredAtMinute, double NowMinute)
	{
		return ShelfLifeDays > 0
			&& DaysBetween(AcquiredAtMinute, NowMinute) >= static_cast<double>(ShelfLifeDays);
	}

	/**
	 * Day rollovers the oldest goods on hand will survive.
	 * Negative return = the product does not spoil at all, which is not the
	 * same answer as "lots of time left" and the UI shows it differently.
	 *
	 * Counted in DAY NUMBERS, not in elapsed time, and that is deliberate:
	 * goods bought at 09:40 with six days on them should read "6" all through
	 * the day they were bought, not "5" from 09:41 onwards. The sweep runs on
	 * the day rollover, so the day number is also what actually decides when
	 * they go.
	 */
	inline int32 DaysUntilSpoilage(int32 ShelfLifeDays, double OldestAcquiredMinute, double NowMinute)
	{
		if (ShelfLifeDays <= 0)
		{
			return -1;
		}
		const double SpoilsAtMinute = OldestAcquiredMinute + ShelfLifeDays * MinutesPerDay;
		const int32 SpoilsOnDay = FMath::FloorToInt(SpoilsAtMinute / MinutesPerDay);
		const int32 Today = FMath::FloorToInt(NowMinute / MinutesPerDay);
		return FMath::Max(0, SpoilsOnDay - Today);
	}
}
