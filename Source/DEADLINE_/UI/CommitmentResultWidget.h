// Copyright DEADLINE. All Rights Reserved.
//
// The result card (GDD 4 step 6, roadmap Month 4): shown when a forecast
// commitment is judged. Read top to bottom it answers, in this order:
//
//   Was the forecast right?                      (verdict)
//   Did I make money, and how much?              (outcome)
//   Where did the money go?                      (ledger lines)
//   Why? -- and what to do differently.          (the "neden yanıldım" card)
//
// The lesson is picked by UForecastSubsystem when it judges (ECommitmentLesson)
// so the notebook can say the same thing later; this widget only words it.
//
// ADeadlinePlayerController owns the queue: several commitments can resolve on
// the same midnight, and each gets its own card, one after the other.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Forecast/ForecastSubsystem.h"
#include "CommitmentResultWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

UCLASS()
class DEADLINE__API UCommitmentResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Fill the card. QueueLeft = cards still waiting after this one. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Forecast")
	void SetCommitment(const FForecastCommitment& Commitment, int32 QueueLeft);

	/** The card's words, one per line of it, in the game's language. Shared
	    with the notebook, which says the same thing in a smaller space. */
	static FLinearColor OutcomeTone(const FForecastCommitment& C);
	static FText Verdict(const FForecastCommitment& C);
	static FText Odds(const FForecastCommitment& C);
	static FText Outcome(const FForecastCommitment& C);
	static FText Ledger(const FForecastCommitment& C);
	static FText LessonTitle(const FForecastCommitment& C);
	static FText LessonBody(const FForecastCommitment& C, const FString& ProductName);

protected:
	virtual void NativeConstruct() override;

	// --- Bound widgets. Exact names required. ------------------------------

	/** Coloured by the outcome: profit, loss, or neutral. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> ResultFrame;

	/** "DOĞRU TAHMİN" / "YANLIŞ TAHMİN". */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> VerdictText;

	/** Event and product. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TitleText;

	/** What the source said and what happened. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OddsText;

	/** "KAZANÇ +$1.240" in large type. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> OutcomeText;

	/** Bought / sold / still held, one per line. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LedgerText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LessonTitleText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LessonText;

	/** "1 sonuç daha var", or empty. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QueueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

private:
	UFUNCTION()
	void HandleCloseClicked();
};
