// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/Modes/BulletFlyingMode.h"
#include "MoveLibrary/BulletAirMovementUtils.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"
#include "MoveLibrary/BulletGroundMovementUtils.h"
#include "MoveLibrary/BulletMovementUtils.h"
#include "BulletMoverComponent.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletFlyingMode)


UBulletFlyingMode::UBulletFlyingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(UBulletCommonLegacyMovementSettings::StaticClass());
	
	GameplayTags.AddTag(BulletMover_IsInAir);
	GameplayTags.AddTag(BulletMover_IsFlying);
}

void UBulletFlyingMode::GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const
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

void UBulletFlyingMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
	UBulletMoverComponent* MoverComp = GetMoverComponent();
	const FBulletMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	FBulletProposedMove ProposedMove = Params.ProposedMove;

	if (!UpdatedComponent)
	{
		return;
	}

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
	const bool bIsOrientationChanging = !StartingOrient.Equals(TargetOrient);
	
	FVector MoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;

	FQuat TargetOrientQuat = TargetOrient.Quaternion();
	if (CommonLegacySettings->bShouldRemainVertical)
	{
		TargetOrientQuat = FRotationMatrix::MakeFromZX(MoverComp->GetUpDirection(), TargetOrientQuat.GetForwardVector()).ToQuat();
	}

	FHitResult Hit(1.f);

	if (!MoveDelta.IsNearlyZero() || bIsOrientationChanging)
	{
		UBulletMovementUtils::TrySafeMoveUpdatedComponent(Params.MovingComps, MoveDelta, TargetOrientQuat, true, Hit, ETeleportType::None, MoveRecord);
	}

	if (Hit.IsValidBlockingHit())
	{
		FBulletMoverOnImpactParams ImpactParams(DefaultModeNames::Flying, Hit, MoveDelta);
		MoverComp->HandleImpact(ImpactParams);

		// Try to slide the remaining distance along the surface.
		UBulletMovementUtils::TryMoveToSlideAlongSurface(FBulletMovingComponentSet(MoverComp), MoveDelta, 1.f - Hit.Time, TargetOrientQuat, Hit.Normal, Hit, true, MoveRecord);
	}

	if (bRespectDistanceOverWalkableSurfaces)
	{
		// If we are very close to a walkable surface, make sure we maintain a small gap over it
		FBulletFloorCheckResult FloorUnderActor;
		UBulletFloorQueryUtils::FindFloor(Params.MovingComps, CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->bUseFlatBaseForFloorChecks, UpdatedComponent->GetComponentLocation(), OUT FloorUnderActor);

		if (FloorUnderActor.IsWalkableFloor())
		{
			UBulletGroundMovementUtils::TryMoveToKeepMinHeightAboveFloor(MoverComp, FloorUnderActor, CommonLegacySettings->MaxWalkSlopeCosine, MoveRecord);
		}
	}

	CaptureFinalState(UpdatedComponent, MoveRecord, *StartingSyncState, ProposedMove.AngularVelocityDegrees, OutputSyncState, DeltaSeconds);
}

// TODO: replace this function with simply looking at/collapsing the MovementRecord
void UBulletFlyingMode::CaptureFinalState(USceneComponent* UpdatedComponent, FBulletMovementRecord& Record, const FBulletMoverDefaultSyncState& StartSyncState, const FVector& AngularVelocityDegrees, FBulletMoverDefaultSyncState& OutputSyncState, const float DeltaSeconds) const
{
	const FVector FinalLocation = UpdatedComponent->GetComponentLocation();
	const FVector FinalVelocity = Record.GetRelevantVelocity();
	
	// TODO: Update Main/large movement record with substeps from our local record

	OutputSyncState.SetTransforms_WorldSpace(FinalLocation,
											  UpdatedComponent->GetComponentRotation(),
											  FinalVelocity,
											  AngularVelocityDegrees,
											  nullptr); // no movement base

	UpdatedComponent->ComponentVelocity = FinalVelocity;
}

void UBulletFlyingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UBulletCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings, TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}


void UBulletFlyingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}
