using UnrealBuildTool;

public class UtilityRenderer : ModuleRules
{
	public UtilityRenderer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseRTTI = true;

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
