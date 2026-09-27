// Copyright DEADLINE. All Rights Reserved.
//
// The interface palette from Assets/DEADLINE_UI_Prompts.md 1.2, in one place.
//
// Every widget that colours anything reads it from here. Two reasons: the
// palette is binding design documentation, not a per-file preference, and
// duplicating the constants per .cpp collides the moment two of those files
// land in the same unity build.

#pragma once

#include "CoreMinimal.h"

namespace DeadlineUI
{
	/** #16191A - near-black charcoal, the page ground. */
	inline const FLinearColor Ground(FColor(0x16, 0x19, 0x1A));

	/** #1E2325 - cards and panels. */
	inline const FLinearColor Panel(FColor(0x1E, 0x23, 0x25));

	/** #272D30 - raised panel: active selection, top layer. */
	inline const FLinearColor PanelRaised(FColor(0x27, 0x2D, 0x30));

	/** #333B3E - dividers and borders. */
	inline const FLinearColor Line(FColor(0x33, 0x3B, 0x3E));

	/** #E8E4DC - warm off-white body text. */
	inline const FLinearColor Body(FColor(0xE8, 0xE4, 0xDC));

	/** #8B938F - labels and secondary text. */
	inline const FLinearColor Muted(FColor(0x8B, 0x93, 0x8F));

	/** #4E9A85 - calm teal-green, success and profit. */
	inline const FLinearColor Profit(FColor(0x4E, 0x9A, 0x85));

	/** #D4A63C - amber warning. */
	inline const FLinearColor Warn(FColor(0xD4, 0xA6, 0x3C));

	/** #B4503A - restrained brick red, danger and loss. */
	inline const FLinearColor Loss(FColor(0xB4, 0x50, 0x3A));

	/** #7C5E9B - heat and grey market. Compliance screens only (Month 7). */
	inline const FLinearColor Heat(FColor(0x7C, 0x5E, 0x9B));

	/** #5B8CA8 - cool blue, interactive elements. */
	inline const FLinearColor Accent(FColor(0x5B, 0x8C, 0xA8));
}
