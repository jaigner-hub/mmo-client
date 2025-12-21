// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class mmoclient : ModuleRules
{
	public mmoclient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"WebSockets",
			"Json",
			"JsonUtilities"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"mmoclient",
			"mmoclient/Variant_Platforming",
			"mmoclient/Variant_Platforming/Animation",
			"mmoclient/Variant_Combat",
			"mmoclient/Variant_Combat/AI",
			"mmoclient/Variant_Combat/Animation",
			"mmoclient/Variant_Combat/Gameplay",
			"mmoclient/Variant_Combat/Interfaces",
			"mmoclient/Variant_Combat/UI",
			"mmoclient/Variant_Combat/Network",
			"mmoclient/Variant_SideScrolling",
			"mmoclient/Variant_SideScrolling/AI",
			"mmoclient/Variant_SideScrolling/Gameplay",
			"mmoclient/Variant_SideScrolling/Interfaces",
			"mmoclient/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
