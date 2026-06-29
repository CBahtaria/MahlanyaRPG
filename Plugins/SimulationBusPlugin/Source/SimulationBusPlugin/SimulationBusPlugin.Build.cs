using UnrealBuildTool;

public class SimulationBusPlugin : ModuleRules
{
    public SimulationBusPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
        });

        // No other plugin dependencies — this is the root of the plugin graph.
    }
}
