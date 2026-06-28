using UnrealBuildTool;

public class LocomotionPhysicsPlugin : ModuleRules
{
    public LocomotionPhysicsPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[]
        {
            ModuleDirectory + "/Public"
        });

        PrivateIncludePaths.AddRange(new string[]
        {
            ModuleDirectory + "/Private"
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PhysicsCore",
            "Chaos"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "SimulationBusPlugin"
        });
    }
}
