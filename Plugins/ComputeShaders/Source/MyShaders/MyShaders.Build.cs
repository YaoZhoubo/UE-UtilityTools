using UnrealBuildTool;

public class MyShaders : ModuleRules

{

    public MyShaders(ReadOnlyTargetRules Target) : base(Target)

    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateIncludePaths.AddRange(new string[]
        {
            "MyShaders/Private"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Renderer",
            "RenderCore",
            "RHI",
            "Projects"
        });

    }

}