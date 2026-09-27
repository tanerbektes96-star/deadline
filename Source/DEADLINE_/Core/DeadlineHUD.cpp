// Copyright DEADLINE. All Rights Reserved.

#include "Core/DeadlineHUD.h"

#include "Actors/ContainerActor.h"
#include "Core/TimeSubsystem.h"
#include "Data/ProductCatalogSubsystem.h"
#include "Economy/EconomySubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Font.h"
#include "Fleet/FleetSubsystem.h"
#include "Travel/TravelSubsystem.h"
#include "Inventory/InventorySubsystem.h"
#include "Inventory/StorageClass.h"
#include "Player/DeadlinePlayerCharacter.h"

namespace
{
	const FLinearColor ColLabel(0.62f, 0.62f, 0.60f);
	const FLinearColor ColValue(0.96f, 0.94f, 0.90f);   // warm off-white (GDD 17)
	const FLinearColor ColGood(0.35f, 0.78f, 0.68f);    // calm blue-green
	const FLinearColor ColWarn(0.95f, 0.72f, 0.25f);    // amber
	const FLinearColor ColBad(0.85f, 0.35f, 0.32f);     // measured red

	/** Days of shelf life left before the warehouse line starts shouting. */
	constexpr int32 SpoilageWarningDays = 3;

	constexpr float Margin = 24.f;
	constexpr float LineStep = 22.f;
}

ADeadlineHUD::ADeadlineHUD()
{
	PrimaryActorTick.bCanEverTick = false;   // DrawHUD is driven by the renderer
}

void ADeadlineHUD::BeginPlay()
{
	Super::BeginPlay();

	if (UEconomySubsystem* Economy = GetEconomy())
	{
		Economy->OnTransactionRecorded.AddDynamic(this, &ADeadlineHUD::HandleTransaction);
	}
	if (UInventorySubsystem* Inventory = GetInventory())
	{
		Inventory->OnStockChanged.AddDynamic(this, &ADeadlineHUD::HandleStockChanged);
		Inventory->OnStockSpoiled.AddDynamic(this, &ADeadlineHUD::HandleStockSpoiled);
	}
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			Fleet->OnCargoSpoiled.AddDynamic(this, &ADeadlineHUD::HandleStockSpoiled);
		}
	}
}

void ADeadlineHUD::HandleStockSpoiled(FName ProductID, int32 Containers, bool bWasOnTheBooks)
{
	AddToast(FString::Printf(TEXT("CURUDU  %s x%d   (%s)"),
		*ProductID.ToString(), Containers,
		bWasOnTheBooks ? TEXT("zarar yazildi") : TEXT("gri, kagitsiz")),
		ColBad);
}

void ADeadlineHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UEconomySubsystem* Economy = GetEconomy())
	{
		Economy->OnTransactionRecorded.RemoveDynamic(this, &ADeadlineHUD::HandleTransaction);
	}
	if (UInventorySubsystem* Inventory = GetInventory())
	{
		Inventory->OnStockChanged.RemoveDynamic(this, &ADeadlineHUD::HandleStockChanged);
	}
	Super::EndPlay(EndPlayReason);
}

// --- subsystem access ---------------------------------------------------------

UEconomySubsystem* ADeadlineHUD::GetEconomy() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEconomySubsystem>() : nullptr;
}

UInventorySubsystem* ADeadlineHUD::GetInventory() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
}

UTravelSubsystem* ADeadlineHUD::GetTravel() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTravelSubsystem>() : nullptr;
}

UTimeSubsystem* ADeadlineHUD::GetTime() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UTimeSubsystem>() : nullptr;
}

ADeadlinePlayerCharacter* ADeadlineHUD::GetPlayer() const
{
	return GetOwningPawn() ? Cast<ADeadlinePlayerCharacter>(GetOwningPawn()) : nullptr;
}

// --- events -------------------------------------------------------------------

void ADeadlineHUD::HandleTransaction(const FTransactionRecord& Record)
{
	// A write-off is logged as a sale for nothing so that the cost basis
	// replay charges it against realised profit. HandleStockSpoiled has
	// already said so in the player's language; showing it again as a sale
	// would be the one place the accounting trick leaks into the fiction.
	if (Record.Containers < 0 && FMath::IsNearlyZero(Record.Amount))
	{
		return;
	}

	// Containers is positive when buying, negative when selling.
	const bool bBought = Record.Containers > 0;
	const TCHAR* Verb = bBought ? TEXT("BOUGHT") : TEXT("SOLD");
	const TCHAR* Ledger = (Record.Ledger == ETradeLedger::White) ? TEXT("invoiced") : TEXT("GREY");
	const TCHAR* Purse = (Record.Payment == EPaymentMethod::Cash) ? TEXT("cash") : TEXT("bank");

	AddToast(
		FString::Printf(TEXT("%s  %s x%d   %+.0f $  (%s, %s)"),
			Verb, *Record.ProductID.ToString(), FMath::Abs(Record.Containers),
			Record.Amount, Ledger, Purse),
		bBought ? ColWarn : ColGood);
}

void ADeadlineHUD::HandleStockChanged(FName ProductID)
{
	const UInventorySubsystem* Inventory = GetInventory();
	if (!Inventory)
	{
		return;
	}
	AddToast(
		FString::Printf(TEXT("STORAGE  %s   physical %d / recorded %d"),
			*ProductID.ToString(),
			Inventory->GetPhysicalStock(ProductID),
			Inventory->GetRecordedStock(ProductID)),
		ColValue);
}

void ADeadlineHUD::AddToast(const FString& Text, const FLinearColor& Colour)
{
	FHUDToast& Toast = Toasts.AddDefaulted_GetRef();
	Toast.Text = Text;
	Toast.Colour = Colour;
	Toast.ExpiresAtSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() + ToastDuration : 0.f;

	while (Toasts.Num() > 6)
	{
		Toasts.RemoveAt(0);
	}
}

// --- drawing -------------------------------------------------------------------

void ADeadlineHUD::DrawLine(const FString& Text, float X, float& Y,
	const FLinearColor& Colour, float Scale)
{
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (Canvas && Font)
	{
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Colour);
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.65f));
		Canvas->DrawItem(Item);
	}
	Y += LineStep * Scale;
}

void ADeadlineHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	// On the road there is nothing to look at and nothing to do: no warehouse
	// numbers, no crosshair, no watching the truck jump across the map.
	const UTravelSubsystem* Travel = GetTravel();
	if (Travel && Travel->IsTravelling())
	{
		DrawTravelCover();
		return;
	}

	float Y = Margin;
	DrawFunds(Y);
	Y += 6.f;
	DrawStorage(Y);

	DrawClock();
	DrawCrosshairAndPrompt();
	DrawLoadZone();
	DrawCarried();
	DrawToasts();
}

void ADeadlineHUD::DrawTravelCover()
{
	const UTravelSubsystem* Travel = GetTravel();
	if (!Travel)
	{
		return;
	}

	DrawRect(FLinearColor(0.02f, 0.02f, 0.02f, 1.f), 0.f, 0.f, Canvas->SizeX, Canvas->SizeY);

	const FTravelQuote& Quote = Travel->GetPendingQuote();
	FString Where = Quote.ToID.ToString();
	if (const FDestinationRow* Row = Travel->FindDestination(Quote.ToID))
	{
		Where = Row->NameTR.IsEmpty() ? Row->NameEN : Row->NameTR;
	}

	const float CX = Canvas->SizeX * 0.5f;
	float Y = Canvas->SizeY * 0.5f - 40.f;

	// Rough centring, same approximation the crosshair prompt uses.
	const FString Line = FString::Printf(TEXT("YOLDA  -  %s"), *Where);
	DrawLine(Line, CX - Line.Len() * 9.f * 1.6f * 0.5f, Y, ColValue, 1.6f);

	const FString Detail = FString::Printf(TEXT("%.1f km   %.0f dk   %.1f L"),
		Quote.DistanceKm, Quote.TravelMinutes, Quote.FuelLitres);
	DrawLine(Detail, CX - Detail.Len() * 9.f * 1.1f * 0.5f, Y, ColLabel, 1.1f);
}

void ADeadlineHUD::DrawFunds(float& Y)
{
	const UEconomySubsystem* Economy = GetEconomy();
	if (!Economy)
	{
		return;
	}

	DrawLine(FString::Printf(TEXT("CASH    $ %s"),
		*FString::FormatAsNumber(FMath::RoundToInt(Economy->GetCash()))), Margin, Y, ColValue, 1.2f);
	DrawLine(FString::Printf(TEXT("BANK    $ %s"),
		*FString::FormatAsNumber(FMath::RoundToInt(Economy->GetBank()))), Margin, Y, ColValue, 1.2f);
	DrawLine(FString::Printf(TEXT("TOTAL   $ %s"),
		*FString::FormatAsNumber(FMath::RoundToInt(Economy->GetTotalFunds()))), Margin, Y, ColLabel);
}

void ADeadlineHUD::DrawStorage(float& Y)
{
	const UInventorySubsystem* Inventory = GetInventory();
	if (!Inventory)
	{
		return;
	}

	const float Used = Inventory->GetUsedBU();
	const float Capacity = Inventory->GetCapacityBU();
	DrawLine(FString::Printf(TEXT("STORAGE %.1f / %.0f BU"), Used, Capacity), Margin, Y,
		Used >= Capacity ? ColBad : ColLabel);

	// A full cold zone is invisible in the total, so each class gets its own
	// line (GDD 11).
	for (int32 Index = 0; Index < NumStorageClasses; ++Index)
	{
		const EStorageClass Class = FStorageClassRules::FromIndex(Index);
		const float ClassCapacity = Inventory->GetClassCapacityBU(Class);
		if (ClassCapacity <= 0.f)
		{
			continue;
		}
		const float ClassUsed = Inventory->GetClassUsedBU(Class);
		DrawLine(FString::Printf(TEXT("  %s  %.1f / %.0f BU  (x%d)"),
			*FStorageClassRules::DisplayName(Class).ToString(),
			ClassUsed, ClassCapacity, Inventory->GetUnitCount(Class)),
			Margin, Y, ClassUsed >= ClassCapacity ? ColBad : ColLabel);
	}

	// Spare wooden pallets (GDD 5.6). You cannot rebuild a pallet without one,
	// and the only place that fact is visible is here.
	DrawLine(FString::Printf(TEXT("  Empty pallets  x%d"), Inventory->GetEmptyPallets()),
		Margin, Y, Inventory->GetEmptyPallets() > 0 ? ColLabel : ColWarn);

	// The number an inspection looks at (GDD 7.1). Zero while you stay white.
	const float Discrepancy = Inventory->GetTotalDiscrepancy();
	if (!FMath::IsNearlyZero(Discrepancy))
	{
		DrawLine(FString::Printf(TEXT("DISCREPANCY %.2f"), Discrepancy), Margin, Y, ColBad);
	}

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UFleetSubsystem* Fleet = GI->GetSubsystem<UFleetSubsystem>())
		{
			for (const FName& Key : Fleet->GetVehicleKeys())
			{
				DrawLine(FString::Printf(TEXT("  %s [%s]  %.1f / %.0f BU   %.0f / %.0f kg"),
					*Key.ToString(), *Fleet->GetVehicleID(Key).ToString(),
					Fleet->GetUsedBU(Key), Fleet->GetCapacityBU(Key),
					Fleet->GetWeightKg(Key), Fleet->GetMaxWeightKg(Key)),
					Margin, Y, ColLabel);
			}
		}
	}

	for (const FName& ID : Inventory->GetStockedProductIDs())
	{
		const int32 Loose = Inventory->GetLooseBoxes(ID);
		const FString LooseText = Loose > 0 ? FString::Printf(TEXT("  +%d loose"), Loose) : FString();

		// Condition, but only when there is something to say. A shelf line that
		// carries "100%" and "never spoils" for sixty products is noise; a line
		// that stays quiet until the clock matters is a warning.
		const int32 DaysLeft = Inventory->GetDaysUntilSpoilage(ID);
		const float Condition = Inventory->GetConditionMultiplier(ID);

		FString Note;
		FLinearColor Colour = ColLabel;
		if (DaysLeft >= 0 && DaysLeft <= SpoilageWarningDays)
		{
			Note = FString::Printf(TEXT("   %d GUN KALDI"), DaysLeft);
			Colour = DaysLeft <= 1 ? ColBad : ColWarn;
		}
		else if (DaysLeft >= 0)
		{
			Note = FString::Printf(TEXT("   %d gun"), DaysLeft);
		}
		if (Condition < 0.995f)
		{
			Note += FString::Printf(TEXT("   deger %%%.0f"), Condition * 100.f);
		}

		DrawLine(FString::Printf(TEXT("  %s   x%d  (recorded %d)%s%s"),
			*ID.ToString(), Inventory->GetPhysicalStock(ID), Inventory->GetRecordedStock(ID),
			*LooseText, *Note),
			Margin, Y, Colour);
	}
}

void ADeadlineHUD::DrawClock()
{
	const UTimeSubsystem* Time = GetTime();
	if (!Time)
	{
		return;
	}
	float Y = Margin;
	DrawLine(Time->GetClockString(), Canvas->SizeX - 200.f, Y, ColValue, 1.2f);
}

void ADeadlineHUD::DrawCrosshairAndPrompt()
{
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	// Small dot, not a weapon reticle.
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.55f), CX - 2.f, CY - 2.f, 4.f, 4.f);

	ADeadlinePlayerCharacter* Player = GetPlayer();
	if (!Player)
	{
		return;
	}

	const FText Prompt = Player->GetFocusPrompt();
	if (Prompt.IsEmpty())
	{
		return;
	}

	const bool bEnabled = Player->CanInteractWithFocus();
	float Y = CY + 48.f;

	// Rough centring: the medium font is about 9 px per character at scale 1.3.
	const FString Text = Prompt.ToString();
	const float Width = Text.Len() * 9.f * 1.3f;
	DrawLine(Text, CX - Width * 0.5f, Y, bEnabled ? ColValue : ColLabel, 1.3f);
}

void ADeadlineHUD::DrawLoadZone()
{
	const ADeadlinePlayerCharacter* Player = GetPlayer();
	if (!Player)
	{
		return;
	}

	// A loading zone is somewhere you stand, not something you look at, so its
	// prompt sits below the crosshair one rather than replacing it.
	const FText Prompt = Player->GetLoadPrompt();
	if (Prompt.IsEmpty())
	{
		return;
	}

	const float CX = Canvas->SizeX * 0.5f;
	float Y = Canvas->SizeY * 0.5f + 76.f;

	const FString Text = Prompt.ToString();
	const float Width = Text.Len() * 9.f * 1.3f;
	DrawLine(Text, CX - Width * 0.5f, Y,
		Player->CanUseLoadZone() ? ColValue : ColLabel, 1.3f);
}

void ADeadlineHUD::DrawCarried()
{
	const ADeadlinePlayerCharacter* Player = GetPlayer();
	if (!Player)
	{
		return;
	}

	const AContainerActor* Carried = Player->GetCarriedContainer();
	float Y = Canvas->SizeY - Margin - LineStep;

	if (!Carried)
	{
		// Say the key: you push the jack in front of you, below the camera, so
		// there is nothing to look at and press E on.
		DrawLine(Player->HasPalletJack() ? TEXT("PALLET JACK  (empty)   Q - park") : TEXT("hands empty"),
			Margin, Y, Player->HasPalletJack() ? ColValue : ColLabel);
		return;
	}

	const int32 DaysLeft = Carried->GetDaysUntilSpoilage();
	const float Condition = Carried->GetConditionMultiplier();

	FString Note;
	if (DaysLeft >= 0)
	{
		Note += FString::Printf(TEXT("   %d gun kaldi"), DaysLeft);
	}
	if (Condition < 0.995f)
	{
		Note += FString::Printf(TEXT("   deger %%%.0f"), Condition * 100.f);
	}

	// The count first: on the forks it is the number you are working towards.
	DrawLine(FString::Printf(TEXT("%s  %d x %s   %.1f BU   %s%s"),
		Player->HasPalletJack() ? TEXT("ON THE JACK") : TEXT("CARRYING "),
		Carried->Containers,
		*Carried->ProductID.ToString(),
		Carried->GetVolumeBU(),
		Carried->bRecorded ? TEXT("invoiced") : TEXT("GREY"),
		*Note),
		Margin, Y,
		// A box about to spoil in your hands outranks the white/grey colour:
		// one is a bookkeeping fact, the other is money running out.
		(DaysLeft >= 0 && DaysLeft <= 1) ? ColBad
			: (Carried->bRecorded ? ColValue : ColWarn), 1.2f);
}

void ADeadlineHUD::DrawToasts()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Toasts.RemoveAll([Now](const FHUDToast& T) { return T.ExpiresAtSeconds <= Now; });

	float Y = Canvas->SizeY * 0.5f - 120.f;
	for (const FHUDToast& Toast : Toasts)
	{
		DrawLine(Toast.Text, Canvas->SizeX - 520.f, Y, Toast.Colour);
	}
}
