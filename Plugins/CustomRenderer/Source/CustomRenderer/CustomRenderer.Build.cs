// Copyright(c) 2025 YaoZhoubo. All rights reserved.

using UnrealBuildTool;

public class CustomRenderer : ModuleRules
{
	public CustomRenderer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
                "Renderer",
				"RenderCore",
				"RHI",
				"Projects"
            }
			);
	}
}
