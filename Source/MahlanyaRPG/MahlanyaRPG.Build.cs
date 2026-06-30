// Copyright Charles Bartaria. All Rights Reserved.

using UnrealBuildTool;

public class MahlanyaRPG : ModuleRules
{
    public MahlanyaRPG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "NetCore", "DeveloperSettings", "TraceLog", "RHI",
            "GameplayAbilities", "UMG",
            "EnhancedInput",
            "LocomotionPhysicsPlugin"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "DeviceProfileServices", "Compression", "Slate", "SlateCore",
            "SimulationBusPlugin", "EmergentNarrativePlugin"
        });

        // Insights tracing only in non-Shipping builds
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
            PrivateDefinitions.Add("MAHLANYA_ENABLE_INSIGHTS=1");
        else
            PrivateDefinitions.Add("MAHLANYA_ENABLE_INSIGHTS=0");
    }
}
