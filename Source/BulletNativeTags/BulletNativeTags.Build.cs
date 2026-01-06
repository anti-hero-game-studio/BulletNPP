using UnrealBuildTool;

public class BulletNativeTags : ModuleRules
{
    public BulletNativeTags(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        OptimizeCode = CodeOptimization.InShippingBuildsOnly;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "GameplayTags"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "GameplayTags"
            }
        );
    }
}