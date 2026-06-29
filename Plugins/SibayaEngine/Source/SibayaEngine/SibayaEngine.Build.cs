using UnrealBuildTool;

public class SibayaEngine : ModuleRules
{
    public SibayaEngine(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "SimulationBusPlugin"
        });
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Projects"
        });
    }
}
