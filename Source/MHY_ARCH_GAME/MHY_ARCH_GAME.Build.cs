// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MHY_ARCH_GAME : ModuleRules
{
	public MHY_ARCH_GAME(ReadOnlyTargetRules Target) : base(Target)
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
			"MotionWarping",
			"PhysicsCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"MHY_ARCH_GAME",
			"MHY_ARCH_GAME/Climb",
			"MHY_ARCH_GAME/MovementAudio",
			"MHY_ARCH_GAME/LightReveal",
			"MHY_ARCH_GAME/LightReveal/Interfaces",
			"MHY_ARCH_GAME/LiquidLight",
			"MHY_ARCH_GAME/OverlapPassage",
			"MHY_ARCH_GAME/StructureInteraction",
			"MHY_ARCH_GAME/StructureInteraction/Interfaces",
			"MHY_ARCH_GAME/TimeShift",
			"MHY_ARCH_GAME/TimeShift/Interfaces",
			"MHY_ARCH_GAME/Variant_Platforming",
			"MHY_ARCH_GAME/Variant_Platforming/Animation",
			"MHY_ARCH_GAME/Variant_Combat",
			"MHY_ARCH_GAME/Variant_Combat/AI",
			"MHY_ARCH_GAME/Variant_Combat/Animation",
			"MHY_ARCH_GAME/Variant_Combat/Gameplay",
			"MHY_ARCH_GAME/Variant_Combat/Interfaces",
			"MHY_ARCH_GAME/Variant_Combat/UI",
			"MHY_ARCH_GAME/Variant_SideScrolling",
			"MHY_ARCH_GAME/Variant_SideScrolling/AI",
			"MHY_ARCH_GAME/Variant_SideScrolling/Gameplay",
			"MHY_ARCH_GAME/Variant_SideScrolling/Interfaces",
			"MHY_ARCH_GAME/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
