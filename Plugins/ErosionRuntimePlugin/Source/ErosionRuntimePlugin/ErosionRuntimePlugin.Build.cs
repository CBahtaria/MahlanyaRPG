// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

using UnrealBuildTool;

public class ErosionRuntimePlugin : ModuleRules
{
    public ErosionRuntimePlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "RHI", "RenderCore", "Renderer",
            "SimulationBusPlugin",
            "Projects",  // IPluginManager for shader path mapping
        });

        // Shader source directory registered in StartupModule via AddShaderSourceDirectoryMapping
        PublicIncludePaths.Add(ModuleDirectory + "/Public");
    }
}
