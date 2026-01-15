// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BulletMover : ModuleRules
{
	public BulletMover(ReadOnlyTargetRules Target) : base(Target)
	{
		OptimizeCode = CodeOptimization.InShippingBuildsOnly;

		// TODO: find a better way to manage optional dependencies, such as Water and PoseSearch. This includes module dependencies here, as well as .uplugin dependencies.

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"NetCore",
				"InputCore",
				"BulletNetworkPrediction",
				"Bullet",
				"AnimGraphRuntime",
				"MotionWarping",
				"Water",
				"GameplayTags",
				"NavigationSystem",
				"Bullet",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Chaos",
				"CoreUObject",
				"Engine",
				"PhysicsCore",
				"DeveloperSettings",
				"PoseSearch",
				"Bullet",
			}
			);

		if (IsChaosVisualDebuggerSupported(Target))
		{
			//PublicDependencyModuleNames.Add("BulletMoverCVDData");
		}
		//SetupModuleChaosVisualDebuggerSupport(Target);

		SetupGameplayDebuggerSupport(Target);

		//SetupModulePhysicsSupport(Target);
	}
}
