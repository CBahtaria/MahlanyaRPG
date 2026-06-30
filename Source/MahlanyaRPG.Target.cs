// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MahlanyaRPGTarget : TargetRules
{
	public MahlanyaRPGTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("MahlanyaRPG");
	}
}
