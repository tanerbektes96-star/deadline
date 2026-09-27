// Copyright DEADLINE. All Rights Reserved.
//
// Lets an editor Python script build a Widget Blueprint's tree.
//
// Why this exists: UWidgetBlueprint::WidgetTree is protected, so stock editor
// Python cannot create a widget inside a Blueprint or set its root. Everything
// else UMG needs — adding a child to a panel, setting slot padding, alignment
// and brushes — is already reachable from Python. This module opens only the
// one door that was shut.
//
// Why bother at all: GDD 17 lists a dozen screens. The market screen and the
// travel map each took several rounds of "put a widget here, rename it, set
// that alignment", and a layout that lives only in someone's clicks cannot be
// reviewed, diffed or rebuilt. A layout written as a script can.
//
// Nothing here ships. The module is Editor-type and is compiled out of a game
// build entirely.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DeadlineWidgetTools.generated.h"

class UPanelWidget;
class UWidget;
class UWidgetBlueprint;

UCLASS()
class DEADLINE_EDITOR_API UDeadlineWidgetTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Create a widget inside the Blueprint's tree.
	 *
	 * @param WidgetName  becomes the widget's variable name, which is what
	 *                    meta=(BindWidget) matches against. Get it wrong and the
	 *                    Blueprint fails to compile, which is the intent.
	 * @param Parent      panel to add it to, or null to make it the tree root.
	 * @return the new widget, or null if the Blueprint or class was bad.
	 */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Editor")
	static UWidget* AddWidget(UWidgetBlueprint* Blueprint, TSubclassOf<UWidget> WidgetClass,
		FName WidgetName, UPanelWidget* Parent);

	/** Throw the whole tree away, root included. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Editor")
	static void ClearWidgetTree(UWidgetBlueprint* Blueprint);

	UFUNCTION(BlueprintCallable, Category = "Deadline|Editor")
	static UWidget* FindWidget(UWidgetBlueprint* Blueprint, FName WidgetName);

	/** Names of every widget in the tree, for verifying a build. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Editor")
	static TArray<FName> GetWidgetNames(UWidgetBlueprint* Blueprint);

	/** Recompile and save. Do this once, after the tree is finished. */
	UFUNCTION(BlueprintCallable, Category = "Deadline|Editor")
	static bool CompileAndSave(UWidgetBlueprint* Blueprint);
};
