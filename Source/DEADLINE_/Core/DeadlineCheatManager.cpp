// Copyright DEADLINE. All Rights Reserved.

#include "Core/DeadlineCheatManager.h"

#include "Actors/ContainerActor.h"
#include "Actors/PalletJackActor.h"
#include "Actors/StorageZoneActor.h"
#include "Actors/VehicleActor.h"
#include "Actors/TradePostActor.h"
#include "Core/DeadlinePlayerController.h"
#include "Core/SaveSubsystem.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Fleet/FleetSubsystem.h"
#include "Travel/TravelSubsystem.h"
#include "Economy/EconomySubsystem.h"
#include "Economy/MarketSubsystem.h"
#include "Events/EventSubsystem.h"
#include "Events/NewsSubsystem.h"
#include "Forecast/ForecastSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventorySubsystem.h"
#include "Inventory/StorageClass.h"

namespace
{
	template <typename T>
	T* GetSub(const UCheatManager* Cheat)
	{
		const APlayerController* PC = Cheat ? Cheat->GetOuterAPlayerController() : nullptr;
		const UGameInstance* GI = PC ? PC->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<T>() : nullptr;
	}
}

void UDeadlineCheatManager::Report(const FString& Message, bool bWarning) const
{
	UE_LOG(LogTemp, Log, TEXT("[Deadline] %s"), *Message);
	if (APlayerController* PC = GetOuterAPlayerController())
	{
		// Also into the console buffer, where a table stays readable: the
		// on-screen stack overlaps the HUD and expires after a few seconds.
		PC->ClientMessage(Message);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f,
			bWarning ? FColor::Orange : FColor::Green, Message);
	}
}

void UDeadlineCheatManager::ReportBlock(const TArray<FString>& Lines, bool bWarning) const
{
	for (const FString& Line : Lines)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline] %s"), *Line);
		if (APlayerController* PC = GetOuterAPlayerController())
		{
			PC->ClientMessage(Line);
		}
	}

	// Backwards on purpose. UEngine::DrawOnscreenDebugMessages walks the
	// priority stack from the end, so the last message added is the one drawn
	// at the top; adding the last line first puts the first line up there.
	if (GEngine)
	{
		for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.f,
				bWarning ? FColor::Orange : FColor::Green, Lines[Index]);
		}
	}
}

// --- Money -------------------------------------------------------------------

void UDeadlineCheatManager::Dl_AddCash(float Amount)
{
	if (UEconomySubsystem* Economy = GetSub<UEconomySubsystem>(this))
	{
		Economy->AddCash(Amount);
		Report(FString::Printf(TEXT("Cash %+.0f -> $%.0f"), Amount, Economy->GetCash()));
	}
}

void UDeadlineCheatManager::Dl_AddBank(float Amount)
{
	if (UEconomySubsystem* Economy = GetSub<UEconomySubsystem>(this))
	{
		Economy->AddBank(Amount);
		Report(FString::Printf(TEXT("Bank %+.0f -> $%.0f"), Amount, Economy->GetBank()));
	}
}

// --- Time --------------------------------------------------------------------

void UDeadlineCheatManager::Dl_AdvanceTime(int32 Minutes)
{
	if (UTimeSubsystem* Time = GetSub<UTimeSubsystem>(this))
	{
		Time->AdvanceMinutes(Minutes);
		Report(FString::Printf(TEXT("Time +%d min -> %s"), Minutes, *Time->GetClockString()));
	}
}

void UDeadlineCheatManager::Dl_SetSpeed(int32 Speed)
{
	UTimeSubsystem* Time = GetSub<UTimeSubsystem>(this);
	if (!Time)
	{
		return;
	}

	EGameSpeed NewSpeed = EGameSpeed::Normal;
	switch (Speed)
	{
	case 0: NewSpeed = EGameSpeed::Paused;  break;
	case 2: NewSpeed = EGameSpeed::Fast;    break;
	case 4: NewSpeed = EGameSpeed::Fastest; break;
	default: NewSpeed = EGameSpeed::Normal; break;
	}
	Time->SetGameSpeed(NewSpeed);
	Report(FString::Printf(TEXT("Game speed set to %dx"), Speed));
}

// --- Boxes and stock ----------------------------------------------------------

void UDeadlineCheatManager::Dl_SpawnBox(FName ProductID, bool bRecorded)
{
	const UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
	if (!Catalogue || !Catalogue->IsValidProduct(ProductID))
	{
		Report(FString::Printf(TEXT("Unknown product '%s'. Try Dl_Products."), *ProductID.ToString()), true);
		return;
	}

	APlayerController* PC = GetOuterAPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return;
	}

	const FVector Where = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 150.f;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AContainerActor* Box = World->SpawnActor<AContainerActor>(
		AContainerActor::StaticClass(), FTransform(Where), Params))
	{
		Box->InitFromProduct(ProductID, bRecorded);
		Report(FString::Printf(TEXT("Spawned %s (%s)"),
			*ProductID.ToString(), bRecorded ? TEXT("white") : TEXT("grey")));
	}
}

void UDeadlineCheatManager::Dl_AddStock(FName ProductID, int32 Containers, bool bRecorded)
{
	UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	const UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
	if (!Inventory || !Catalogue)
	{
		return;
	}
	if (!Catalogue->IsValidProduct(ProductID))
	{
		Report(FString::Printf(TEXT("Unknown product '%s'."), *ProductID.ToString()), true);
		return;
	}

	if (Inventory->AddStock(ProductID, Containers, bRecorded))
	{
		Report(FString::Printf(TEXT("%s x%d added (%s). %.1f / %.1f BU used."),
			*ProductID.ToString(), Containers, bRecorded ? TEXT("white") : TEXT("grey"),
			Inventory->GetUsedBU(), Inventory->GetCapacityBU()));
	}
	else
	{
		Report(FString::Printf(TEXT("No room in the %s."),
			*FStorageClassRules::DisplayName(
				Inventory->GetStorageClassFor(ProductID)).ToString()), true);
	}
}

namespace
{
	/** Parse a storage class typed at the console. Returns false on a typo
	    rather than silently resizing the racks. */
	bool ParseStorageClass(FName Text, EStorageClass& OutClass)
	{
		for (int32 Index = 0; Index < NumStorageClasses; ++Index)
		{
			const EStorageClass Class = FStorageClassRules::FromIndex(Index);
			const FString Name = StaticEnum<EStorageClass>()->GetNameStringByValue(Index);
			if (Text.ToString().Equals(Name, ESearchCase::IgnoreCase))
			{
				OutClass = Class;
				return true;
			}
		}
		return false;
	}
}

void UDeadlineCheatManager::Dl_SetZone(FName Class, int32 Units)
{
	UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	if (!Inventory)
	{
		return;
	}

	EStorageClass Parsed = EStorageClass::Rack;
	if (!ParseStorageClass(Class, Parsed))
	{
		Report(TEXT("Class must be Rack, PalletBay, ColdZone or SecureRack."), true);
		return;
	}

	Inventory->SetUnitCount(Parsed, Units);
	Report(FString::Printf(TEXT("%s: %d units, %.0f BU."),
		*FStorageClassRules::DisplayName(Parsed).ToString(),
		Inventory->GetUnitCount(Parsed), Inventory->GetClassCapacityBU(Parsed)));
}

void UDeadlineCheatManager::BuildConditionReport(TArray<FString>& OutLines) const
{
	const UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	const UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
	if (!Inventory || !Catalogue)
	{
		return;
	}

	OutLines.Add(TEXT("--- DURUM ---   mal        yas      bozulma            deger"));

	const TArray<FName> IDs = Inventory->GetStockedProductIDs();
	if (IDs.Num() == 0)
	{
		OutLines.Add(TEXT("  depo bos"));
	}

	for (const FName& ID : IDs)
	{
		const FProductRow* Row = Catalogue->FindProduct(ID);
		if (!Row)
		{
			continue;
		}

		// "3 gun kaldi (6)" rather than "3/6": a slash between two day counts
		// reads as a fraction, which is what the last pass of this looked like.
		const int32 DaysLeft = Inventory->GetDaysUntilSpoilage(ID);
		const FString Spoils = Row->ShelfLifeDays > 0
			? FString::Printf(TEXT("%d gun kaldi (%d)"), DaysLeft, Row->ShelfLifeDays)
			: FString(TEXT("bozulmaz"));

		OutLines.Add(FString::Printf(TEXT("  %-5s %6.2f kap   %5.1f gun   %-18s  %%%.0f"),
			*ID.ToString(),
			Inventory->GetPhysicalContainers(ID),
			Inventory->GetAverageAgeDays(ID),
			*Spoils,
			Inventory->GetConditionMultiplier(ID) * 100.f));
	}

	if (const UFleetSubsystem* Fleet = GetSub<UFleetSubsystem>(this))
	{
		for (const FName& ID : Fleet->GetAllCarriedProductIDs())
		{
			OutLines.Add(FString::Printf(TEXT("  %-5s %6.2f kap   yolda                          %%%.0f"),
				*ID.ToString(), Fleet->GetTotalContainers(ID),
				Fleet->GetConditionMultiplier(ID) * 100.f));
		}
	}
}

void UDeadlineCheatManager::Dl_Condition()
{
	TArray<FString> Lines;
	BuildConditionReport(Lines);
	ReportBlock(Lines);
}

void UDeadlineCheatManager::Dl_AgeStock(float Days)
{
	UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	if (!Inventory)
	{
		return;
	}

	Inventory->AgeAllStockByDays(Days);

	// One block, not two: separate calls would interleave on the message stack
	// and the header would come back out from under the table.
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("Mal %.1f gun yaslandirildi."), Days));
	BuildConditionReport(Lines);
	ReportBlock(Lines);
}

void UDeadlineCheatManager::Dl_Storage()
{
	const UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	if (!Inventory)
	{
		return;
	}

	Report(FString::Printf(TEXT("--- Storage %.1f / %.0f BU ---"),
		Inventory->GetUsedBU(), Inventory->GetCapacityBU()));
	for (int32 Index = 0; Index < NumStorageClasses; ++Index)
	{
		const EStorageClass Class = FStorageClassRules::FromIndex(Index);
		Report(FString::Printf(TEXT("  %-12s x%-3d  %6.1f / %5.0f BU  free %.1f"),
			*FStorageClassRules::DisplayName(Class).ToString(),
			Inventory->GetUnitCount(Class),
			Inventory->GetClassUsedBU(Class),
			Inventory->GetClassCapacityBU(Class),
			Inventory->GetClassFreeBU(Class)));
	}
}

// --- Pallets ------------------------------------------------------------------

void UDeadlineCheatManager::Dl_Pallets(int32 Count)
{
	UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	if (!Inventory)
	{
		return;
	}

	if (Count > 0)
	{
		Inventory->AddEmptyPallets(Count);
	}
	Report(FString::Printf(TEXT("Empty pallets: %d"), Inventory->GetEmptyPallets()));
}

void UDeadlineCheatManager::Dl_Jack()
{
	APlayerController* PC = GetOuterAPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = Pawn ? Pawn->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	APalletJackActor* Jack = nullptr;
	for (TActorIterator<APalletJackActor> It(World); It; ++It)
	{
		Jack = *It;
		break;
	}
	if (!Jack)
	{
		Report(TEXT("No pallet jack in the level. Run setup_month3.py."), /*bWarning=*/true);
		return;
	}

	// Bring it to the player rather than the other way round: you call for a
	// jack when you are standing where you need it.
	if (Jack->IsHeld())
	{
		Report(TEXT("You are already pushing it."));
		return;
	}
	Jack->SetActorLocation(Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 150.f);
	Report(TEXT("Pallet jack brought to you."));
}

void UDeadlineCheatManager::Dl_OpenPallet(FName ProductID, int32 Pallets)
{
	UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	if (!Inventory)
	{
		return;
	}

	if (Inventory->OpenPallet(ProductID, Pallets))
	{
		Report(FString::Printf(TEXT("%s: %d pallet(s) opened, %d loose boxes on the racks."),
			*ProductID.ToString(), Pallets, Inventory->GetLooseBoxes(ProductID)));
	}
	else
	{
		Report(TEXT("Not a pallet product, not enough pallets, or the racks are full."), true);
	}
}

void UDeadlineCheatManager::Dl_ClosePallet(FName ProductID, int32 Pallets)
{
	UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	if (!Inventory)
	{
		return;
	}

	if (Inventory->ClosePallet(ProductID, Pallets))
	{
		Report(FString::Printf(TEXT("%s: %d pallet(s) closed, %d loose boxes left."),
			*ProductID.ToString(), Pallets, Inventory->GetLooseBoxes(ProductID)));
	}
	else
	{
		Report(TEXT("Fewer than 16 loose boxes, or the pallet bay is full."), true);
	}
}

// --- Fleet --------------------------------------------------------------------

namespace
{
	/** The named truck, or the first one in the fleet when the name is blank.
	    Typing a key every time would make these cheats useless in practice. */
	FName ResolveVehicleKey(const UFleetSubsystem& Fleet, FName Wanted)
	{
		if (!Wanted.IsNone() && Fleet.HasVehicle(Wanted))
		{
			return Wanted;
		}
		const TArray<FName> Keys = Fleet.GetVehicleKeys();
		return Keys.Num() > 0 ? Keys[0] : NAME_None;
	}
}

void UDeadlineCheatManager::Dl_Fleet()
{
	const UFleetSubsystem* Fleet = GetSub<UFleetSubsystem>(this);
	if (!Fleet)
	{
		return;
	}

	const TArray<FName> Keys = Fleet->GetVehicleKeys();
	if (Keys.IsEmpty())
	{
		Report(TEXT("No vehicles in the level."), true);
		return;
	}

	for (const FName& Key : Keys)
	{
		Report(FString::Printf(TEXT("--- %s [%s]  %.1f / %.0f BU   %.0f / %.0f kg ---"),
			*Key.ToString(), *Fleet->GetVehicleID(Key).ToString(),
			Fleet->GetUsedBU(Key), Fleet->GetCapacityBU(Key),
			Fleet->GetWeightKg(Key), Fleet->GetMaxWeightKg(Key)));

		const FVehicleCargo* Cargo = Fleet->GetAllCargo().Find(Key);
		if (!Cargo || Cargo->Entries.IsEmpty())
		{
			Report(TEXT("  (empty)"));
			continue;
		}
		for (const TPair<FName, FInventoryEntry>& Pair : Cargo->Entries)
		{
			Report(FString::Printf(TEXT("  %-6s x%-3d recorded %-3d loose %d"),
				*Pair.Key.ToString(), Pair.Value.PhysicalStock,
				Pair.Value.RecordedStock, Pair.Value.PhysicalLooseBoxes));
		}
	}
}

void UDeadlineCheatManager::Dl_LoadCargo(FName ProductID, int32 Count, FName VehicleKey, bool bRecorded)
{
	UFleetSubsystem* Fleet = GetSub<UFleetSubsystem>(this);
	if (!Fleet)
	{
		return;
	}

	const FName Key = ResolveVehicleKey(*Fleet, VehicleKey);
	if (Key.IsNone())
	{
		Report(TEXT("No vehicles in the level."), true);
		return;
	}

	FText Reason;
	if (!Fleet->CanLoad(Key, ProductID, Count, /*bLoose=*/false, Reason))
	{
		Report(Reason.ToString(), true);
		return;
	}

	Fleet->Load(Key, ProductID, Count, bRecorded, /*bLoose=*/false);
	Report(FString::Printf(TEXT("%s x%d aboard %s. %.1f / %.0f BU."),
		*ProductID.ToString(), Count, *Key.ToString(),
		Fleet->GetUsedBU(Key), Fleet->GetCapacityBU(Key)));
}

void UDeadlineCheatManager::Dl_EmptyVehicle(FName VehicleKey)
{
	UFleetSubsystem* Fleet = GetSub<UFleetSubsystem>(this);
	if (!Fleet)
	{
		return;
	}

	const FName Key = ResolveVehicleKey(*Fleet, VehicleKey);
	if (Key.IsNone())
	{
		Report(TEXT("No vehicles in the level."), true);
		return;
	}

	// Copy first: unloading edits the map being read.
	const FVehicleCargo* Cargo = Fleet->GetAllCargo().Find(Key);
	if (!Cargo)
	{
		return;
	}
	TArray<TPair<FName, FInventoryEntry>> Lines;
	for (const TPair<FName, FInventoryEntry>& Pair : Cargo->Entries)
	{
		Lines.Add(Pair);
	}

	for (const TPair<FName, FInventoryEntry>& Line : Lines)
	{
		if (Line.Value.PhysicalStock > 0)
		{
			Fleet->Unload(Key, Line.Key, Line.Value.PhysicalStock, true, false);
		}
		if (Line.Value.PhysicalLooseBoxes > 0)
		{
			Fleet->Unload(Key, Line.Key, Line.Value.PhysicalLooseBoxes, true, true);
		}
	}
	Report(FString::Printf(TEXT("%s emptied."), *Key.ToString()));
}

void UDeadlineCheatManager::Dl_Vehicles()
{
	const UFleetSubsystem* Fleet = GetSub<UFleetSubsystem>(this);
	if (!Fleet)
	{
		return;
	}

	for (const FName& ID : Fleet->GetAllVehicleIDs())
	{
		FVehicleRow Row;
		if (!Fleet->GetVehicleRow(ID, Row))
		{
			continue;
		}
		Report(FString::Printf(TEXT("  %-4s %-26s %5.0f BU  %3.0f km/h  %.2f L/km  %6.0f kg%s%s%s"),
			*ID.ToString(), *Row.NameEN, Row.CapacityBU, Row.SpeedKmh, Row.FuelPerKm,
			Row.MaxWeightKg,
			Row.AllowsSecure ? TEXT("") : TEXT("  no-secure"),
			Row.ColdOnly ? TEXT("  cold-only") : TEXT(""),
			Row.WarehouseOnly ? TEXT("  yard-only") : TEXT("")));
	}
}

// --- Travel -------------------------------------------------------------------

void UDeadlineCheatManager::Dl_Destinations()
{
	UTravelSubsystem* Travel = GetSub<UTravelSubsystem>(this);
	UFleetSubsystem* Fleet = GetSub<UFleetSubsystem>(this);
	if (!Travel || !Fleet)
	{
		return;
	}

	const FName Key = ResolveVehicleKey(*Fleet, NAME_None);
	Report(FString::Printf(TEXT("--- At %s, traffic x%.2f, quoting for %s ---"),
		*Travel->GetCurrentLocation().ToString(), Travel->GetTrafficMultiplier(),
		*Key.ToString()));

	for (const FName& ID : Travel->GetDestinationIDs())
	{
		FDestinationRow Row;
		if (!Travel->GetDestination(ID, Row))
		{
			continue;
		}
		const FTravelQuote Quote = Travel->GetQuote(Key, ID);
		Report(FString::Printf(TEXT("  %-4s %-20s %-9s %5.1f km  %4.0f min  %5.1f L  $%-6.0f %s"),
			*ID.ToString(), *Row.NameEN, *Row.Category.ToString(),
			Quote.DistanceKm, Quote.TravelMinutes, Quote.FuelLitres, Quote.FuelCost,
			Quote.bPossible ? TEXT("") : *Quote.Reason.ToString()));
	}
}

void UDeadlineCheatManager::Dl_Travel(FName DestinationID, FName VehicleKey)
{
	UTravelSubsystem* Travel = GetSub<UTravelSubsystem>(this);
	UFleetSubsystem* Fleet = GetSub<UFleetSubsystem>(this);
	if (!Travel || !Fleet)
	{
		return;
	}

	const FName Key = ResolveVehicleKey(*Fleet, VehicleKey);
	const FTravelQuote Quote = Travel->GetQuote(Key, DestinationID);
	if (!Quote.bPossible)
	{
		Report(Quote.Reason.ToString(), true);
		return;
	}

	Travel->BeginTravel(Key, DestinationID);
	Report(FString::Printf(TEXT("%s -> %s: %.1f km, %.0f min, %.1f L, $%.0f."),
		*Quote.FromID.ToString(), *Quote.ToID.ToString(),
		Quote.DistanceKm, Quote.TravelMinutes, Quote.FuelLitres, Quote.FuelCost));
}

void UDeadlineCheatManager::Dl_OpenMap()
{
	if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOuterAPlayerController()))
	{
		PC->OpenTravelScreen();
	}
}

// --- Navigation ---------------------------------------------------------------

void UDeadlineCheatManager::Dl_Goto(FName Where)
{
	APlayerController* PC = GetOuterAPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return;
	}

	const FString Target = Where.ToString();
	AActor* Destination = nullptr;

	EStorageClass WantedClass = EStorageClass::Rack;
	const bool bWantsClass = ParseStorageClass(Where, WantedClass);

	if (Target.Equals(TEXT("Vehicle"), ESearchCase::IgnoreCase)
		|| Target.Equals(TEXT("Truck"), ESearchCase::IgnoreCase))
	{
		for (TActorIterator<AVehicleActor> It(World); It; ++It)
		{
			Destination = *It;
			break;
		}
	}
	else if (bWantsClass || Target.Equals(TEXT("Storage"), ESearchCase::IgnoreCase))
	{
		for (TActorIterator<AStorageZoneActor> It(World); It; ++It)
		{
			if (bWantsClass && It->StorageClass != WantedClass)
			{
				continue;
			}
			Destination = *It;
			break;
		}
	}
	else
	{
		const ETradePostRole WantedRole = Target.Equals(TEXT("Buyer"), ESearchCase::IgnoreCase)
			? ETradePostRole::Buyer
			: ETradePostRole::Supplier;
		for (TActorIterator<ATradePostActor> It(World); It; ++It)
		{
			if (It->PostRole == WantedRole)
			{
				Destination = *It;
				break;
			}
		}
	}

	if (!Destination)
	{
		Report(FString::Printf(
			TEXT("No such point '%s'. Try Supplier, Buyer, Storage, Vehicle, or a storage class ")
			TEXT("(Rack / PalletBay / ColdZone / SecureRack)."), *Target), true);
		return;
	}

	// Stand off along the line back to the middle of the warehouse, so the
	// approach side is always the open one.
	const FVector Here = Destination->GetActorLocation();
	FVector Away = (FVector(0.f, 0.f, Here.Z) - Here).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = -Destination->GetActorForwardVector();
	}

	const FVector Stand = Here + Away * 220.f + FVector(0.f, 0.f, 60.f);
	Pawn->SetActorLocation(Stand, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	PC->SetControlRotation((Here - Stand).Rotation());

	Report(FString::Printf(TEXT("Moved to %s"), *Destination->GetActorLabel()));
}

// --- Reporting ----------------------------------------------------------------

void UDeadlineCheatManager::Dl_Inventory()
{
	const UInventorySubsystem* Inventory = GetSub<UInventorySubsystem>(this);
	const UEconomySubsystem* Economy = GetSub<UEconomySubsystem>(this);
	const UTimeSubsystem* Time = GetSub<UTimeSubsystem>(this);
	if (!Inventory || !Economy)
	{
		return;
	}

	Report(FString::Printf(TEXT("--- %s ---"), Time ? *Time->GetClockString() : TEXT("Deadline")));
	Report(FString::Printf(TEXT("Cash $%.0f   Bank $%.0f   Total $%.0f"),
		Economy->GetCash(), Economy->GetBank(), Economy->GetTotalFunds()));
	Report(FString::Printf(TEXT("Storage %.1f / %.1f BU   total discrepancy %.2f"),
		Inventory->GetUsedBU(), Inventory->GetCapacityBU(), Inventory->GetTotalDiscrepancy()));

	const TArray<FName> IDs = Inventory->GetStockedProductIDs();
	if (IDs.IsEmpty())
	{
		Report(TEXT("(warehouse empty)"));
		return;
	}
	for (const FName& ID : IDs)
	{
		const int32 Loose = Inventory->GetLooseBoxes(ID);
		Report(FString::Printf(TEXT("  %-6s physical %3d   recorded %3d   diff %+.2f%s"),
			*ID.ToString(),
			Inventory->GetPhysicalStock(ID),
			Inventory->GetRecordedStock(ID),
			Inventory->GetDiscrepancy(ID),
			Loose > 0 ? *FString::Printf(TEXT("   loose %d"), Loose) : TEXT("")));
	}
}

void UDeadlineCheatManager::Dl_Products(FName LaunchPhase)
{
	const UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
	const UEconomySubsystem* Economy = GetSub<UEconomySubsystem>(this);
	if (!Catalogue)
	{
		return;
	}

	const TArray<FName> IDs = LaunchPhase.IsNone()
		? Catalogue->GetAllProductIDs()
		: Catalogue->GetProductIDsForPhase(LaunchPhase);

	Report(FString::Printf(TEXT("%d products:"), IDs.Num()));
	for (const FName& ID : IDs)
	{
		const FProductRow* Row = Catalogue->FindProduct(ID);
		if (!Row)
		{
			continue;
		}
		UE_LOG(LogTemp, Log, TEXT("[Deadline]   %-6s %-38s base $%-8.0f buy $%-8.0f %4.1f BU  %s"),
			*ID.ToString(), *Row->NameEN, Row->BasePrice,
			Economy ? Economy->GetBuyPrice(ID) : 0.f,
			Row->VolumeBU, *Row->LaunchPhase.ToString());
	}
	Report(TEXT("(full list in the Output Log)"));
}

// --- Market -------------------------------------------------------------------

namespace
{
	const TCHAR* TrendArrow(EPriceTrend Trend)
	{
		switch (Trend)
		{
		case EPriceTrend::Rising:  return TEXT("up");
		case EPriceTrend::Falling: return TEXT("down");
		default:                   return TEXT("--");
		}
	}
}

void UDeadlineCheatManager::Dl_Market(FName ProductID)
{
	const UMarketSubsystem* Market = GetSub<UMarketSubsystem>(this);
	const UEconomySubsystem* Economy = GetSub<UEconomySubsystem>(this);
	if (!Market)
	{
		return;
	}

	if (!ProductID.IsNone())
	{
		const FMarketQuote Quote = Market->GetQuote(ProductID);
		if (Quote.BasePrice <= 0.f)
		{
			Report(FString::Printf(TEXT("Unknown product '%s'."), *ProductID.ToString()), true);
			return;
		}
		Report(FString::Printf(TEXT("%s  $%.2f  x%.2f  %+.1f%% %s   buy $%.2f  sell $%.2f"),
			*ProductID.ToString(), Quote.Price, Quote.Multiple, Quote.DayChangePercent,
			TrendArrow(Quote.Trend),
			Economy ? Economy->GetBuyPrice(ProductID) : 0.f,
			Economy ? Economy->GetSellPrice(ProductID) : 0.f));
		return;
	}

	const TArray<FMarketQuote> Quotes = Market->GetAllQuotes();
	Report(FString::Printf(TEXT("Market, day %d: %d products (full list in the Output Log)"),
		Market->GetMarketDay(), Quotes.Num()));
	for (const FMarketQuote& Quote : Quotes)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline]   %-6s $%-9.2f x%-5.2f %+6.1f%% %-4s base $%.2f"),
			*Quote.ProductID.ToString(), Quote.Price, Quote.Multiple,
			Quote.DayChangePercent, TrendArrow(Quote.Trend), Quote.BasePrice);
	}
}

void UDeadlineCheatManager::Dl_MarketDump(FName ProductID, int32 Days)
{
	const UMarketSubsystem* Market = GetSub<UMarketSubsystem>(this);
	if (!Market)
	{
		return;
	}

	const TArray<double> Series = Market->PeekSeries(ProductID, FMath::Max(0, Days));
	if (Series.Num() == 0)
	{
		Report(FString::Printf(TEXT("Unknown product '%s'."), *ProductID.ToString()), true);
		return;
	}

	const double Base = Series[0];
	for (int32 Day = 0; Day < Series.Num(); ++Day)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline]   %s day %3d  $%9.4f  x%.3f"),
			*ProductID.ToString(), Day, Series[Day],
			Base > 0.0 ? Series[Day] / Base : 1.0);
	}
	Report(FString::Printf(TEXT("%s: %d days dumped to the Output Log."),
		*ProductID.ToString(), Series.Num()));
}

void UDeadlineCheatManager::Dl_MarketDumpAll(int32 Days)
{
	const UMarketSubsystem* Market = GetSub<UMarketSubsystem>(this);
	const UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
	const USaveSubsystem* Save = GetSub<USaveSubsystem>(this);
	if (!Market || !Catalogue || !Save)
	{
		return;
	}

	Days = FMath::Clamp(Days, 1, 5000);

	FString Csv = TEXT("Seed,ProductID,Band,Day,Price\n");
	const int32 Seed = Save->GetGameSeed();
	for (const FName& ID : Catalogue->GetAllProductIDs())
	{
		const FProductRow* Row = Catalogue->FindProduct(ID);
		if (!Row)
		{
			continue;
		}
		const TArray<double> Series = Market->PeekSeries(ID, Days);
		const FString Band = StaticEnum<ERiskBand>()->GetNameStringByValue(
			static_cast<int64>(Row->RiskBand));
		for (int32 Day = 0; Day < Series.Num(); ++Day)
		{
			Csv += FString::Printf(TEXT("%d,%s,%s,%d,%.9g\n"),
				Seed, *ID.ToString(), *Band, Day, Series[Day]);
		}
	}

	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(),
		FString::Printf(TEXT("MarketDump_%d.csv"), Seed));
	if (FFileHelper::SaveStringToFile(Csv, *Path))
	{
		Report(FString::Printf(TEXT("Wrote %s"), *Path));
	}
	else
	{
		Report(FString::Printf(TEXT("Could not write %s"), *Path), true);
	}
}

// --- Events (GDD 13) -----------------------------------------------------------

void UDeadlineCheatManager::Dl_Events(int32 Days)
{
	const UEventSubsystem* Events = GetSub<UEventSubsystem>(this);
	const UTimeSubsystem* Time = GetSub<UTimeSubsystem>(this);
	if (!Events || !Time)
	{
		return;
	}
	const int32 Today = Time->GetDay();

	const TArray<FActiveEvent> Active = Events->GetActiveEventsToday();
	const TArray<FEventSignal> Signals = Events->GetSignalsToday();
	Report(FString::Printf(TEXT("Day %d: %d active, %d signals, routes x%.2f (details in the Output Log)"),
		Today, Active.Num(), Signals.Num(), Events->GetRouteCostMultiplier(Today)));

	for (const FActiveEvent& E : Active)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline]   ACTIVE %s %s  +%.0f%%  days %d-%d%s  %d products"),
			*E.EventID.ToString(), *E.Name, E.Impact * 100.f, E.StartDay, E.EndDay - 1,
			E.bForced ? TEXT(" (forced)") : TEXT(""), E.AffectedProducts.Num());
	}
	for (const FEventSignal& S : Signals)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline]   SIGNAL %s %s  %.0f%%  expected day %d"),
			*S.EventID.ToString(), *S.Name, S.Confidence * 100.f, S.ExpectedDay);
	}

	UE_LOG(LogTemp, Log, TEXT("[Deadline]   Calendar, days %d-%d, with the answers:"), Today, Today + Days);
	for (const FScheduledEvent& E : Events->GetCalendar(Today, Today + Days))
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline]     day %3d signal %s %-22s %3.0f%% -> %s  start %d, end %d, +%.0f%%"),
			E.SignalDay, *E.EventID.ToString(), *Events->GetEventName(E.EventID), E.Confidence * 100.f,
			E.bHappens ? TEXT("HAPPENS") : TEXT("false  "), E.StartDay, E.EndDay, E.Impact * 100.f);
	}
}

void UDeadlineCheatManager::Dl_ForceEvent(FName EventID, float Impact, int32 Duration)
{
	UEventSubsystem* Events = GetSub<UEventSubsystem>(this);
	if (!Events)
	{
		return;
	}
	if (!Events->ForceEvent(EventID, Impact, Duration))
	{
		Report(FString::Printf(TEXT("Unknown event '%s'. Try Dl_Events, or see DT_Events.csv."), *EventID.ToString()), true);
		return;
	}
	const UTimeSubsystem* Time = GetSub<UTimeSubsystem>(this);
	const TArray<FName> Products = Events->GetAffectedProducts(EventID);
	Report(FString::Printf(TEXT("%s started: %d products pushed, routes x%.2f."),
		*Events->GetEventName(EventID), Products.Num(),
		Events->GetRouteCostMultiplier(Time ? Time->GetDay() : 0)));
}

void UDeadlineCheatManager::Dl_News(int32 Days)
{
	const UNewsSubsystem* News = GetSub<UNewsSubsystem>(this);
	if (!News)
	{
		return;
	}
	const TArray<FNewsBulletin> Log = News->GetNewsLog(Days);
	Report(FString::Printf(TEXT("%d bulletins in the last %d days (full text in the Output Log)"), Log.Num(), Days));
	static const TCHAR* KindNames[] = { TEXT("RUMOUR"), TEXT("HEADLINE"), TEXT("DENIED"), TEXT("ENDED"), TEXT("UPDATE") };
	for (const FNewsBulletin& B : Log)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline]   Day %d %02d:00"), B.Day, B.Hour);
		for (const FNewsItem& Item : B.Items)
		{
			UE_LOG(LogTemp, Log, TEXT("[Deadline]     %-8s %s %s%s"),
				KindNames[static_cast<int32>(Item.Kind)], *Item.EventID.ToString(), *Item.Text,
				Item.Kind == ENewsKind::Rumour ? *FString::Printf(TEXT("  [%.0f%%, day %d]"), Item.Confidence * 100.f, Item.RelatedDay) : TEXT(""));
		}
	}
}

void UDeadlineCheatManager::Dl_Forecast()
{
	const UForecastSubsystem* Forecast = GetSub<UForecastSubsystem>(this);
	if (!Forecast)
	{
		return;
	}
	const TArray<FForecastEntry> Board = Forecast->GetBoard();
	Report(FString::Printf(TEXT("Forecast board: %d entries, total exposure $%.0f (details in the Output Log)"),
		Board.Num(), Forecast->GetTotalExposure()));
	for (const FForecastEntry& E : Board)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline]   %s %-8s %-22s %3.0f%%  day %d (%+d)  impact +%.0f..%.0f%%  exposure $%.0f -> +$%.0f..%.0f"),
			*E.EventID.ToString(), E.Status == EForecastStatus::Signal ? TEXT("SIGNAL") : TEXT("ACTIVE"), *E.Name,
			E.Confidence * 100.f, E.Day, E.DaysAway, E.ImpactMin * 100.f, E.ImpactMax * 100.f,
			E.Exposure, E.GainIfHappensMin, E.GainIfHappensMax);
		for (const FForecastProductLine& P : E.Products)
		{
			UE_LOG(LogTemp, Log, TEXT("[Deadline]       %-6s $%-8.2f held %.1f ($%.0f)%s"),
				*P.ProductID.ToString(), P.Price, P.HeldContainers, P.HeldValue,
				E.Status == EForecastStatus::Active ? *FString::Printf(TEXT("  %+.0f%% since start"), P.ChangePercent) : TEXT(""));
		}
	}
}

void UDeadlineCheatManager::Dl_OpenForecast()
{
	if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOuterAPlayerController()))
	{
		PC->OpenForecastScreen();
	}
}

void UDeadlineCheatManager::Dl_Commit(FName EventID, FName ProductID, int32 Containers, int32 HoldDays)
{
	UForecastSubsystem* Forecast = GetSub<UForecastSubsystem>(this);
	if (!Forecast)
	{
		return;
	}
	for (const FForecastEntry& E : Forecast->GetBoard())
	{
		const bool bMatch = EventID.IsNone()
			? E.Status == EForecastStatus::Signal && E.ImpactMax > 0.f
			: E.EventID == EventID;
		if (bMatch && E.Products.Num() > 0)
		{
			EventID = E.EventID;
			if (ProductID.IsNone())
			{
				ProductID = E.Products[0].ProductID;
			}
			break;
		}
	}
	const ECommitRefusal Refusal = Forecast->CanCommit(EventID, ProductID, Containers);
	const int32 ID = Forecast->Commit(EventID, ProductID, Containers, HoldDays);
	Report(ID != INDEX_NONE
		? FString::Printf(TEXT("Commitment #%d: %s on %s, %d containers, hold %d days."), ID, *ProductID.ToString(), *EventID.ToString(), Containers, HoldDays)
		: FString::Printf(TEXT("Commitment refused (%s)."), *UEnum::GetValueAsString(Refusal)));
}

void UDeadlineCheatManager::Dl_Commitments()
{
	const UForecastSubsystem* Forecast = GetSub<UForecastSubsystem>(this);
	const UEconomySubsystem* Economy = GetSub<UEconomySubsystem>(this);
	if (!Forecast || !Economy)
	{
		return;
	}
	Report(FString::Printf(TEXT("%d commitments, $%.0f locked, $%.0f free (details in the Output Log)"),
		Forecast->GetCommitments().Num(), Economy->GetLockedFunds(), Economy->GetAvailableFunds()));
	for (const FForecastCommitment& C : Forecast->GetCommitments())
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline]   #%d %s %s/%s %.0f%% day %d->%d  bought %d/%d spent $%.0f  sold %d $%.0f  lock $%.0f  %s result %+.0f"),
			C.ID, *UEnum::GetValueAsString(C.State), *C.EventID.ToString(), *C.ProductID.ToString(), C.Confidence * 100.f,
			C.ExpectedDay, C.GetResolveDay(), C.BoughtContainers, C.TargetContainers, C.Spent, C.SoldContainers, C.Proceeds,
			C.LockRemaining, C.State == ECommitmentState::Resolved ? (C.bEventHappened ? TEXT("HAPPENED") : TEXT("DID-NOT")) : TEXT("-"),
			C.Result);
	}
}

void UDeadlineCheatManager::Dl_ShowResult(int32 CommitmentID)
{
	const UForecastSubsystem* Forecast = GetSub<UForecastSubsystem>(this);
	ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOuterAPlayerController());
	if (!Forecast || !PC)
	{
		return;
	}
	if (CommitmentID <= 0)
	{
		for (const FForecastCommitment& C : Forecast->GetCommitments())
		{
			if (C.State == ECommitmentState::Resolved)
			{
				CommitmentID = C.ID;
			}
		}
	}
	if (CommitmentID <= 0)
	{
		Report(TEXT("No judged commitment yet."));
		return;
	}
	PC->ShowResultCard(CommitmentID);
}

void UDeadlineCheatManager::Dl_OpenMarket()
{
	if (ADeadlinePlayerController* PC = Cast<ADeadlinePlayerController>(GetOuterAPlayerController()))
	{
		PC->ToggleMarketScreen();
		Report(PC->IsMarketScreenOpen() ? TEXT("Market screen open.") : TEXT("Market screen closed."));
	}
}

// --- Balance data -------------------------------------------------------------

void UDeadlineCheatManager::Dl_ReloadProducts()
{
	UProductCatalogSubsystem* Catalogue = GetSub<UProductCatalogSubsystem>(this);
	if (!Catalogue)
	{
		return;
	}

	TArray<FString> Problems;
	if (!Catalogue->ReloadFromCSV(FString(), Problems))
	{
		Report(Problems.Num() > 0 ? Problems[0] : TEXT("Reload failed."), true);
		return;
	}

	for (const FString& Problem : Problems)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Deadline]   %s"), *Problem);
	}
	Report(FString::Printf(TEXT("Reloaded %d products, %d problems%s. Prices rebuilt."),
		Catalogue->GetProductCount(), Problems.Num(),
		Problems.Num() > 0 ? TEXT(" (see Output Log)") : TEXT("")),
		Problems.Num() > 0);
}

// --- Save ---------------------------------------------------------------------

void UDeadlineCheatManager::Dl_Save()
{
	if (USaveSubsystem* Save = GetSub<USaveSubsystem>(this))
	{
		Report(Save->SaveGame() ? TEXT("Saved.") : TEXT("Save failed."), false);
	}
}

void UDeadlineCheatManager::Dl_Load()
{
	if (USaveSubsystem* Save = GetSub<USaveSubsystem>(this))
	{
		Report(Save->LoadGame() ? TEXT("Loaded.") : TEXT("Load failed (no save?)."), false);
	}
}

void UDeadlineCheatManager::Dl_NewGame(int32 Seed)
{
	if (USaveSubsystem* Save = GetSub<USaveSubsystem>(this))
	{
		Save->StartNewGame(Seed);
		Report(FString::Printf(TEXT("New game, seed %d."), Save->GetGameSeed()));
	}
}
