// Copyright DEADLINE. All Rights Reserved.

using UnrealBuildTool;

/// Editor-only tooling. Nothing in here ships: it exists so that layout work
/// which would otherwise be hundreds of manual clicks in the UMG Designer can
/// be written down, run again, and reviewed in a diff.
public class DEADLINE_Editor : ModuleRules
{
	public DEADLINE_Editor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory + "/Public");

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"UMGEditor",
			"Kismet",
			"Slate",
			"SlateCore",
			"DEADLINE_"
		});
	}
}
