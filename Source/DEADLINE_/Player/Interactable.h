// Copyright DEADLINE. All Rights Reserved.
//
// Anything the player can look at and press E on. Implemented in C++ by
// AContainerActor and ATradePostActor, and available to Blueprints so level
// props can join in without new C++.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class DEADLINE__API IInteractable
{
	GENERATED_BODY()

public:
	/** Line shown on the HUD while focused, e.g. "E - Pick up". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Deadline|Interaction")
	FText GetInteractionPrompt(APawn* Interactor) const;

	/** False greys the prompt out: no room, no money, hands full. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Deadline|Interaction")
	bool CanInteract(APawn* Interactor) const;

	/** Do the thing. Only called when CanInteract returned true. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Deadline|Interaction")
	void Interact(APawn* Interactor);
};
