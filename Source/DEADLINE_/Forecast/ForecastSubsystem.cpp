// Copyright DEADLINE. All Rights Reserved.

#include "Forecast/ForecastSubsystem.h"

#include "Core/DeadlineSettings.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Economy/EconomySubsystem.h"
#include "Economy/MarketSubsystem.h"
#include "Engine/GameInstance.h"
#include "Events/EventSubsystem.h"
#include "Fleet/FleetSubsystem.h"
#include "Inventory/InventorySubsystem.h"

void UForecastSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency(UProductCatalogSubsystem::StaticClass());
	Collection.InitializeDependency(UTimeSubsystem::StaticClass());
	Collection.InitializeDependency(UEventSubsystem::StaticClass());
	Collection.InitializeDependency(UMarketSubsystem::StaticClass());
	Collection.InitializeDependency(UEconomySubsystem::StaticClass());
	Collection.InitializeDependency(UInventorySubsystem::StaticClass());
	Collection.InitializeDependency(UFleetSubsystem::StaticClass());
	Super::Initialize(Collection);

	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnDayChanged.AddDynamic(this, &UForecastSubsystem::HandleDayChanged);
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
		{
			Inventory->OnStockChanged.AddDynamic(this, &UForecastSubsystem::HandleStockChanged);
		}
		if (UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			Fleet->OnCargoChanged.AddDynamic(this, &UForecastSubsystem::HandleStockChanged);
		}
	}
	if (UEconomySubsystem* Economy = GetEconomy())
	{
		Economy->OnTransactionRecorded.AddDynamic(this, &UForecastSubsystem::HandleTransaction);
	}
}

void UForecastSubsystem::Deinitialize()
{
	if (UTimeSubsystem* Time = GetTime())
	{
		Time->OnDayChanged.RemoveDynamic(this, &UForecastSubsystem::HandleDayChanged);
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
		{
			Inventory->OnStockChanged.RemoveDynamic(this, &UForecastSubsystem::HandleStockChanged);
		}
		if (UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			Fleet->OnCargoChanged.RemoveDynamic(this, &UForecastSubsystem::HandleStockChanged);
		}
	}
	if (UEconomySubsystem* Economy = GetEconomy())
	{
		Economy->OnTransactionRecorded.RemoveDynamic(this, &UForecastSubsystem::HandleTransaction);
	}
	Super::Deinitialize();
}

// --- Board ---------------------------------------------------------------------

TArray<FForecastEntry> UForecastSubsystem::GetBoard() const
{
	TArray<FForecastEntry> Board;
	const UEventSubsystem* Events = GetEvents();
	const UTimeSubsystem* Time = GetTime();
	if (!Events || !Time)
	{
		return Board;
	}
	const int32 Today = Time->GetDay();

	TArray<FEventSignal> Signals = Events->GetSignals(Today);
	Signals.Sort([](const FEventSignal& A, const FEventSignal& B) { return A.ExpectedDay < B.ExpectedDay; });
	for (const FEventSignal& S : Signals)
	{
		FForecastEntry& Entry = Board.AddDefaulted_GetRef();
		Entry.EventID = S.EventID;
		Entry.Status = EForecastStatus::Signal;
		Entry.Name = S.Name;
		Entry.Text = S.Text;
		Entry.Confidence = S.Confidence;
		Entry.Day = S.ExpectedDay;
		Entry.DaysAway = S.ExpectedDay - Today;
		Entry.RouteCostMultiplier = S.RouteCostMultiplier;
		Events->GetImpactRange(S.EventID, Entry.ImpactMin, Entry.ImpactMax);
		FillProducts(Entry, S.AffectedProducts, /*EventStartDay=*/INDEX_NONE);
		for (const FForecastCommitment& C : Commitments)
		{
			if (C.State == ECommitmentState::Open && C.EventID == S.EventID && C.ExpectedDay == S.ExpectedDay)
			{
				Entry.CommitmentID = C.ID;
				break;
			}
		}
	}

	for (const FActiveEvent& E : Events->GetActiveEvents(Today))
	{
		FForecastEntry& Entry = Board.AddDefaulted_GetRef();
		Entry.EventID = E.EventID;
		Entry.Status = EForecastStatus::Active;
		Entry.Name = E.Name;
		Entry.Text = E.Headline;
		Entry.Confidence = 1.f;
		Entry.Day = E.EndDay - 1;
		Entry.DaysAway = Entry.Day - Today;
		Entry.RouteCostMultiplier = E.RouteCostMultiplier;
		Events->GetImpactRange(E.EventID, Entry.ImpactMin, Entry.ImpactMax);
		FillProducts(Entry, E.AffectedProducts, E.StartDay);
	}
	return Board;
}

void UForecastSubsystem::FillProducts(FForecastEntry& Entry, const TArray<FName>& ProductIDs, int32 EventStartDay) const
{
	const UEconomySubsystem* Economy = GetEconomy();
	const UMarketSubsystem* Market = GetMarket();
	const UProductCatalogSubsystem* Catalogue = GetCatalogue();

	for (const FName& ID : ProductIDs)
	{
		FForecastProductLine& Line = Entry.Products.AddDefaulted_GetRef();
		Line.ProductID = ID;
		Line.Name = Catalogue ? Catalogue->GetDisplayName(ID) : ID.ToString();
		Line.Price = Market ? Market->GetPrice(ID) : 0.f;
		if (Market && EventStartDay != INDEX_NONE)
		{
			const float Before = Market->GetPriceOnDay(ID, FMath::Max(0, EventStartDay - 1));
			Line.ChangePercent = Before > 0.f ? (Line.Price / Before - 1.f) * 100.f : 0.f;
		}
		Line.HeldContainers = Economy ? Economy->GetHeldContainers(ID) : 0.f;
		Line.HeldValue = Economy ? Economy->GetStockValueOf(ID) : 0.f;
		Entry.Exposure += Line.HeldValue;
	}

	// What you hold first, then the dearest: the lines that matter to you on top.
	Entry.Products.Sort([](const FForecastProductLine& A, const FForecastProductLine& B)
	{
		if ((A.HeldValue > 0.f) != (B.HeldValue > 0.f))
		{
			return A.HeldValue > 0.f;
		}
		return A.HeldValue != B.HeldValue ? A.HeldValue > B.HeldValue : A.Price > B.Price;
	});

	Entry.GainIfHappensMin = Entry.Exposure * Entry.ImpactMin;
	Entry.GainIfHappensMax = Entry.Exposure * Entry.ImpactMax;
}

float UForecastSubsystem::GetTotalExposure() const
{
	TSet<FName> Counted;
	float Total = 0.f;
	for (const FForecastEntry& Entry : GetBoard())
	{
		for (const FForecastProductLine& Line : Entry.Products)
		{
			bool bAlready = false;
			Counted.Add(Line.ProductID, &bAlready);
			if (!bAlready)
			{
				Total += Line.HeldValue;
			}
		}
	}
	return Total;
}

// --- Commitments ---------------------------------------------------------------

ECommitRefusal UForecastSubsystem::CanCommit(FName EventID, FName ProductID, int32 Containers) const
{
	const UEventSubsystem* Events = GetEvents();
	const UTimeSubsystem* Time = GetTime();
	const UEconomySubsystem* Economy = GetEconomy();
	if (!Events || !Time || !Economy)
	{
		return ECommitRefusal::NotOnBoard;
	}
	// Signals only: once it is happening the forecast has been made for you.
	const TArray<FEventSignal> Signals = Events->GetSignals(Time->GetDay());
	const FEventSignal* Signal = Signals.FindByPredicate([EventID](const FEventSignal& S) { return S.EventID == EventID; });
	if (!Signal || !Signal->AffectedProducts.Contains(ProductID))
	{
		return ECommitRefusal::NotOnBoard;
	}
	if (FindLive(ProductID))
	{
		return ECommitRefusal::AlreadyCommitted;
	}
	if (Containers <= 0)
	{
		return ECommitRefusal::BadAmount;
	}
	if (Economy->GetBuyPrice(ProductID) * Containers > Economy->GetAvailableFunds())
	{
		return ECommitRefusal::NotEnoughFunds;
	}
	return ECommitRefusal::None;
}

int32 UForecastSubsystem::Commit(FName EventID, FName ProductID, int32 Containers, int32 HoldDays)
{
	if (CanCommit(EventID, ProductID, Containers) != ECommitRefusal::None)
	{
		return INDEX_NONE;
	}
	UEconomySubsystem* Economy = GetEconomy();
	const UEventSubsystem* Events = GetEvents();
	const int32 Today = GetTime()->GetDay();
	const TArray<FEventSignal> Signals = Events->GetSignals(Today);
	const FEventSignal& Signal = *Signals.FindByPredicate([EventID](const FEventSignal& S) { return S.EventID == EventID; });

	const float Price = Economy->GetBuyPrice(ProductID);
	const float Budget = Price * Containers;
	if (!Economy->LockFunds(ProductID, Budget))
	{
		return INDEX_NONE;
	}

	FForecastCommitment& C = Commitments.AddDefaulted_GetRef();
	C.ID = NextCommitmentID++;
	C.EventID = EventID;
	C.ProductID = ProductID;
	C.State = ECommitmentState::Open;
	C.Confidence = Signal.Confidence;
	C.CommitDay = Today;
	C.ExpectedDay = Signal.ExpectedDay;
	C.HoldDays = FMath::Clamp(HoldDays, 1, FMath::Max(1, UDeadlineSettings::Get().CommitHoldDaysMax));
	C.TargetContainers = Containers;
	C.Budget = Budget;
	C.BuyPriceAtCommit = Price;
	C.LockRemaining = Budget;

	UE_LOG(LogTemp, Log, TEXT("[Deadline] Commitment #%d: %s on %s, %d x %.0f locked, judged day %d."),
		C.ID, *ProductID.ToString(), *EventID.ToString(), Containers, Price, C.GetResolveDay());
	OnBoardChanged.Broadcast();
	return C.ID;
}

bool UForecastSubsystem::CancelCommitment(int32 CommitmentID)
{
	FForecastCommitment* C = FindByID(CommitmentID);
	if (!C || C->State != ECommitmentState::Open)
	{
		return false;
	}
	CloseLock(*C);
	C->State = ECommitmentState::Cancelled;
	OnBoardChanged.Broadcast();
	return true;
}

bool UForecastSubsystem::GetCommitment(int32 CommitmentID, FForecastCommitment& Out) const
{
	const FForecastCommitment* C = Commitments.FindByPredicate([CommitmentID](const FForecastCommitment& X) { return X.ID == CommitmentID; });
	if (!C)
	{
		return false;
	}
	Out = *C;
	return true;
}

int32 UForecastSubsystem::GetMaxAffordable(FName ProductID) const
{
	const UEconomySubsystem* Economy = GetEconomy();
	const float Price = Economy ? Economy->GetBuyPrice(ProductID) : 0.f;
	return Price > 0.f ? FMath::Max(0, FMath::FloorToInt(Economy->GetAvailableFunds() / Price)) : 0;
}

void UForecastSubsystem::ResetAll()
{
	// The economy drops its locks on its own reset; nothing to hand back.
	Commitments.Reset();
	NextCommitmentID = 1;
	OnBoardChanged.Broadcast();
}

void UForecastSubsystem::RestoreCommitments(const TArray<FForecastCommitment>& InCommitments)
{
	Commitments = InCommitments;
	NextCommitmentID = 1;
	UEconomySubsystem* Economy = GetEconomy();
	for (FForecastCommitment& C : Commitments)
	{
		NextCommitmentID = FMath::Max(NextCommitmentID, C.ID + 1);
		if (C.State == ECommitmentState::Open && C.LockRemaining > 0.f && Economy)
		{
			// Never lock more than is there: a save edited by hand, or funds
			// spent by a cost that ignores locks, must not leave a lock the
			// accounts cannot cover.
			C.LockRemaining = FMath::Min(C.LockRemaining, FMath::Max(0.f, Economy->GetAvailableFunds()));
			Economy->LockFunds(C.ProductID, C.LockRemaining);
		}
	}
	// A load can land past a day that would have moved a commitment on.
	if (const UTimeSubsystem* Time = GetTime())
	{
		AdvanceCommitments(Time->GetDay());
	}
	OnBoardChanged.Broadcast();
}

const FForecastCommitment* UForecastSubsystem::FindLive(FName ProductID) const
{
	return Commitments.FindByPredicate([ProductID](const FForecastCommitment& C) { return C.IsLive() && C.ProductID == ProductID; });
}

FForecastCommitment* UForecastSubsystem::FindByID(int32 CommitmentID)
{
	return Commitments.FindByPredicate([CommitmentID](const FForecastCommitment& C) { return C.ID == CommitmentID; });
}

void UForecastSubsystem::CloseLock(FForecastCommitment& C)
{
	if (UEconomySubsystem* Economy = GetEconomy())
	{
		Economy->ReleaseLock(C.ProductID);
	}
	C.LockRemaining = 0.f;
}

void UForecastSubsystem::AdvanceCommitments(int32 Today)
{
	for (FForecastCommitment& C : Commitments)
	{
		// The buying window shuts on the expected day: the event is either
		// under way or it is not, and the rest of the budget comes back.
		if (C.State == ECommitmentState::Open && Today >= C.ExpectedDay)
		{
			CloseLock(C);
			C.State = ECommitmentState::Holding;
		}
		if (C.State == ECommitmentState::Holding && Today >= C.GetResolveDay())
		{
			Judge(C);
		}
	}
}

void UForecastSubsystem::Judge(FForecastCommitment& C)
{
	// Whether it happened is public by now: it was either running on its day
	// or it was not. Nothing here reads the calendar's truth.
	if (const UEventSubsystem* Events = GetEvents())
	{
		for (const FActiveEvent& E : Events->GetActiveEvents(C.ExpectedDay))
		{
			if (E.EventID == C.EventID && E.StartDay == C.ExpectedDay && !E.bForced)
			{
				C.bEventHappened = true;
				break;
			}
		}
	}

	const UEconomySubsystem* Economy = GetEconomy();
	const UMarketSubsystem* Market = GetMarket();
	C.PriceAtResolve = Market ? Market->GetPrice(C.ProductID) : 0.f;
	const int32 Unsold = FMath::Max(0, C.BoughtContainers - C.SoldContainers);
	const float RestValue = Economy ? Economy->GetSellPrice(C.ProductID) * Unsold : 0.f;
	C.Result = C.Proceeds + RestValue - C.Spent;
	C.State = ECommitmentState::Resolved;

	UE_LOG(LogTemp, Log, TEXT("[Deadline] Commitment #%d judged: %s %s, bought %d, result %+.0f."),
		C.ID, *C.EventID.ToString(), C.bEventHappened ? TEXT("happened") : TEXT("did not happen"),
		C.BoughtContainers, C.Result);
	OnCommitmentResolved.Broadcast(C);
}

void UForecastSubsystem::HandleTransaction(const FTransactionRecord& Record)
{
	FForecastCommitment* C = Commitments.FindByPredicate([&Record](const FForecastCommitment& X)
	{
		return X.IsLive() && X.ProductID == Record.ProductID;
	});
	if (!C || Record.Containers == 0)
	{
		return;
	}
	if (Record.Containers > 0)
	{
		// Only buying before the day counts: after it, you are chasing the
		// news rather than acting on the forecast.
		if (C->State != ECommitmentState::Open)
		{
			return;
		}
		C->BoughtContainers += Record.Containers;
		C->Spent += -Record.Amount;
		if (const UEconomySubsystem* Economy = GetEconomy())
		{
			C->LockRemaining = Economy->GetLockedFundsFor(C->ProductID);
		}
	}
	else
	{
		const int32 Sold = -Record.Containers;
		const int32 Counted = FMath::Min(Sold, C->BoughtContainers - C->SoldContainers);
		if (Counted <= 0)
		{
			return;
		}
		C->SoldContainers += Counted;
		C->Proceeds += Record.Amount * Counted / Sold;
	}
	OnBoardChanged.Broadcast();
}

// --- Change notifications ----------------------------------------------------

void UForecastSubsystem::HandleDayChanged(int32 NewDay)
{
	AdvanceCommitments(NewDay);
	OnBoardChanged.Broadcast();
}

void UForecastSubsystem::HandleStockChanged(FName ProductID)
{
	OnBoardChanged.Broadcast();
}

// --- Lookups ----------------------------------------------------------------

UEventSubsystem* UForecastSubsystem::GetEvents() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEventSubsystem>() : nullptr;
}

UEconomySubsystem* UForecastSubsystem::GetEconomy() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEconomySubsystem>() : nullptr;
}

UMarketSubsystem* UForecastSubsystem::GetMarket() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UMarketSubsystem>() : nullptr;
}

UProductCatalogSubsystem* UForecastSubsystem::GetCatalogue() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UProductCatalogSubsystem>() : nullptr;
}

UTimeSubsystem* UForecastSubsystem::GetTime() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
}
