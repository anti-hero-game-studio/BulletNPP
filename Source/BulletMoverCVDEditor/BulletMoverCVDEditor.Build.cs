// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BulletMoverCVDEditor : ModuleRules
{
    public BulletMoverCVDEditor(ReadOnlyTargetRules Target) : base(Target)
    {
	    OptimizeCode = CodeOptimization.InShippingBuildsOnly;
	    
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			});

		PrivateDependencyModuleNames.AddRange(
		    new string[]
		    {
				"ApplicationCore",
				"ChaosVD",
				"ChaosVDData",
				"ChaosVDRuntime",
				"CoreUObject",
				"EditorWidgets",
				"Engine",
				"BulletMover",
				"BulletMoverCVDData",
				"Projects",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"TraceServices",
				"TypedElementFramework",
				"UnrealEd",
			}
		);
        
        SetupModulePhysicsSupport(Target);

		SetupModuleChaosVisualDebuggerSupport(Target);
	}
}