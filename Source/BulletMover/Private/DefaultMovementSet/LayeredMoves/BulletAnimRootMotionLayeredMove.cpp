// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/LayeredMoves/BulletAnimRootMotionLayeredMove.h"
#include "BulletMoverComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoveLibrary/BulletMovementUtils.h"
#include "BulletMoverTypes.h"
#include "BulletMoverLog.h"
#include "MotionWarpingComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletAnimRootMotionLayeredMove)

#if !UE_BUILD_SHIPPING
FAutoConsoleVariable CVarLogAnimRootMotionSteps(
	TEXT("bullet.mover.debug.LogAnimRootMotionSteps"),
	false,
	TEXT("Whether to log detailed information about anim root motion layered moves. 0: Disable, 1: Enable"),
	ECVF_Cheat);
#endif	// !UE_BUILD_SHIPPING



FBulletLayeredMove_AnimRootMotion::FBulletLayeredMove_AnimRootMotion()
{
	DurationMs = 0.f;
	MixMode = EBulletMoveMixMode::OverrideAll;

}

bool FBulletLayeredMove_AnimRootMotion::GenerateMove(const FBulletMoverTickStartData& SimState, const FBulletMoverTimeStep& TimeStep, const UBulletMoverComponent* MoverComp, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutProposedMove)
{
	// Stop this move if the montage is no longer playing on the mesh
	if (!TimeStep.bIsResimulating)
	{
		bool bIsMontageStillPlaying = false;

		if (const USkeletalMeshComponent* MeshComp = Cast<USkeletalMeshComponent>(MoverComp->GetPrimaryVisualComponent()))
		{
			if (const UAnimInstance* MeshAnimInstance = MeshComp->GetAnimInstance())
			{
				bIsMontageStillPlaying = MontageState.Montage && MeshAnimInstance->Montage_IsPlaying(MontageState.Montage);
			}
		}

		if (!bIsMontageStillPlaying)
		{
			DurationMs = 0.f;
			return false;
		}
	}

	const float DeltaSeconds = TimeStep.StepMs / 1000.f;

	const FBulletMoverDefaultSyncState* SyncState = SimState.SyncState.SyncStateCollection.FindDataByType<FBulletMoverDefaultSyncState>();

	if (SyncState == nullptr)
	{
		return false;
	}

	// First pass simply samples based on the duration. For long animations, this has the potential to diverge.
	// Future improvements could include:
	//     - speeding up or slowing down slightly to match the associated montage instance
	//     - detecting if the montage instance is interrupted and attempting to interrupt and scheduling this move to end at the same sim time
	
	// Note that Montage 'position' equates to seconds when PlayRate is 1
	const double SecondsSinceMontageStarted = (TimeStep.BaseSimTimeMs - StartSimTimeMs) / 1000.0;
	const double ScaledSecondsSinceMontageStarted = SecondsSinceMontageStarted * MontageState.PlayRate;

	const float ExtractionStartPosition = MontageState.StartingMontagePosition + ScaledSecondsSinceMontageStarted;
	const float ExtractionEndPosition   = ExtractionStartPosition + (DeltaSeconds * MontageState.PlayRate);

	// Read the local transform directly from the montage
	const FTransform LocalRootMotion = MontageState.Montage ? UMotionWarpingUtilities::ExtractRootMotionFromAnimation(MontageState.Montage, ExtractionStartPosition, ExtractionEndPosition) : FTransform::Identity;

	FMotionWarpingUpdateContext WarpingContext;
	WarpingContext.Animation = MontageState.Montage;
	WarpingContext.CurrentPosition = ExtractionEndPosition;
	WarpingContext.PreviousPosition = ExtractionStartPosition;
	WarpingContext.PlayRate = MontageState.PlayRate;
	WarpingContext.Weight = 1.f;

	// Note that we're forcing the use of the sync state's actor transform data. This is necessary when the movement simulation 
	// is running ahead of the actor's visual representation and may be rotated differently, such as in an async physics sim.
	const FTransform SimActorTransform = FTransform(SyncState->GetOrientation_WorldSpace().Quaternion(), SyncState->GetLocation_WorldSpace());
	const FTransform WorldSpaceRootMotion = MoverComp->ConvertLocalRootMotionToWorld(LocalRootMotion, DeltaSeconds, &SimActorTransform, &WarpingContext);
	
	OutProposedMove = FBulletProposedMove();
	OutProposedMove.MixMode = MixMode;

	// Convert the transform into linear and angular velocities
	if (DeltaSeconds > UE_KINDA_SMALL_NUMBER)
	{
		OutProposedMove.LinearVelocity    = WorldSpaceRootMotion.GetTranslation() / DeltaSeconds;
		OutProposedMove.AngularVelocityDegrees   = FMath::RadiansToDegrees(WorldSpaceRootMotion.GetRotation().ToRotationVector() / DeltaSeconds);
	}

	MontageState.CurrentPosition = ExtractionStartPosition;

#if !UE_BUILD_SHIPPING
	UE_CLOG(CVarLogAnimRootMotionSteps->GetBool(), LogBulletMover, Log, TEXT("AnimRootMotion. SimF %i (dt %.3f) Range [%.3f, %.3f] => LocalT: %s (WST: %s)  Vel: %.3f"),
	        TimeStep.ServerFrame, DeltaSeconds, ExtractionStartPosition, ExtractionEndPosition, 
	        *LocalRootMotion.GetTranslation().ToString(), *WorldSpaceRootMotion.GetTranslation().ToString(), OutProposedMove.LinearVelocity.Length());
#endif // !UE_BUILD_SHIPPING

	return true;
}

FBulletLayeredMoveBase* FBulletLayeredMove_AnimRootMotion::Clone() const
{
	FBulletLayeredMove_AnimRootMotion* CopyPtr = new FBulletLayeredMove_AnimRootMotion(*this);
	return CopyPtr;
}

void FBulletLayeredMove_AnimRootMotion::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	MontageState.NetSerialize(Ar);
}

UScriptStruct* FBulletLayeredMove_AnimRootMotion::GetScriptStruct() const
{
	return FBulletLayeredMove_AnimRootMotion::StaticStruct();
}

FString FBulletLayeredMove_AnimRootMotion::ToSimpleString() const
{
	return FString::Printf(TEXT("AnimRootMotion"));
}

void FBulletLayeredMove_AnimRootMotion::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}

FBulletMoverAnimMontageState FBulletLayeredMove_AnimRootMotion::GetMontageState() const
{
	return MontageState;
}