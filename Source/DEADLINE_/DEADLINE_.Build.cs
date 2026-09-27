// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DEADLINE_ : ModuleRules
{
	public DEADLINE_(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// UE 5.8 defaults bLegacyPublicIncludePaths to false, so subfolders are
		// not added automatically. Register the module root once so headers can
		// be included as "Core/Foo.h", "Economy/Bar.h" and so on.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"DeveloperSettings",
			// Month 2: the market screen. UUserWidget bases live in C++ and
			// bind their children by name, so no Blueprint graph is needed.
			"UMG",
			"Slate",
			"SlateCore",
			// Month 5: NPCs walk the NavMesh (NPC/DeadlineNPCController).
			"AIModule",
			"NavigationSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
