// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UtilityTools : ModuleRules
{
	public UtilityTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateIncludePaths.AddRange(new string[]
            {
                
            });

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "Engine",
                "AssetRegistry",
                "Renderer",
				"RenderCore",
				"RHI",
				"Projects",
				"GameplayTags",
				"CommonUI",
				"UMG"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Slate",
				"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
