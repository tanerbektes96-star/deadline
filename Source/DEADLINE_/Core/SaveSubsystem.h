// Copyright DEADLINE. All Rights Reserved.
//
// Gathers state from the other subsystems into UDeadlineSaveGame and puts it
// back on load. Also owns the run seed, because that is the one value every
// deterministic system needs and it must survive a reload.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveSubsystem.generated.h"

class UDeadlineSaveGame;
class UInventorySubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameSaved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameLoaded);

UCLASS()
class DEADLINE__API USaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Save")
	FOnGameSaved OnGameSaved;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Save")
	FOnGameLoaded OnGameLoaded;

	UFUNCTION(BlueprintCallable, Category = "Deadline|Save")
	bool SaveGame(const FString& SlotName = TEXT("DeadlineSlot0"));

	UFUNCTION(BlueprintCallable, Category = "Deadline|Save")
	bool LoadGame(const FString& SlotName = TEXT("DeadlineSlot0"));

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Save")
	bool DoesSaveExist(const FString& SlotName = TEXT("DeadlineSlot0")) const;

	/** Wipe every subsystem back to starting state and roll a fresh seed. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Save")
	void StartNewGame(int32 InSeed = 0);

	/** The run seed. Subsystem FRandomStreams are derived from this. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Save")
	int32 GetGameSeed() const { return GameSeed; }

private:
	/** Bring an older payload up to LatestVersion. Returns false if the save is
	    too old to rescue. */
	bool Migrate(UDeadlineSaveGame& Save) const;

	/** A v1 save had one pooled capacity, so it could hold more of one storage
	    class than that class now fits. Widen the class rather than leaving the
	    warehouse permanently over its own limit. */
	void GrowStorageToFitLoadedStock(UInventorySubsystem& Inventory) const;

	UPROPERTY()
	int32 GameSeed = 0;
};
