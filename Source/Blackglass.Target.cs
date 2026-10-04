using UnrealBuildTool;
using System.Collections.Generic;
public class BlackglassTarget : TargetRules
{
 public BlackglassTarget(TargetInfo Target) : base(Target)
 {
  Type = TargetType.Game;
  DefaultBuildSettings = BuildSettingsVersion.Latest;
  IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
  ExtraModuleNames.Add("Blackglass");
 }
}
