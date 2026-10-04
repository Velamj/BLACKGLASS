using UnrealBuildTool;
public class Blackglass : ModuleRules
{
 public Blackglass(ReadOnlyTargetRules Target) : base(Target)
 {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  CppStandard = CppStandardVersion.Cpp20;
  PublicDependencyModuleNames.AddRange(new[] {
   "Core", "CoreUObject", "Engine", "InputCore",
   "NavigationSystem", "AIModule", "UMG", "Slate", "SlateCore"
  });
 }
}
