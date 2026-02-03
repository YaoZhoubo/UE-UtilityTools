// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UtilityTools : ModuleRules
{
	public UtilityTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;	
		
		PrivateDependencyModuleNames.AddRange(
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
				"UMG",
				"CoreUObject",
				"Slate",
				"SlateCore",
                "CustomRenderer",
                "InputCore",
                "UtilityRenderer"
            }
			);
	}
}
