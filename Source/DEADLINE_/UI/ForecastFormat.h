// Copyright DEADLINE. All Rights Reserved.
//
// Number and colour rules shared by the forecast card and the forecast
// screen. In a header for the same reason as DeadlineUIPalette.h: helpers
// defined per .cpp collide once two of them share a unity build.

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

	/** Calm for a near-certainty, amber for a coin toss, red for a long shot.
	    Never the only signal: the percentage is always printed beside it. */
	inline FLinearColor ConfidenceColour(float Confidence)
	{
		return Confidence >= 0.8f ? DeadlineUI::Profit
			: Confidence >= 0.65f ? DeadlineUI::Warn
			: DeadlineUI::Loss;
	}
}
