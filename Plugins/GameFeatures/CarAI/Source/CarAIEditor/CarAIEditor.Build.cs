// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class CarAIEditor : ModuleRules
{
	public CarAIEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] 
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Blutility",
			"UMG",
			"UMGEditor",
			"EditorSubsystem",
		});
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"EditorFramework",
				"ToolMenus",
				"LevelEditor",
				"InputCore",
				"Projects",
				"Blutility",
				"Framework",
				"Cars",
				"JSON",
				"CarAIRuntime",
                "ChaosVehicles",
            });
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
			});
	}
}
