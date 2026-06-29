// Copyright Charles Bartaria. All Rights Reserved.

using UnrealBuildTool;

public class EconomySimulatorPlugin : ModuleRules
{
    public EconomySimulatorPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "SimulationBusPlugin"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
        });
    }
}
