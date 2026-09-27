// Copyright DEADLINE. All Rights Reserved.
//
// "Seyahat Et" (GDD 9.2). The player never drives (GDD 9.1): they pick a place,
// the clock and the fuel gauge move, and a couple of seconds later they are
// standing next to their truck somewhere else.
//
// Everything the trip costs is still simulated (GDD 9.4) — distance, vehicle
// speed, load weight, rush hour. Only the steering is gone. That is what makes
// the decision "is this run worth making?" a real one while costing nothing in
// vehicle physics, traffic AI or road navigation.
//
// Not here yet, and deliberately:
//   Travel events (GDD 9.3) — the 20% roadblock / breakdown / grey-offer cards
//                             need UEventSubsystem and a decision screen, both
//                             Month 4. The roadmap puts them there.
//   Weather, driver skill    — GDD 9.4 lists both; neither system exists yet.

#pragma once

#include "CoreMinimal.h"
#include "Data/DestinationRow.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TravelSubsystem.generated.h"

class UDataTable;
class UEconomySubsystem;
class UFleetSubsystem;
class UTimeSubsystem;

/** What one trip would cost. Everything the map's sidebar needs (GDD 9.2). */
USTRUCT(BlueprintType)
struct FTravelQuote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	FName FromID;

	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	FName ToID;

	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	FName VehicleKey;

	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	float DistanceKm = 0.f;

	/** Game minutes the clock will jump. */
	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	float TravelMinutes = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	float FuelLitres = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	float FuelCost = 0.f;

	/** 1.0 normally, 1.35 in rush hour (GDD 9.4). */
	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	float TrafficMultiplier = 1.f;

	/** How full the truck is, 0..1. Adds up to 10% fuel (GDD 9.4). */
	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	float LoadFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	bool bPossible = false;

	/** Why not, when bPossible is false. Empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Travel")
	FText Reason;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTravelStarted, const FTravelQuote&, Quote);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTravelFinished, FName, ArrivedAt);

UCLASS()
class DEADLINE__API UTravelSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Travel")
	FOnTravelStarted OnTravelStarted;

	UPROPERTY(BlueprintAssignable, Category = "Deadline|Travel")
	FOnTravelFinished OnTravelFinished;

	// --- Places -------------------------------------------------------------

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	FName GetCurrentLocation() const { return CurrentLocationID; }

	/** Used by the save system and by a new game. Does not move anything. */
	void SetCurrentLocation(FName DestinationID) { CurrentLocationID = DestinationID; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	TArray<FName> GetDestinationIDs() const;

	const FDestinationRow* FindDestination(FName DestinationID) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Deadline|Travel")
	bool GetDestination(FName DestinationID, FDestinationRow& OutRow) const;

	/** Straight-line distance between two places, in km. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	float GetDistanceKm(FName FromID, FName ToID) const;

	/** 1.35 between 07:00-09:00 and 17:00-19:00, else 1.0 (GDD 9.4). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	float GetTrafficMultiplier() const;

	// --- Trips --------------------------------------------------------------

	/** Price a trip without committing to it. Safe to call every frame the map
	    is open; it touches no state. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Deadline|Travel")
	FTravelQuote GetQuote(FName VehicleKey, FName ToID) const;

	/**
	 * Commit. Charges the fuel now, then a short cab transition, then the
	 * clock jumps and the truck and the player are standing at the far end.
	 * @return false if the quote said no.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Travel")
	bool BeginTravel(FName VehicleKey, FName ToID);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	bool IsTravelling() const { return bTravelling; }

	/** The trip currently under way. Only meaningful while IsTravelling(). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|Travel")
	const FTravelQuote& GetPendingQuote() const { return PendingQuote; }

	void ResetAll();

private:
	UEconomySubsystem* GetEconomy() const;
	UFleetSubsystem* GetFleet() const;
	UTimeSubsystem* GetTime() const;

	void EnsureTable() const;

	/** Timer callback: the far end of the transition. */
	void FinishTravel();

	/** Put the truck and the player down at the destination marker. */
	void PlaceAtDestination(FName DestinationID, FName VehicleKey);

	UPROPERTY(Transient)
	mutable TObjectPtr<UDataTable> DestinationTable;

	/** Where the player is now. Saved. */
	UPROPERTY()
	FName CurrentLocationID;

	UPROPERTY()
	bool bTravelling = false;

	/** The trip in progress, kept so the timer callback knows what it is
	    finishing and so the cab cover can name where you are going. */
	FTravelQuote PendingQuote;

	FTimerHandle TravelTimer;
};
