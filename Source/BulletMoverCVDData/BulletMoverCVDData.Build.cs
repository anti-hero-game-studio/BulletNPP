// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BulletMoverCVDData : ModuleRules
{
    public BulletMoverCVDData(ReadOnlyTargetRules Target) : base(Target)
    {
	    OptimizeCode = CodeOptimization.InShippingBuildsOnly;
	    
	    PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ChaosVDRuntime",
			}
		);
	}
}