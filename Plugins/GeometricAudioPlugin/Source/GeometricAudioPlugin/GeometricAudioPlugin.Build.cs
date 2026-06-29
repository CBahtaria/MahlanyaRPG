using UnrealBuildTool;

public class GeometricAudioPlugin : ModuleRules
{
    public GeometricAudioPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "SimulationBusPlugin", "AudioMixer"
        });
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Projects"
        });
    }
}
