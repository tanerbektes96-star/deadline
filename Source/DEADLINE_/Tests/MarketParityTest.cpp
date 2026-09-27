// Copyright DEADLINE. All Rights Reserved.
//
// Proves the C++ market model produces the same prices as the Python
// reference in EconomyPrototype. This is the mechanical half of the Month 2
// gate test ("Excel prototipiyle sonuclar uyumlu").
//
// Run headless (about 20 seconds, mostly editor startup):
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "D:\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.Economy;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log
//
// Two tests, deliberately separate:
//
//   Deadline.Economy.RandomStreamPort  checks the generator itself against
//       hardcoded numbers from EconomyPrototype/rng.py. If an engine upgrade
//       ever changes FRandomStream, this fails first and says so plainly,
//       instead of leaving MarketParity failing for reasons nobody can read.
//
//   Deadline.Economy.MarketParity      replays Tests/MarketParityGolden.csv.

#include "CoreMinimal.h"
#include "Economy/MarketModel.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Relative tolerance between the two implementations.
	    The only expected source of difference is BasePrice: Python parses the
	    CSV decimal straight into a double, while FProductRow stores a float
	    and the model widens it back. That is a ~1e-7 relative difference on
	    day 0, and mean reversion contracts it rather than growing it, so
	    anything above 1e-5 means the models really have diverged. */
	constexpr double ParityTolerance = 1e-5;

	ERiskBand ParseBand(const FString& Text, bool& bOutOk)
	{
		bOutOk = true;
		if (Text == TEXT("Stable"))   { return ERiskBand::Stable; }
		if (Text == TEXT("Reactive")) { return ERiskBand::Reactive; }
		if (Text == TEXT("Volatile")) { return ERiskBand::Volatile; }
		if (Text == TEXT("Crisis"))   { return ERiskBand::Crisis; }
		bOutOk = false;
		return ERiskBand::Stable;
	}

	FString GoldenFilePath()
	{
		return FPaths::Combine(FPaths::ProjectDir(), TEXT("Tests"), TEXT("MarketParityGolden.csv"));
	}
}

// --- The generator ----------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineRandomStreamPortTest,
	"Deadline.Economy.RandomStreamPort",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineRandomStreamPortTest::RunTest(const FString& Parameters)
{
	// Straight from EconomyPrototype/rng.py, `python rng.py`.
	const double Expected[] = {
		0.476009488, 0.661768794, 0.577346206,
		0.958549142, 0.026449680, 0.593309999
	};

	FRandomStream Stream(12345);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
	{
		const double Actual = static_cast<double>(Stream.GetFraction());
		if (FMath::Abs(Actual - Expected[Index]) > 1e-7)
		{
			AddError(FString::Printf(
				TEXT("FRandomStream(12345) draw %d: engine gave %.9f, rng.py expects %.9f. ")
				TEXT("The Python port of FRandomStream is out of date -- check ")
				TEXT("Engine/Source/Runtime/Core/Public/Math/RandomStream.h against ")
				TEXT("EconomyPrototype/rng.py (see HATA_GUNLUGU H011)."),
				Index, Actual, Expected[Index]));
			return false;
		}
	}

	// Seed derivation has to agree too, or every product draws a different
	// sequence even though the generator itself is fine.
	TestEqual(TEXT("FNV1a32('MarketStream')"),
		static_cast<int64>(DeadlineSeed::FNV1a32(TEXT("MarketStream"))), static_cast<int64>(254816041));
	TestEqual(TEXT("Derive(42, 'MarketStream:S01')"),
		DeadlineSeed::Derive(42, TEXT("MarketStream:S01")), 1500719297);
	TestEqual(TEXT("Derive(0, 'MarketStream:S01')"),
		DeadlineSeed::Derive(0, TEXT("MarketStream:S01")), 1869826915);

	// And the two together, which is what MakeProductStream does.
	FRandomStream Product = FMarketModel::MakeProductStream(42, FName(TEXT("S01")));
	const double FirstDraws[] = { 0.898061156, 0.313171029, 0.717588425 };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FirstDraws); ++Index)
	{
		TestNearlyEqual(*FString::Printf(TEXT("S01 seed 42, draw %d"), Index),
			static_cast<double>(Product.GetFraction()), FirstDraws[Index], 1e-7);
	}

	return true;
}

// --- The model --------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineMarketParityTest,
	"Deadline.Economy.MarketParity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineMarketParityTest::RunTest(const FString& Parameters)
{
	const FString Path = GoldenFilePath();

	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *Path))
	{
		AddError(FString::Printf(
			TEXT("Could not read '%s'. Regenerate it with: ")
			TEXT("cd EconomyPrototype && python dump_parity.py"), *Path));
		return false;
	}

	/** One (seed, product) series the golden file has rows for. */
	struct FSeries
	{
		int32 Seed = 0;
		FName ProductID;
		ERiskBand Band = ERiskBand::Stable;
		/** Day 0 of the golden file, which is the product's BasePrice. */
		double BasePrice = -1.0;
		int32 MaxDay = 0;
		TArray<double> Prices;
	};
	struct FExpectation
	{
		FString Key;
		int32 Day = 0;
		double Price = 0.0;
	};

	TMap<FString, FSeries> Series;
	TArray<FExpectation> Expectations;

	// Pass 1: read the file. Simulating as we go would re-run each series once
	// per row, so gather first and simulate once per (seed, product).
	for (const FString& Raw : Lines)
	{
		const FString Line = Raw.TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT("Seed,")))
		{
			continue;
		}

		TArray<FString> Cells;
		Line.ParseIntoArray(Cells, TEXT(","), /*InCullEmpty=*/false);
		if (Cells.Num() != 5)
		{
			AddError(FString::Printf(TEXT("Malformed golden row: '%s'"), *Line));
			return false;
		}

		bool bBandOk = false;
		const ERiskBand Band = ParseBand(Cells[2], bBandOk);
		if (!bBandOk)
		{
			AddError(FString::Printf(TEXT("Unknown risk band '%s' in row '%s'"), *Cells[2], *Line));
			return false;
		}

		const int32 Seed = FCString::Atoi(*Cells[0]);
		const int32 Day = FCString::Atoi(*Cells[3]);
		const double Price = FCString::Atod(*Cells[4]);
		const FString Key = FString::Printf(TEXT("%d|%s"), Seed, *Cells[1]);

		FSeries& Entry = Series.FindOrAdd(Key);
		Entry.Seed = Seed;
		Entry.ProductID = FName(*Cells[1]);
		Entry.Band = Band;
		Entry.MaxDay = FMath::Max(Entry.MaxDay, Day);
		if (Day == 0)
		{
			Entry.BasePrice = Price;
		}

		Expectations.Add(FExpectation{ Key, Day, Price });
	}

	if (Expectations.Num() == 0)
	{
		AddError(TEXT("Golden file has no data rows."));
		return false;
	}

	// Pass 2: run the C++ model once per series.
	for (TPair<FString, FSeries>& Pair : Series)
	{
		FSeries& Entry = Pair.Value;
		if (Entry.BasePrice < 0.0)
		{
			AddError(FString::Printf(
				TEXT("Golden file has no day 0 row for %s, so there is no base price to start from."),
				*Pair.Key));
			return false;
		}
		Entry.Prices = FMarketModel::SimulateSeries(
			Entry.Seed, Entry.ProductID, Entry.BasePrice, Entry.Band, Entry.MaxDay);
	}

	// Pass 3: compare. Report the single worst row rather than thousands of
	// near-identical failures.
	double WorstRelative = 0.0;
	FString WorstDetail;
	int32 Mismatches = 0;

	for (const FExpectation& Expected : Expectations)
	{
		const FSeries& Entry = Series[Expected.Key];
		if (!Entry.Prices.IsValidIndex(Expected.Day))
		{
			AddError(FString::Printf(TEXT("%s: no simulated price for day %d."),
				*Expected.Key, Expected.Day));
			return false;
		}

		const double Actual = Entry.Prices[Expected.Day];
		const double Divisor = FMath::Max(FMath::Abs(Expected.Price), UE_DOUBLE_SMALL_NUMBER);
		const double Relative = FMath::Abs(Actual - Expected.Price) / Divisor;

		if (Relative > WorstRelative)
		{
			WorstRelative = Relative;
			WorstDetail = FString::Printf(TEXT("%s day %d: C++ %.9g, Python %.9g (relative %.3g)"),
				*Expected.Key, Expected.Day, Actual, Expected.Price, Relative);
		}
		if (Relative > ParityTolerance)
		{
			++Mismatches;
		}
	}

	if (Mismatches > 0)
	{
		AddError(FString::Printf(
			TEXT("%d of %d prices differ from EconomyPrototype by more than %.0e. Worst: %s. ")
			TEXT("The C++ model and EconomyPrototype/market.py have drifted apart -- ")
			TEXT("fix one, then regenerate the golden file with dump_parity.py."),
			Mismatches, Expectations.Num(), ParityTolerance, *WorstDetail));
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("%d prices across %d seeded product series match EconomyPrototype. Worst deviation: %s"),
		Expectations.Num(), Series.Num(),
		WorstDetail.IsEmpty() ? TEXT("none") : *WorstDetail));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
