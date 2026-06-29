using UnrealBuildTool;
public class CulturalProtocolPlugin : ModuleRules
{
    public CulturalProtocolPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsageMode = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "Json", "JsonUtilities"
        });
    }
}
