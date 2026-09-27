// Copyright DEADLINE. All Rights Reserved.
//
// TR or EN, one rule for the whole game: Turkish when the game runs in
// Turkish and the Turkish text exists, English otherwise. The data tables
// carry both columns (NameTR/NameEN and friends); this picks between them.
//
// In a header, not an anonymous namespace per .cpp, for the same reason as
// DeadlineUIPalette.h: two files defining the same helper collide the moment
// they land in one unity build.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace DeadlineLocale
{
	inline bool IsTurkish()
	{
		return FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() == TEXT("tr");
	}

	inline const FString& Pick(const FString& TR, const FString& EN)
	{
		return (IsTurkish() && !TR.IsEmpty()) ? TR : EN;
	}

	/** For fixed words built into code: both languages side by side at the call. */
	inline const TCHAR* Pick(const TCHAR* TR, const TCHAR* EN)
	{
		return IsTurkish() ? TR : EN;
	}
}

/**
 * FString::Printf in the game's language. The choice is made around the call,
 * not inside the format argument, because UE 5.8 checks format strings at
 * compile time and a format picked at run time does not compile.
 *
 *     DL_PRINTF("%s sona erdi.", "%s is over.", *Name)
 */
#define DL_PRINTF(TRFormat, ENFormat, ...) \
	(DeadlineLocale::IsTurkish() \
		? FString::Printf(TEXT(TRFormat), ##__VA_ARGS__) \
		: FString::Printf(TEXT(ENFormat), ##__VA_ARGS__))
