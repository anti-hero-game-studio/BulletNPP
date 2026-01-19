// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BulletMoverEditor : ModuleRules
{
	public BulletMoverEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		OptimizeCode = CodeOptimization.Never;
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"BulletMover",
				"BlueprintGraph",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			});

	}
}