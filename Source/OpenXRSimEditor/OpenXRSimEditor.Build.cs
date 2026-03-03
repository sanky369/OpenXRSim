// OpenXRSim: Editor module build rules and dependencies.
// Easy guide: read this file first when you need this behavior.

using UnrealBuildTool;

public class OpenXRSimEditor : ModuleRules
{
    public OpenXRSimEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd",
            "LevelEditor",
            "OpenXRSim"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Projects"
        });
    }
}
