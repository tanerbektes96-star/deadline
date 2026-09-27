// Copyright DEADLINE. All Rights Reserved.
//
// Number and colour rules shared by the forecast card, the forecast screen,
// the result card and the notebook. In a header for the same reason as
// DeadlineUIPalette.h: helpers defined per .cpp collide once two of them
// share a unity build.

#pragma once

#include "CoreMinimal.h"
#include "UI/DeadlineUIPalette.h"

namespace ForecastFormat
{
	/** "$1,240" -- thousands grouped, no cents: stakes, not prices. */
	inline FText Money(float Value)
	{
		FNumberFormattingOptions Options;
		Options.SetMaximumFractionalDigits(0);
		Options.SetUseGrouping(true);
		return FText::Format(NSLOCTEXT("Deadline", "ForecastMoney", "${0}"), FText::AsNumber(FMath::RoundToInt(Value), &Options));
	}

	/** "+$1,240" / "−$380": the sign outside the currency, as people say it. */
	inline FText SignedMoney(float Value)
	{
		return FText::Format(NSLOCTEXT("Deadline", "SignedMoney", "{0}{1}"),
			FText::FromString(Value < 0.f ? TEXT("−") : TEXT("+")), Money(FMath::Abs(Value)));
	}

	/** 0.75 -> "75", for texts that print the % sign themselves. */
	inline FText Percent(float Fraction)
	{
		return FText::AsNumber(FMath::RoundToInt(Fraction * 100.f));
	}

	/** Calm for a near-certainty, amber for a coin toss, red for a long shot.
	    Never the only signal: the percentage is always printed beside it. */
	inline FLinearColor ConfidenceColour(float Confidence)
	{
		return Confidence >= 0.8f ? DeadlineUI::Profit
			: Confidence >= 0.65f ? DeadlineUI::Warn
			: DeadlineUI::Loss;
	}
}
