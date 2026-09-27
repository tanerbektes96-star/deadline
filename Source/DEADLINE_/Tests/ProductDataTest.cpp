// Copyright DEADLINE. All Rights Reserved.
//
// Checks the product catalogue's data rather than its code.
//
// Two things go wrong with CSV-driven balance data, and both are silent:
// a row stops parsing (so a product quietly vanishes from the game), or the
// CSV is edited and DT_Products is never re-imported (so the running game
// disagrees with the file the designer is looking at). This test catches both,
// and it is also the test coverage for the Dl_ReloadProducts path, since that
// is the same CreateTableFromCSVString call.
//
//   "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" \
//     "D:\DEADLINE_\DEADLINE_.uproject" \
//     -ExecCmds="Automation RunTests Deadline.Data;Quit" \
//     -unattended -nopause -nosplash -nullrhi -log

#include "CoreMinimal.h"
#include "Core/DeadlineSettings.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Data/ProductRow.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeadlineProductCsvTest,
	"Deadline.Data.ProductCsv",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDeadlineProductCsvTest::RunTest(const FString& Parameters)
{
	const FString CsvPath = UProductCatalogSubsystem::DefaultCSVPath();

	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *CsvPath))
	{
		AddError(FString::Printf(TEXT("Could not read '%s'."), *CsvPath));
		return false;
	}

	// Parse into a throwaway table so the real asset is left alone.
	UDataTable* Parsed = NewObject<UDataTable>(GetTransientPackage(), TEXT("ParsedProductsForTest"));
	Parsed->RowStruct = FProductRow::StaticStruct();

	const TArray<FString> Problems = Parsed->CreateTableFromCSVString(Csv);
	for (const FString& Problem : Problems)
	{
		AddError(FString::Printf(TEXT("DT_Products.csv: %s"), *Problem));
	}
	if (Problems.Num() > 0)
	{
		return false;
	}

	const int32 ParsedRows = Parsed->GetRowMap().Num();
	if (ParsedRows == 0)
	{
		AddError(TEXT("DT_Products.csv parsed to zero rows."));
		return false;
	}

	// Every row has to be usable by the economy, or a product silently trades
	// at zero and the market model divides by it.
	int32 Checked = 0;
	for (const TPair<FName, uint8*>& Pair : Parsed->GetRowMap())
	{
		const FProductRow* Row = reinterpret_cast<const FProductRow*>(Pair.Value);
		const FString ID = Pair.Key.ToString();

		if (Row->NameTR.IsEmpty() || Row->NameEN.IsEmpty())
		{
			AddError(FString::Printf(TEXT("%s: NameTR or NameEN is empty."), *ID));
		}

		// Encoding canary. A UTF-8 file read as single-byte ANSI still parses
		// and still round-trips, so comparing the CSV against the asset cannot
		// catch it -- both sides would be wrong in the same way. What it cannot
		// hide is the signature: every Turkish character becomes a pair
		// starting with U+00C3 or U+00C5, neither of which belongs in this
		// data. If this fires, DT_Products.csv has lost its byte order mark.
		auto LooksMisdecoded = [](const FString& Text)
		{
			return Text.Contains(TEXT("\u00C3")) || Text.Contains(TEXT("\u00C5"));
		};
		if (LooksMisdecoded(Row->NameTR) || LooksMisdecoded(Row->NameEN))
		{
			AddError(FString::Printf(
				TEXT("%s: '%s' looks like UTF-8 read as ANSI. DT_Products.csv must keep ")
				TEXT("its UTF-8 byte order mark -- Unreal has no other way to tell."),
				*ID, *Row->NameTR));
		}
		if (Row->BasePrice <= 0.f)
		{
			AddError(FString::Printf(TEXT("%s: BasePrice is %.2f, must be > 0."), *ID, Row->BasePrice));
		}
		if (Row->VolumeBU <= 0.f)
		{
			AddError(FString::Printf(TEXT("%s: VolumeBU is %.2f, must be > 0."), *ID, Row->VolumeBU));
		}
		else if (!FMath::IsNearlyEqual(Row->VolumeBU,
			FProductRow::ContainerVolumeBU(Row->ContainerType), 1e-4f))
		{
			AddError(FString::Printf(
				TEXT("%s: VolumeBU %.2f does not match its container type's %.2f."),
				*ID, Row->VolumeBU, FProductRow::ContainerVolumeBU(Row->ContainerType)));
		}

		const FRiskBandParams Band = FProductRow::GetRiskBandParams(Row->RiskBand);
		if (Band.CeilingMultiplier <= Band.FloorMultiplier)
		{
			AddError(FString::Printf(TEXT("%s: risk band has ceiling <= floor."), *ID));
		}
		++Checked;
	}

	// Now the asset. If this half fails, the CSV is fine and someone just
	// forgot to re-import -- say exactly that rather than "data mismatch".
	UDataTable* Asset = UDeadlineSettings::Get().ProductTable.LoadSynchronous();
	if (!Asset)
	{
		AddWarning(TEXT("No product table set in Project Settings > Game > Deadline, ")
			TEXT("so the CSV could only be checked against itself."));
		AddInfo(FString::Printf(TEXT("%d products parsed cleanly from the CSV."), Checked));
		return true;
	}

	if (Asset->GetRowMap().Num() != ParsedRows)
	{
		AddError(FString::Printf(
			TEXT("DT_Products has %d rows but the CSV has %d. Re-import the asset: ")
			TEXT("Content/Deadline/Data/import_products_datatable.py"),
			Asset->GetRowMap().Num(), ParsedRows));
		return false;
	}

	int32 Drifted = 0;
	FString FirstDrift;
	for (const TPair<FName, uint8*>& Pair : Parsed->GetRowMap())
	{
		const FProductRow* FromCsv = reinterpret_cast<const FProductRow*>(Pair.Value);
		const FProductRow* FromAsset = Asset->FindRow<FProductRow>(Pair.Key, TEXT("ProductCsvTest"), false);
		if (!FromAsset)
		{
			++Drifted;
			if (FirstDrift.IsEmpty())
			{
				FirstDrift = FString::Printf(TEXT("%s is in the CSV but not in the asset"), *Pair.Key.ToString());
			}
			continue;
		}
		// Names are compared too, not just the economy numbers: they are the
		// columns most likely to be edited without a re-import, and a stale
		// name is invisible until someone reads it in game.
		if (!FMath::IsNearlyEqual(FromCsv->BasePrice, FromAsset->BasePrice, 1e-3f)
			|| FromCsv->RiskBand != FromAsset->RiskBand
			|| !FMath::IsNearlyEqual(FromCsv->VolumeBU, FromAsset->VolumeBU, 1e-3f)
			|| FromCsv->NameTR != FromAsset->NameTR
			|| FromCsv->NameEN != FromAsset->NameEN)
		{
			++Drifted;
			if (FirstDrift.IsEmpty())
			{
				FirstDrift = (FromCsv->NameTR != FromAsset->NameTR)
					? FString::Printf(TEXT("%s: CSV says NameTR '%s', asset says '%s'"),
						*Pair.Key.ToString(), *FromCsv->NameTR, *FromAsset->NameTR)
					: FString::Printf(TEXT("%s: CSV says base $%.2f, asset says $%.2f"),
						*Pair.Key.ToString(), FromCsv->BasePrice, FromAsset->BasePrice);
			}
		}
	}

	if (Drifted > 0)
	{
		AddError(FString::Printf(
			TEXT("%d products differ between DT_Products.csv and the imported asset (%s). ")
			TEXT("Re-import it: Content/Deadline/Data/import_products_datatable.py"),
			Drifted, *FirstDrift));
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("%d products parsed cleanly and match the imported DT_Products."), Checked));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
