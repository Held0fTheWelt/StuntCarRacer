// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StuntCarRacer : ModuleRules
{
	public StuntCarRacer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"AIModule",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
            "GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PublicIncludePaths.AddRange(new string[] {
			"StuntCarRacer",
			"StuntCarRacer/SportsCar",
			"StuntCarRacer/OffroadCar",
			"StuntCarRacer/Variant_Offroad",
			"StuntCarRacer/Variant_TimeTrial",
			"StuntCarRacer/Variant_TimeTrial/UI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
