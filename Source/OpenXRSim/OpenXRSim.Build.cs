// OpenXRSim: Runtime module build rules and dependencies.
// Easy guide: read this file first when you need this behavior.

using UnrealBuildTool;

public class OpenXRSim : ModuleRules
{
    public OpenXRSim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "HeadMountedDisplay",
            "XRBase",
            "Json",
            "JsonUtilities",
            "DeveloperSettings",
            "Sockets",
            "Networking"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Projects" // IPluginManager
        });

        if (Target.Type != TargetType.Server)
        {
            PrivateDependencyModuleNames.AddRange(new[]
            {
                "Slate",
                "SlateCore",
                "ApplicationCore"
            });
        }

        // For Windows-specific polling helpers (GetAsyncKeyState / XInput optional).
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("WindowsPlatformFeatures");

            // Optional XInput linkage (safe even if unused on some setups).
            PublicSystemLibraries.Add("xinput9_1_0.lib");
        }
    }
}
