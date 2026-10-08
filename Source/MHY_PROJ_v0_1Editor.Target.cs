using UnrealBuildTool;
using System.Collections.Generic;

public class MHY_PROJ_v0_1EditorTarget : TargetRules
{
	public MHY_PROJ_v0_1EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("MHY_PROJ_v0_1");
	}
}
