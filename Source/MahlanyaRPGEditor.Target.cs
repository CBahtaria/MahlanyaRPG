// Copyright Charles Bartaria / BRT Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MahlanyaRPGEditorTarget : TargetRules
{
	public MahlanyaRPGEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("MahlanyaRPG");
	}
}
