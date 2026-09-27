// Copyright DEADLINE. All Rights Reserved.

#include "DeadlineWidgetTools.h"

#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace
{
	/**
	 * UWidgetBlueprint::WidgetTree is protected, so reach it through the
	 * property system rather than by inheriting from the Blueprint class.
	 *
	 * This is the one piece of reflection trickery in the project. It is here
	 * and nowhere else: everything above this function talks to ordinary public
	 * UMG API.
	 */
	UWidgetTree* GetWidgetTree(UWidgetBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return nullptr;
		}

		const FObjectProperty* Property = FindFProperty<FObjectProperty>(
			UWidgetBlueprint::StaticClass(), TEXT("WidgetTree"));
		if (!Property)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[Deadline] UWidgetBlueprint has no WidgetTree property; UMG changed shape."));
			return nullptr;
		}
		return Cast<UWidgetTree>(Property->GetObjectPropertyValue_InContainer(Blueprint));
	}
}

UWidget* UDeadlineWidgetTools::AddWidget(UWidgetBlueprint* Blueprint,
	TSubclassOf<UWidget> WidgetClass, FName WidgetName, UPanelWidget* Parent)
{
	UWidgetTree* Tree = GetWidgetTree(Blueprint);
	if (!Tree || !WidgetClass)
	{
		return nullptr;
	}

	Blueprint->Modify();
	Tree->Modify();

	// A name already in use would silently produce Widget_1 and break the
	// BindWidget the caller is relying on, so say so instead.
	if (StaticFindObject(nullptr, Tree, *WidgetName.ToString()))
	{
		UE_LOG(LogTemp, Error, TEXT("[Deadline] '%s' already exists in %s."),
			*WidgetName.ToString(), *Blueprint->GetName());
		return nullptr;
	}

	UWidget* NewWidget = Tree->ConstructWidget<UWidget>(WidgetClass, WidgetName);
	if (!NewWidget)
	{
		return nullptr;
	}
	NewWidget->SetDisplayLabel(WidgetName.ToString());

	if (Parent)
	{
		Parent->AddChild(NewWidget);
	}
	else
	{
		Tree->RootWidget = NewWidget;
	}

	return NewWidget;
}

void UDeadlineWidgetTools::ClearWidgetTree(UWidgetBlueprint* Blueprint)
{
	UWidgetTree* Tree = GetWidgetTree(Blueprint);
	if (!Tree)
	{
		return;
	}

	Blueprint->Modify();
	Tree->Modify();

	// Rename the old widgets out of the way. Without this a rebuild that reuses
	// the same names collides with the objects it is replacing, which are still
	// in the package until the next garbage collect.
	TArray<UWidget*> All;
	Tree->GetAllWidgets(All);
	for (UWidget* Widget : All)
	{
		if (Widget)
		{
			Widget->Rename(nullptr, GetTransientPackage(),
				REN_DontCreateRedirectors | REN_DoNotDirty);
		}
	}

	Tree->RootWidget = nullptr;
}

UWidget* UDeadlineWidgetTools::FindWidget(UWidgetBlueprint* Blueprint, FName WidgetName)
{
	const UWidgetTree* Tree = GetWidgetTree(Blueprint);
	return Tree ? Tree->FindWidget(WidgetName) : nullptr;
}

TArray<FName> UDeadlineWidgetTools::GetWidgetNames(UWidgetBlueprint* Blueprint)
{
	TArray<FName> Names;

	UWidgetTree* Tree = GetWidgetTree(Blueprint);
	if (!Tree)
	{
		return Names;
	}

	TArray<UWidget*> All;
	Tree->GetAllWidgets(All);
	for (const UWidget* Widget : All)
	{
		if (Widget)
		{
			Names.Add(Widget->GetFName());
		}
	}
	return Names;
}

bool UDeadlineWidgetTools::CompileAndSave(UWidgetBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	UPackage* Package = Blueprint->GetOutermost();
	Package->MarkPackageDirty();

	return UEditorLoadingAndSavingUtils::SavePackages({ Package }, /*bOnlyDirty=*/false);
}
