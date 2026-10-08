using UnrealBuildTool;
using System.Collections.Generic;

public class MHY_PROJ_v0_1Target : TargetRules
{
	public MHY_PROJ_v0_1Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("MHY_PROJ_v0_1");
	}
}
