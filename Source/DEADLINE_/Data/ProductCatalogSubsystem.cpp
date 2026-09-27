// Copyright DEADLINE. All Rights Reserved.

#include "Data/ProductCatalogSubsystem.h"

#include "Core/DeadlineLocale.h"
#include "Core/DeadlineSettings.h"
#include "Engine/DataTable.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

void UProductCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UDeadlineSettings& Settings = UDeadlineSettings::Get();
	ProductTable = Settings.ProductTable.LoadSynchronous();

	if (!ProductTable)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Deadline] Product table not set. Project Settings > Game > Deadline > Product Table."));
		return;
	}

	RebuildCache();

	UE_LOG(LogTemp, Log, TEXT("[Deadline] Product catalogue loaded: %d products."), CachedRows.Num());
}

void UProductCatalogSubsystem::RebuildCache()
{
	// The cache holds pointers into the table's row storage, so it has to be
	// rebuilt after any reimport -- the old pointers are freed by then.
	CachedRows.Reset();
	if (!ProductTable)
	{
		return;
	}
	// Row names are the CSV's ID column (see FProductRow header note).
	for (const TPair<FName, uint8*>& Pair : ProductTable->GetRowMap())
	{
		CachedRows.Add(Pair.Key, reinterpret_cast<FProductRow*>(Pair.Value));
	}
}

FString UProductCatalogSubsystem::DefaultCSVPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectContentDir(), TEXT("Deadline"), TEXT("Data"), TEXT("DT_Products.csv")));
}

bool UProductCatalogSubsystem::ReloadFromCSV(const FString& CsvPath, TArray<FString>& OutProblems)
{
	OutProblems.Reset();

	if (!ProductTable)
	{
		OutProblems.Add(TEXT("No product table is set."));
		return false;
	}

	const FString Path = CsvPath.IsEmpty() ? DefaultCSVPath() : CsvPath;

	FString Csv;
	if (!FFileHelper::LoadFileToString(Csv, *Path))
	{
		OutProblems.Add(FString::Printf(TEXT("Could not read '%s'."), *Path));
		return false;
	}

	// Replaces every row. Anything holding an FProductRow* from before this
	// call is now pointing at freed memory, hence RebuildCache immediately
	// after and the OnCatalogueReloaded broadcast for everyone else.
	OutProblems = ProductTable->CreateTableFromCSVString(Csv);
	RebuildCache();

	UE_LOG(LogTemp, Log, TEXT("[Deadline] Reloaded catalogue from '%s': %d products, %d problems."),
		*Path, CachedRows.Num(), OutProblems.Num());

	OnCatalogueReloaded.Broadcast();
	return true;
}

const FProductRow* UProductCatalogSubsystem::FindProduct(FName ProductID) const
{
	const FProductRow* const* Found = CachedRows.Find(ProductID);
	return Found ? *Found : nullptr;
}

bool UProductCatalogSubsystem::GetProduct(FName ProductID, FProductRow& OutProduct) const
{
	if (const FProductRow* Row = FindProduct(ProductID))
	{
		OutProduct = *Row;
		return true;
	}
	return false;
}

TArray<FName> UProductCatalogSubsystem::GetAllProductIDs() const
{
	TArray<FName> Result;
	CachedRows.GetKeys(Result);
	Result.Sort(FNameLexicalLess());
	return Result;
}

TArray<FName> UProductCatalogSubsystem::GetProductIDsForPhase(FName LaunchPhase) const
{
	TArray<FName> Result;
	for (const TPair<FName, FProductRow*>& Pair : CachedRows)
	{
		if (Pair.Value->LaunchPhase == LaunchPhase)
		{
			Result.Add(Pair.Key);
		}
	}
	Result.Sort(FNameLexicalLess());
	return Result;
}

FString UProductCatalogSubsystem::GetDisplayName(FName ProductID) const
{
	const FProductRow* Row = FindProduct(ProductID);
	if (!Row)
	{
		return FString();
	}

	return DeadlineLocale::Pick(Row->NameTR, Row->NameEN);
}

float UProductCatalogSubsystem::GetVolumeBU(FName ProductID) const
{
	const FProductRow* Row = FindProduct(ProductID);
	return Row ? Row->VolumeBU : 0.f;
}
