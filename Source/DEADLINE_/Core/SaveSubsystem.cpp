// Copyright DEADLINE. All Rights Reserved.

#include "Core/SaveSubsystem.h"

#include "Core/DeadlineSaveGame.h"
#include "Core/DeadlineSettings.h"
#include "Core/TimeSubsystem.h"
#include "Economy/EconomySubsystem.h"
#include "Economy/MarketSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/InventorySubsystem.h"
#include "Fleet/FleetSubsystem.h"
#include "Inventory/StorageClass.h"
#include "Travel/TravelSubsystem.h"
#include "Kismet/GameplayStatics.h"

void USaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UInventorySubsystem::StaticClass());
	Collection.InitializeDependency(UEconomySubsystem::StaticClass());
	Collection.InitializeDependency(UTimeSubsystem::StaticClass());
	Collection.InitializeDependency(UMarketSubsystem::StaticClass());
	Collection.InitializeDependency(UFleetSubsystem::StaticClass());
	Collection.InitializeDependency(UTravelSubsystem::StaticClass());
	Super::Initialize(Collection);

	if (GameSeed == 0)
	{
		GameSeed = FMath::Rand();  // one-off: picking a fresh run seed, not gameplay randomness
	}
}

bool USaveSubsystem::SaveGame(const FString& SlotName)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return false;
	}

	UDeadlineSaveGame* Save = Cast<UDeadlineSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UDeadlineSaveGame::StaticClass()));
	if (!Save)
	{
		return false;
	}

	Save->SaveVersion = UDeadlineSaveGame::LatestVersion;
	Save->GameSeed = GameSeed;

	if (const UTimeSubsystem* Time = GI->GetSubsystem<UTimeSubsystem>())
	{
		Save->TotalGameMinutes = Time->GetTotalMinutes();
	}

	if (const UEconomySubsystem* Economy = GI->GetSubsystem<UEconomySubsystem>())
	{
		Save->Cash = Economy->GetCash();
		Save->Bank = Economy->GetBank();
		Save->Transactions = Economy->GetTransactions();
	}

	if (const UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
	{
		Save->WarehouseCapacityBU = Inventory->GetCapacityBU();   // v1 field, informational
		for (int32 Index = 0; Index < NumStorageClasses; ++Index)
		{
			Save->StorageUnitCounts.Add(
				Inventory->GetUnitCount(FStorageClassRules::FromIndex(Index)));
		}

		for (const TPair<FName, FInventoryEntry>& Pair : Inventory->GetAllEntries())
		{
			FInventorySaveEntry& Entry = Save->Inventory.AddDefaulted_GetRef();
			Entry.ProductID = Pair.Key;
			Entry.PhysicalStock = Pair.Value.PhysicalStock;
			Entry.RecordedStock = Pair.Value.RecordedStock;
			Entry.PhysicalLooseBoxes = Pair.Value.PhysicalLooseBoxes;
			Entry.RecordedLooseBoxes = Pair.Value.RecordedLooseBoxes;
			Entry.Batches = Pair.Value.Batches;
		}
		Save->EmptyPallets = Inventory->GetEmptyPallets();
	}

	if (const UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
	{
		for (const TPair<FName, FVehicleCargo>& Truck : Fleet->GetAllCargo())
		{
			FVehicleCargoSaveEntry& Saved = Save->Fleet.AddDefaulted_GetRef();
			Saved.VehicleKey = Truck.Key;
			Saved.VehicleID = Truck.Value.VehicleID;
			for (const TPair<FName, FInventoryEntry>& Pair : Truck.Value.Entries)
			{
				FInventorySaveEntry& Line = Saved.Cargo.AddDefaulted_GetRef();
				Line.ProductID = Pair.Key;
				Line.PhysicalStock = Pair.Value.PhysicalStock;
				Line.RecordedStock = Pair.Value.RecordedStock;
				Line.PhysicalLooseBoxes = Pair.Value.PhysicalLooseBoxes;
				Line.RecordedLooseBoxes = Pair.Value.RecordedLooseBoxes;
				Line.Batches = Pair.Value.Batches;
			}
		}
	}

	if (const UTravelSubsystem* Travel = GI->GetSubsystem<UTravelSubsystem>())
	{
		Save->CurrentLocationID = Travel->GetCurrentLocation();
	}

	if (const UWorld* World = GI->GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				Save->PlayerLocation = Pawn->GetActorLocation();
				Save->PlayerRotation = PC->GetControlRotation();
			}
		}
	}

	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
	if (bSaved)
	{
		UE_LOG(LogTemp, Log, TEXT("[Deadline] Saved to '%s' (v%d)."), *SlotName, Save->SaveVersion);
		OnGameSaved.Broadcast();
	}
	return bSaved;
}

bool USaveSubsystem::LoadGame(const FString& SlotName)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI || !DoesSaveExist(SlotName))
	{
		return false;
	}

	UDeadlineSaveGame* Save = Cast<UDeadlineSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	const bool bWasV1 = Save && Save->SaveVersion == 1;
	if (!Save || !Migrate(*Save))
	{
		UE_LOG(LogTemp, Error, TEXT("[Deadline] Save '%s' could not be loaded."), *SlotName);
		return false;
	}

	GameSeed = Save->GameSeed;

	// Prices are not in the save file at all: they are a pure function of the
	// run seed and the day, both of which are. Dropping the cache is enough to
	// rebuild the exact market this save left behind.
	if (UMarketSubsystem* Market = GI->GetSubsystem<UMarketSubsystem>())
	{
		Market->ResetAll();
	}

	if (UTimeSubsystem* Time = GI->GetSubsystem<UTimeSubsystem>())
	{
		Time->RestoreTotalMinutes(Save->TotalGameMinutes);
	}

	if (UEconomySubsystem* Economy = GI->GetSubsystem<UEconomySubsystem>())
	{
		Economy->RestoreFunds(Save->Cash, Save->Bank);
		Economy->RestoreTransactions(Save->Transactions);
	}

	if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
	{
		Inventory->ResetAll();   // also puts storage back to its starting size

		for (int32 Index = 0; Index < Save->StorageUnitCounts.Num()
			&& Index < NumStorageClasses; ++Index)
		{
			Inventory->SetUnitCount(FStorageClassRules::FromIndex(Index),
				Save->StorageUnitCounts[Index]);
		}

		for (const FInventorySaveEntry& Saved : Save->Inventory)
		{
			FInventoryEntry Entry;
			Entry.PhysicalStock = Saved.PhysicalStock;
			Entry.RecordedStock = Saved.RecordedStock;
			Entry.PhysicalLooseBoxes = Saved.PhysicalLooseBoxes;
			Entry.RecordedLooseBoxes = Saved.RecordedLooseBoxes;
			Entry.Batches = Saved.Batches;
			Inventory->RestoreEntry(Saved.ProductID, Entry);
			Inventory->OnStockChanged.Broadcast(Saved.ProductID);
		}

		Inventory->RestoreEmptyPallets(Save->EmptyPallets);

		if (bWasV1)
		{
			GrowStorageToFitLoadedStock(*Inventory);
		}
	}

	if (UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
	{
		Fleet->ResetAll();
		for (const FVehicleCargoSaveEntry& Saved : Save->Fleet)
		{
			FVehicleCargo Cargo;
			Cargo.VehicleID = Saved.VehicleID;
			for (const FInventorySaveEntry& Line : Saved.Cargo)
			{
				FInventoryEntry Entry;
				Entry.PhysicalStock = Line.PhysicalStock;
				Entry.RecordedStock = Line.RecordedStock;
				Entry.PhysicalLooseBoxes = Line.PhysicalLooseBoxes;
				Entry.RecordedLooseBoxes = Line.RecordedLooseBoxes;
				Entry.Batches = Line.Batches;
				Cargo.Entries.Add(Line.ProductID, Entry);
			}
			Fleet->RestoreCargo(Saved.VehicleKey, Cargo);
			Fleet->OnCargoChanged.Broadcast(Saved.VehicleKey);
		}
	}

	if (UTravelSubsystem* Travel = GI->GetSubsystem<UTravelSubsystem>())
	{
		Travel->ResetAll();
		if (!Save->CurrentLocationID.IsNone())
		{
			Travel->SetCurrentLocation(Save->CurrentLocationID);
		}
	}

	if (const UWorld* World = GI->GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				Pawn->SetActorLocation(Save->PlayerLocation);
				PC->SetControlRotation(Save->PlayerRotation);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Deadline] Loaded '%s' (v%d, seed %d)."),
		*SlotName, Save->SaveVersion, GameSeed);
	OnGameLoaded.Broadcast();
	return true;
}

bool USaveSubsystem::DoesSaveExist(const FString& SlotName) const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, 0);
}

void USaveSubsystem::StartNewGame(int32 InSeed)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	GameSeed = (InSeed != 0) ? InSeed : FMath::Rand();

	if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
	{
		Inventory->ResetAll();
	}
	if (UEconomySubsystem* Economy = GI->GetSubsystem<UEconomySubsystem>())
	{
		Economy->ResetAll();
	}
	if (UTimeSubsystem* Time = GI->GetSubsystem<UTimeSubsystem>())
	{
		Time->ResetAll();
	}
	if (UMarketSubsystem* Market = GI->GetSubsystem<UMarketSubsystem>())
	{
		Market->ResetAll();
	}
	if (UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
	{
		Fleet->ResetAll();
	}
	if (UTravelSubsystem* Travel = GI->GetSubsystem<UTravelSubsystem>())
	{
		Travel->ResetAll();
	}

	UE_LOG(LogTemp, Log, TEXT("[Deadline] New game, seed %d."), GameSeed);
}

bool USaveSubsystem::Migrate(UDeadlineSaveGame& Save) const
{
	if (Save.SaveVersion == UDeadlineSaveGame::LatestVersion)
	{
		return true;
	}

	if (Save.SaveVersion > UDeadlineSaveGame::LatestVersion)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Deadline] Save is v%d but this build understands up to v%d."),
			Save.SaveVersion, UDeadlineSaveGame::LatestVersion);
		return false;
	}

	// Step the payload forward one version at a time, one block per version.
	if (Save.SaveVersion == 1)
	{
		// v1 had one pooled capacity and no storage classes. No warehouse
		// upgrade existed in v1, so every v1 save was the starting warehouse:
		// hand it the starting units and let LoadGame widen a class afterwards
		// if the old pooled ledger put more into one than it now holds.
		Save.StorageUnitCounts.Reset();
		Save.SaveVersion = 2;
	}

	if (Save.SaveVersion == 2)
	{
		// v2 had no vehicles at all, so an empty fleet is the truthful answer.
		Save.Fleet.Reset();
		Save.SaveVersion = 3;
	}

	if (Save.SaveVersion == 3)
	{
		// v3 had nowhere to travel to. None means "wherever the table calls
		// home", which is what ResetAll picks on load.
		Save.CurrentLocationID = NAME_None;
		Save.SaveVersion = 4;
	}

	if (Save.SaveVersion == 4)
	{
		// v4 goods had no age. Leaving the batch lists empty is deliberate:
		// RestoreEntry / RestoreCargo then stamp them with the moment they
		// load, so an old save resumes with fresh stock instead of watching
		// every perishable rot on the first day. Generous, and it only ever
		// happens once per save file.
		for (FInventorySaveEntry& Entry : Save.Inventory)
		{
			Entry.Batches.Reset();
		}
		for (FVehicleCargoSaveEntry& Truck : Save.Fleet)
		{
			for (FInventorySaveEntry& Line : Truck.Cargo)
			{
				Line.Batches.Reset();
			}
		}
		Save.SaveVersion = 5;
	}

	if (Save.SaveVersion == 5)
	{
		// v5 had no empty pallets, because opening one used to make the wood
		// vanish. Hand the save the starting count rather than zero: a v5
		// warehouse may hold loose boxes it could then never rebuild.
		Save.EmptyPallets = FMath::Max(0, UDeadlineSettings::Get().StartingEmptyPallets);
		Save.SaveVersion = 6;
	}

	Save.SaveVersion = UDeadlineSaveGame::LatestVersion;
	return true;
}

void USaveSubsystem::GrowStorageToFitLoadedStock(UInventorySubsystem& Inventory) const
{
	for (int32 Index = 0; Index < NumStorageClasses; ++Index)
	{
		const EStorageClass Class = FStorageClassRules::FromIndex(Index);
		const float PerUnit = FStorageClassRules::BUPerUnit(Class);
		if (PerUnit <= 0.f)
		{
			continue;
		}

		const int32 Needed = FMath::CeilToInt(Inventory.GetClassUsedBU(Class) / PerUnit);
		if (Needed > Inventory.GetUnitCount(Class))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Deadline] v1 save held more than %d units of %s; widened to %d."),
				Inventory.GetUnitCount(Class), *FStorageClassRules::DisplayName(Class).ToString(),
				Needed);
			Inventory.SetUnitCount(Class, Needed);
		}
	}
}
