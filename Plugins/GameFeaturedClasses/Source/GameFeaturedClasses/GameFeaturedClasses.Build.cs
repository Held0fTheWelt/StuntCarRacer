using UnrealBuildTool;

public class GameFeaturedClasses : ModuleRules
{
    public GameFeaturedClasses(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // Normalerweise keine Extra-Pfade nötig, wenn alles im Standard-Schema liegt
        PublicIncludePaths.AddRange(new string[] { });
        PrivateIncludePaths.AddRange(new string[] { });

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "ChaosVehicles",
                "PhysicsCore",
                "ModularGameplay",
                "GameFeatures",
                "AIModule",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore",
                "LoggingMacros",
            }
        );

        DynamicallyLoadedModuleNames.AddRange(new string[] { });
    }
}
