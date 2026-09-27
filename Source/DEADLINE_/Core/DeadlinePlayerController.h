// Copyright DEADLINE. All Rights Reserved.
//
// Exists to install UDeadlineCheatManager, to keep the mouse cursor and input
// mode sane for a first-person game, and to own the full-screen menus.
//
// Opening a screen is a controller job because it swaps the input mode: while
// the market is up the mouse drives the UI, not the camera.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

class UUserWidget;   // TSoftClassPtr comes in with CoreMinimal.h

#include "DeadlinePlayerController.generated.h"

UCLASS()
class DEADLINE__API ADeadlinePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADeadlinePlayerController();

	/** Open the market screen (GDD 17) and hand input to it. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void OpenMarketScreen();

	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void CloseMarketScreen();

	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void ToggleMarketScreen();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|UI")
	bool IsMarketScreenOpen() const;

	/** Open the travel map (GDD 9.2) and hand input to it.
	    @param VehicleKey  the truck you pressed E on, so the map quotes for
	                       that one rather than guessing. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void OpenTravelScreen(FName VehicleKey = NAME_None);

	/** Which truck the open map is quoting for. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|UI")
	FName GetTravelVehicleKey() const { return TravelVehicleKey; }

	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void CloseTravelScreen();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|UI")
	bool IsTravelScreenOpen() const;

	/** The forecast board (GDD 17): today's signals, their confidence, and
	    what you stand to gain or lose on each. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void OpenForecastScreen();

	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void CloseForecastScreen();

	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void ToggleForecastScreen();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|UI")
	bool IsForecastScreenOpen() const;

	/** Queue the result card for a judged commitment (GDD 4 step 6). Cards
	    show one at a time; closing one brings up the next. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void ShowResultCard(int32 CommitmentID);

	UFUNCTION(BlueprintCallable, Category = "Deadline|UI")
	void CloseResultCard();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Deadline|UI")
	bool IsResultCardOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** Travel takes the controls away and gives them back: you are a passenger
	    for those seconds, not someone who can walk out of the cab. */
	UFUNCTION() void HandleTravelStarted(const struct FTravelQuote& Quote);
	UFUNCTION() void HandleTravelFinished(FName ArrivedAt);

	UFUNCTION() void HandleCommitmentResolved(const struct FForecastCommitment& Commitment);

private:
	/** Build a screen from a soft class, show it, and give it the mouse.
	    @param SettingName  named in the log when the class is not set, so the
	                        message says which project setting to go and fill in. */
	void OpenScreen(const TSoftClassPtr<UUserWidget>& ScreenClass, const TCHAR* SettingName,
		TObjectPtr<UUserWidget>& OutScreen);

	/** Take a screen down and give input back to the game. */
	void CloseScreen(TObjectPtr<UUserWidget>& Screen);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MarketScreen;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> TravelScreen;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ForecastScreen;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ResultCard;

	/** Commitment IDs waiting for their card, oldest first. */
	TArray<int32> ResultQueue;

	void ShowNextResult();

	UPROPERTY(Transient)
	FName TravelVehicleKey;
};
