// Copyright Charles Bartaria. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MahlanyaPipelinePlugin : ModuleRules
{
    public MahlanyaPipelinePlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModuleDirectory, "Public")
        });

        PrivateIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModuleDirectory, "Private")
        });

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "DesktopPlatform"
        });

        // Default path for libmahlanya_compute.so relative to the plugin root.
        // Can be overridden at runtime via the MAHLANYA_ZIG_LIB_PATH environment variable.
        string ZigLibPath = Path.Combine(PluginDirectory, "zig-out", "lib", "libmahlanya_compute.so");
        PublicDefinitions.Add(string.Format("MAHLANYA_ZIG_LIB_PATH=\"{0}\"", ZigLibPath.Replace("\\", "/")));
    }
}
