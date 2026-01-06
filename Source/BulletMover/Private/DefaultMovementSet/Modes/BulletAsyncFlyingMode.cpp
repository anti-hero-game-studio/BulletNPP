// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/Modes/BulletAsyncFlyingMode.h"
#include "MoveLibrary/BulletAirMovementUtils.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"
#include "MoveLibrary/BulletGroundMovementUtils.h"
#include "MoveLibrary/BulletMovementUtils.h"
#include "MoveLibrary/BulletAsyncMovementUtils.h"
#include "BulletMoverComponent.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletAsyncFlyingMode)


UBulletAsyncFlyingMode::UBulletAsyncFlyingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(UBulletCommonLegacyMovementSettings::StaticClass());
	
	GameplayTags.AddTag(BulletMover_IsInAir);
	GameplayTags.AddTag(BulletMover_IsFlying);
}

void UBulletAsyncFlyingMode::GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const
{
	const UBulletMoverComponent* MoverComp = GetMoverComponent();
	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FBulletMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	FBulletFreeMoveParams Params;
	if (CharacterInputs)
	{
		Params.MoveInputType = CharacterInputs->GetMoveInputType();
		const bool bMaintainInputMagnitude = true;
		Params.MoveInput = UBulletPlanarConstraintUtils::ConstrainDirectionToPlane(MoverComp->GetPlanarConstraint(), CharacterInputs->GetMoveInput_WorldSpace(), bMaintainInputMagnitude);
	}
	else
	{
		Params.MoveInputType = EBulletMoveInputType::None;
		Params.MoveInput = FVector::ZeroVector;
	}

	FRotator IntendedOrientation_WorldSpace;
	// If there's no intent from input to change orientation, use the current orientation
	if (!CharacterInputs || CharacterInputs->OrientationIntent.IsNearlyZero())
	{
		IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace();
	}
	else
	{
		IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
	}

	IntendedOrientation_WorldSpace = UBulletMovementUtils::ApplyGravityToOrientationIntent(IntendedOrientation_WorldSpace, MoverComp->GetWorldToGravityTransform(), CommonLegacySettings->bShouldRemainVertical);
	
	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = StartingSyncState->GetVelocity_WorldSpace();
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.TurningRate = CommonLegacySettings->TurningRate;
	Params.TurningBoost = CommonLegacySettings->TurningBoost;
	Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
	Params.Acceleration = CommonLegacySettings->Acceleration;
	Params.Deceleration = CommonLegacySettings->Deceleration;
	Params.DeltaSeconds = DeltaSeconds;
	Params.WorldToGravityQuat = MoverComp->GetWorldToGravityTransform();
	Params.bUseAccelerationForVelocityMove = CommonLegacySettings->bUseAccelerationForVelocityMove;
	
	OutProposedMove = UBulletAirMovementUtils::ComputeControlledFreeMove(Params);
}

void UBulletAsyncFlyingMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
	const UBulletMoverComponent* MoverComp = GetMoverComponent();
	const FBulletMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	const FBulletProposedMove& ProposedMove = Params.ProposedMove;

	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FBulletMoverDefaultSyncState>();
	check(StartingSyncState);

	FBulletMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FBulletMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	FBulletMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);

	UBulletMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();
	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);	// flying = no valid floor
	SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

	OutputSyncState.MoveDirectionIntent = (ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector);

	// Use the orientation intent directly. If no intent is provided, use last frame's orientation. Note that we are assuming rotation changes can't fail. 
	const FRotator StartingOrient = StartingSyncState->GetOrientation_WorldSpace();
	const FRotator TargetOrient = UBulletMovementUtils::ApplyAngularVelocityToRotator(StartingOrient, ProposedMove.AngularVelocityDegrees, DeltaSeconds);
	
	const FVector StartLocation = StartingSyncState->GetLocation_WorldSpace();
	const FVector TargetLocation = StartLocation + (ProposedMove.LinearVelocity * DeltaSeconds);

	const FQuat StartRotation = StartingOrient.Quaternion();
	FQuat TargetRotation = TargetOrient.Quaternion();
	if (CommonLegacySettings->bShouldRemainVertical)
	{
		TargetRotation = FRotationMatrix::MakeFromZX(MoverComp->GetUpDirection(), TargetRotation.GetForwardVector()).ToQuat();
	}

	FHitResult SweepHit(1.f);
	FBulletMovementRecord SweepRecord;
	SweepRecord.SetDeltaSeconds(DeltaSeconds);
	FVector LocationInProgress = StartLocation;
	FQuat   RotationInProgress = StartRotation;

	const bool bWouldMove = UBulletAsyncMovementUtils::TestDepenetratingMove(Params.MovingComps, StartLocation, TargetLocation, StartRotation, TargetRotation, /* bShouldSweep */ true, OUT SweepHit, SweepRecord);

	LocationInProgress = StartLocation + ((TargetLocation - StartLocation) * SweepHit.Time);
	RotationInProgress = FQuat::Slerp(StartRotation, TargetRotation, SweepHit.Time);

	if (SweepHit.IsValidBlockingHit())
	{
		const float PctOfTimeUsedForSliding = UBulletAsyncMovementUtils::TestSlidingMoveAlongHitSurface(Params.MovingComps, TargetLocation - StartLocation, LocationInProgress, TargetRotation, SweepHit, SweepRecord);

		if (PctOfTimeUsedForSliding > 0.f)
		{
			LocationInProgress = SweepHit.TraceStart + ((SweepHit.TraceEnd - SweepHit.TraceStart) * PctOfTimeUsedForSliding);
			RotationInProgress = FQuat::Slerp(RotationInProgress, TargetRotation, PctOfTimeUsedForSliding);
		}
	}

	if (bRespectDistanceOverWalkableSurfaces)
	{
		// If we are very close to a walkable surface, make sure we maintain a small gap over it
		FBulletFloorCheckResult FloorUnderActor;
		UBulletFloorQueryUtils::FindFloor(Params.MovingComps, CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->bUseFlatBaseForFloorChecks, LocationInProgress, FloorUnderActor);

		if (FloorUnderActor.IsWalkableFloor())
		{
			LocationInProgress = UBulletGroundMovementUtils::TestMoveToKeepMinHeightAboveFloor(Params.MovingComps, LocationInProgress, RotationInProgress, CommonLegacySettings->MaxWalkSlopeCosine, IN OUT FloorUnderActor, MoveRecord);
		}
	}

	OutputSyncState.SetTransforms_WorldSpace(
		LocationInProgress,
		RotationInProgress.Rotator(),
		SweepRecord.GetRelevantVelocity(),
		ProposedMove.AngularVelocityDegrees,
		nullptr); // no movement base
}


void UBulletAsyncFlyingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UBulletCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings, TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}


void UBulletAsyncFlyingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}