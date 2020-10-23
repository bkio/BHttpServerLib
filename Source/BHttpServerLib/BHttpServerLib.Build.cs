/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

using UnrealBuildTool;
using System.IO;

public class BHttpServerLib : ModuleRules
{
    public BHttpServerLib(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core"
			});

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine"
            });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            bEnableExceptions = true;

            PrivateIncludePaths.AddRange(
                new string[] {
                    "BHttpserverLib/Private",
                    "BHttpserverLib/Private/Win64",
                    "BHttpserverLib/Private/Win64/include",
                    });

            PublicIncludePaths.AddRange(
                new string[] {
                    Path.Combine(ModuleDirectory, "Private"),
                    Path.Combine(ModuleDirectory, "Private/Win64"),
                    Path.Combine(ModuleDirectory, "Private/Win64/include"),
                });
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            bEnableExceptions = true;

            PrivateIncludePaths.AddRange(
                new string[] {
                    "BHttpserverLib/Private",
                    "BHttpserverLib/Private/Mac",
                    "BHttpserverLib/Private/Mac/include",
                    });

            PublicIncludePaths.AddRange(
                new string[] {
                    Path.Combine(ModuleDirectory, "Private"),
                    Path.Combine(ModuleDirectory, "Private/Mac"),
                    Path.Combine(ModuleDirectory, "Private/Mac/include"),
                });
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            bEnableExceptions = true;

            PrivateIncludePaths.AddRange(
                new string[] {
                    "BHttpserverLib/Private",
                    "BHttpserverLib/Private/Linux",
                    "BHttpserverLib/Private/Linux/include",
                    });

            PublicIncludePaths.AddRange(
                new string[] {
                    Path.Combine(ModuleDirectory, "Private"),
                    Path.Combine(ModuleDirectory, "Private/Linux"),
                    Path.Combine(ModuleDirectory, "Private/Linux/include"),
                });
        }
    }
}