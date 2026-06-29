using UnrealBuildTool;
public class EcologySimulatorPlugin : ModuleRules
{
    public EcologySimulatorPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsageMode = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "Json", "JsonUtilities", "SimulationBusPlugin"
        });
    }
}
