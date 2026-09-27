// Copyright DEADLINE. All Rights Reserved.

#include "Travel/TravelSubsystem.h"

#include "Actors/DestinationActor.h"
#include "Actors/VehicleActor.h"
#include "Core/DeadlineSettings.h"
#include "Core/TimeSubsystem.h"
#include "Economy/EconomySubsystem.h"
#include "Events/EventSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fleet/FleetSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UFleetSubsystem::StaticClass());
	Collection.InitializeDependency(UEconomySubsystem::StaticClass());
	Collection.InitializeDependency(UTimeSubsystem::StaticClass());
	Collection.InitializeDependency(UEventSubsystem::StaticClass());
	Super::Initialize(Collection);

	ResetAll();
}

UEconomySubsystem* UTravelSubsystem::GetEconomy() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEconomySubsystem>() : nullptr;
}

UFleetSubsystem* UTravelSubsystem::GetFleet() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFleetSubsystem>() : nullptr;
}

UTimeSubsystem* UTravelSubsystem::GetTime() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
}

void UTravelSubsystem::EnsureTable() const
{
	if (DestinationTable)
	{
		return;
	}

	const TSoftObjectPtr<UDataTable>& Soft = UDeadlineSettings::Get().DestinationTable;
	if (Soft.IsNull())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Deadline] No DestinationTable set in Project Settings > Deadline."));
		return;
	}

	DestinationTable = Soft.LoadSynchronous();
	if (!DestinationTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[Deadline] DT_Destinations could not be loaded."));
	}
}

void UTravelSubsystem::ResetAll()
{
	bTravelling = false;
	CurrentLocationID = NAME_None;

	// Start at whichever row calls itself home, so the starting point is data
	// rather than a hardcoded ID.
	EnsureTable();
	if (DestinationTable)
	{
		for (const FName& ID : DestinationTable->GetRowNames())
		{
			const FDestinationRow* Row = FindDestination(ID);
			if (Row && Row->IsHome)
			{
				CurrentLocationID = ID;
				break;
			}
		}
	}
}

// --- Places ------------------------------------------------------------------

TArray<FName> UTravelSubsystem::GetDestinationIDs() const
{
	EnsureTable();

	TArray<FName> IDs;
	if (DestinationTable)
	{
		IDs = DestinationTable->GetRowNames();
		IDs.Sort(FNameLexicalLess());
	}
	return IDs;
}

const FDestinationRow* UTravelSubsystem::FindDestination(FName DestinationID) const
{
	EnsureTable();
	if (!DestinationTable || DestinationID.IsNone())
	{
		return nullptr;
	}
	return DestinationTable->FindRow<FDestinationRow>(
		DestinationID, TEXT("UTravelSubsystem"), /*bWarnIfMissing=*/false);
}

bool UTravelSubsystem::GetDestination(FName DestinationID, FDestinationRow& OutRow) const
{
	if (const FDestinationRow* Row = FindDestination(DestinationID))
	{
		OutRow = *Row;
		return true;
	}
	return false;
}

float UTravelSubsystem::GetDistanceKm(FName FromID, FName ToID) const
{
	const FDestinationRow* From = FindDestination(FromID);
	const FDestinationRow* To = FindDestination(ToID);
	if (!From || !To)
	{
		return 0.f;
	}

	// The map picture is the map. Marker separation scaled by one number is the
	// whole road network this game needs (GDD 9.1).
	const FVector2D Delta(To->MapX - From->MapX, To->MapY - From->MapY);
	return Delta.Size() * UDeadlineSettings::Get().CityScaleKm;
}

float UTravelSubsystem::GetTrafficMultiplier() const
{
	const UTimeSubsystem* Time = GetTime();
	if (!Time)
	{
		return 1.f;
	}

	const int32 Hour = Time->GetHour();
	const bool bRushHour = (Hour >= 7 && Hour < 9) || (Hour >= 17 && Hour < 19);
	return bRushHour ? UDeadlineSettings::Get().RushHourMultiplier : 1.f;
}

// --- Trips -------------------------------------------------------------------

FTravelQuote UTravelSubsystem::GetQuote(FName VehicleKey, FName ToID) const
{
	const UDeadlineSettings& Settings = UDeadlineSettings::Get();

	FTravelQuote Quote;
	Quote.FromID = CurrentLocationID;
	Quote.ToID = ToID;
	Quote.VehicleKey = VehicleKey;
	Quote.TrafficMultiplier = GetTrafficMultiplier();

	const UFleetSubsystem* Fleet = GetFleet();
	const FDestinationRow* To = FindDestination(ToID);
	const FVehicleRow* Vehicle = Fleet ? Fleet->FindVehicleRow(Fleet->GetVehicleID(VehicleKey)) : nullptr;

	if (!To)
	{
		Quote.Reason = NSLOCTEXT("Deadline", "TravelNoPlace", "No such place");
		return Quote;
	}
	if (!Vehicle)
	{
		Quote.Reason = NSLOCTEXT("Deadline", "TravelNoVehicle", "No such vehicle");
		return Quote;
	}
	if (Vehicle->WarehouseOnly)
	{
		Quote.Reason = NSLOCTEXT("Deadline", "TravelYardOnly", "This one never leaves the yard");
		return Quote;
	}
	if (ToID == CurrentLocationID)
	{
		Quote.Reason = NSLOCTEXT("Deadline", "TravelAlreadyHere", "Already here");
		return Quote;
	}
	if (bTravelling)
	{
		Quote.Reason = NSLOCTEXT("Deadline", "TravelBusy", "Already on the road");
		return Quote;
	}

	Quote.DistanceKm = GetDistanceKm(CurrentLocationID, ToID);

	// Time: distance over speed, stretched by rush hour (GDD 9.4).
	const float Speed = FMath::Max(1.f, Vehicle->SpeedKmh);
	Quote.TravelMinutes = (Quote.DistanceKm / Speed) * 60.f * Quote.TrafficMultiplier;

	// Fuel: a full truck burns up to 10% more than an empty one (GDD 9.4).
	const float MaxKg = FMath::Max(1.f, Vehicle->MaxWeightKg);
	Quote.LoadFraction = FMath::Clamp(Fleet->GetWeightKg(VehicleKey) / MaxKg, 0.f, 1.f);
	Quote.FuelLitres = Quote.DistanceKm * Vehicle->FuelPerKm
		* (1.f + Settings.LoadedFuelPenalty * Quote.LoadFraction);
	Quote.FuelCost = Quote.FuelLitres * Settings.FuelPricePerLitre;

	// Road closures and fuel hikes (GDD 13) make the trip dearer, not longer.
	if (const UEventSubsystem* Events = GetGameInstance()->GetSubsystem<UEventSubsystem>())
	{
		const UTimeSubsystem* Time = GetTime();
		Quote.EventMultiplier = Events->GetRouteCostMultiplier(Time ? Time->GetDay() : 0);
		Quote.FuelCost *= Quote.EventMultiplier;
	}

	const UEconomySubsystem* Economy = GetEconomy();
	if (!Economy || Economy->GetTotalFunds() < Quote.FuelCost)
	{
		Quote.Reason = NSLOCTEXT("Deadline", "TravelNoFuelMoney", "Not enough money for fuel");
		return Quote;
	}

	Quote.bPossible = true;
	return Quote;
}

bool UTravelSubsystem::BeginTravel(FName VehicleKey, FName ToID)
{
	const FTravelQuote Quote = GetQuote(VehicleKey, ToID);
	if (!Quote.bPossible)
	{
		return false;
	}

	UEconomySubsystem* Economy = GetEconomy();
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!Economy || !World)
	{
		return false;
	}

	// Fuel is a white operating cost, so it comes off the bank first; cash only
	// covers what the bank cannot. A proper expense ledger arrives with the
	// finance screen (GDD 17, Month 5) -- until then this is a plain movement.
	const float FromBank = FMath::Min(Economy->GetBank(), Quote.FuelCost);
	Economy->AddBank(-FromBank);
	if (Quote.FuelCost - FromBank > KINDA_SMALL_NUMBER)
	{
		Economy->AddCash(-(Quote.FuelCost - FromBank));
	}

	PendingQuote = Quote;
	bTravelling = true;
	OnTravelStarted.Broadcast(Quote);

	// GDD 9.2 step 5: a short ride, not a loading screen. The camera work that
	// puts you inside the cab is presentation and comes with the real vehicles.
	World->GetTimerManager().SetTimer(TravelTimer, this, &UTravelSubsystem::FinishTravel,
		FMath::Max(0.1f, UDeadlineSettings::Get().TravelTransitionSeconds), /*bLoop=*/false);
	return true;
}

void UTravelSubsystem::FinishTravel()
{
	// The clock jumps for the whole trip, not for the two seconds you watched.
	if (UTimeSubsystem* Time = GetTime())
	{
		Time->AdvanceMinutes(FMath::RoundToInt(PendingQuote.TravelMinutes));
	}

	CurrentLocationID = PendingQuote.ToID;
	bTravelling = false;

	PlaceAtDestination(PendingQuote.ToID, PendingQuote.VehicleKey);

	UE_LOG(LogTemp, Log, TEXT("[Deadline] Arrived at %s: %.1f km, %.0f min, %.1f L, $%.0f."),
		*PendingQuote.ToID.ToString(), PendingQuote.DistanceKm, PendingQuote.TravelMinutes,
		PendingQuote.FuelLitres, PendingQuote.FuelCost);

	OnTravelFinished.Broadcast(PendingQuote.ToID);
}

void UTravelSubsystem::PlaceAtDestination(FName DestinationID, FName VehicleKey)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	ADestinationActor* Marker = nullptr;
	for (TActorIterator<ADestinationActor> It(World); It; ++It)
	{
		if (It->DestinationID == DestinationID)
		{
			Marker = *It;
			break;
		}
	}
	if (!Marker)
	{
		// The trip still happened -- the clock moved and the fuel is gone. Only
		// the teleport is missing, and that is a level problem, not a rule one.
		UE_LOG(LogTemp, Warning,
			TEXT("[Deadline] No DestinationActor for '%s'; nothing was moved."),
			*DestinationID.ToString());
		return;
	}

	// The truck first, then the player beside it: GDD 9.2 step 6 is "you appear
	// next to your vehicle", and your load came with you.
	for (TActorIterator<AVehicleActor> It(World); It; ++It)
	{
		if (It->GetVehicleKey() == VehicleKey)
		{
			It->SetActorLocationAndRotation(
				Marker->GetVehicleParkLocation(), Marker->GetVehicleParkRotation());
			break;
		}
	}

	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->SetActorLocation(Marker->GetPlayerArrivalLocation());
			PC->SetControlRotation(Marker->GetVehicleParkRotation());
		}
	}
}
