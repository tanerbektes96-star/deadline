// Copyright DEADLINE. All Rights Reserved.
//
// Greybox developer HUD. Draws straight to the canvas so the Month 1 gate test
// is actually verifiable: without it you cannot tell a sale from a storage
// drop, because both make the box disappear.
//
// This is deliberately temporary. GDD 17 specifies the real HUD (cash, bank,
// company value, clock, active contract, heat gauge, carried box) and CLAUDE.md
// puts presentation in Blueprint/UMG. This class is the placeholder until the
// WBP_ widgets are built in the Month 4 showcase week; keep gameplay logic out
// of it, it only reads and draws.

#pragma once

#include "CoreMinimal.h"
#include "Economy/EconomySubsystem.h"
#include "Events/NewsSubsystem.h"
#include "GameFramework/HUD.h"
#include "DeadlineHUD.generated.h"

class ADeadlinePlayerCharacter;
class UEconomySubsystem;
class UInventorySubsystem;
class UTimeSubsystem;

/** A short-lived line of feedback, e.g. "SOLD  S01  +$18". */
USTRUCT()
struct FHUDToast
{
	GENERATED_BODY()

	FString Text;
	FLinearColor Colour = FLinearColor::White;
	float ExpiresAtSeconds = 0.f;
};

UCLASS()
class DEADLINE__API ADeadlineHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADeadlineHUD();

	virtual void DrawHUD() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** How long a toast stays on screen. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|HUD")
	float ToastDuration = 4.f;

	UFUNCTION()
	void HandleTransaction(const FTransactionRecord& Record);

	UFUNCTION()
	void HandleStockChanged(FName ProductID);

	/** Goods rotted, in the warehouse or in a truck. Announced on its own
	    rather than through the trade log: a write-off IS a sale for nothing,
	    which is what makes the accounting work, but telling the player "SOLD
	    S09 x4, +0 $" describes a loss as a sale. */
	UFUNCTION()
	void HandleStockSpoiled(FName ProductID, int32 Containers, bool bWasOnTheBooks);

	/** 08:00 and 12:00 news (GDD 16). Shown as a panel across the top for
	    BulletinDuration seconds; the full log is Dl_News until the news
	    screen exists. */
	UFUNCTION()
	void HandleBulletin(const FNewsBulletin& Bulletin);

	/** Real seconds a bulletin stays on screen. */
	UPROPERTY(EditDefaultsOnly, Category = "Deadline|HUD")
	float BulletinDuration = 14.f;

private:
	void DrawFunds(float& Y);
	void DrawStorage(float& Y);
	void DrawClock();
	void DrawCrosshairAndPrompt();
	void DrawLoadZone();
	void DrawCarried();
	void DrawToasts();
	void DrawBulletin();

	/** Full-screen cover while a trip is under way. GDD 9.2 asks for a couple
	    of seconds inside the cab; until there is a cab to sit in, the honest
	    version is that you do not watch yourself teleport. */
	void DrawTravelCover();

	void DrawLine(const FString& Text, float X, float& Y, const FLinearColor& Colour, float Scale = 1.f);

	void AddToast(const FString& Text, const FLinearColor& Colour);

	UEconomySubsystem* GetEconomy() const;
	UInventorySubsystem* GetInventory() const;
	UTimeSubsystem* GetTime() const;
	class UTravelSubsystem* GetTravel() const;
	ADeadlinePlayerCharacter* GetPlayer() const;

	UPROPERTY()
	TArray<FHUDToast> Toasts;

	UPROPERTY()
	FNewsBulletin ShownBulletin;

	float BulletinExpiresAtSeconds = 0.f;
};
