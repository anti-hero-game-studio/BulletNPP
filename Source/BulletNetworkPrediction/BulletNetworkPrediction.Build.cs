// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BulletNetworkPrediction : ModuleRules
	{
		public BulletNetworkPrediction(ReadOnlyTargetRules Target) : base(Target)
		{
			OptimizeCode = CodeOptimization.InShippingBuildsOnly;
			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"NetCore",
                    "Engine",
                    "RenderCore",
					"PhysicsCore",
					"Chaos",
					"TraceLog",
					"Bullet"
				}
				);

            // Only needed for the PIE delegate in FBulletNetworkPredictionModule::StartupModule
            if (Target.Type == TargetType.Editor) {
                PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "UnrealEd",
                });
            }

			SetupIrisSupport(Target);
		}
	}
}
