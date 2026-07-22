// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

using UnrealBuildTool;

public class EswatiniGeophysics : ModuleRules
{
    public EswatiniGeophysics(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "Json", "JsonUtilities", "Spline", "SimulationBusPlugin"
        });

        PublicIncludePaths.Add(ModuleDirectory + "/Public");
    }
}
