using UnrealBuildTool;

public class MicroclimateEngine : ModuleRules
{
    public MicroclimateEngine(ReadOnlyTargetRules Target) : base(Target)
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
