using UnrealBuildTool;
using System.Collections.Generic;
public class BlackglassEditorTarget : TargetRules
{
 public BlackglassEditorTarget(TargetInfo Target) : base(Target)
 {
  Type = TargetType.Editor;
  DefaultBuildSettings = BuildSettingsVersion.Latest;
  IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
  ExtraModuleNames.Add("Blackglass");
 }
}
