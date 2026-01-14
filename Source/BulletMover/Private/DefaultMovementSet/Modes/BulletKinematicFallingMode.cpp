// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/Modes/BulletKinematicFallingMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "BulletMoverComponent.h"
#include "MoveLibrary/BulletMovementUtils.h"
#include "MoveLibrary/BulletBasedMovementUtils.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"
#include "MoveLibrary/BulletGroundMovementUtils.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"
#include "MoveLibrary/BulletAirMovementUtils.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletKinematicFallingMode)

UBulletKinematicFallingMode::UBulletKinematicFallingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCancelVerticalSpeedOnLanding(true)
	, AirControlPercentage(0.4f)
	, FallingDeceleration(200.0f)
	, FallingLateralFriction(0.0f)
	, OverTerminalSpeedFallingDeceleration(800.0f)
	, TerminalMovementPlaneSpeed(1500.0f)
	, bShouldClampTerminalVerticalSpeed(true)
	, VerticalFallingDeceleration(4000.0f)
	, TerminalVerticalSpeed(2000.0f)
{
	SharedSettingsClasses.Add(UBulletCommonLegacyMovementSettings::StaticClass());

	GameplayTags.AddTag(BulletMover_IsInAir);
	GameplayTags.AddTag(BulletMover_IsFalling);
	GameplayTags.AddTag(BulletMover_SkipVerticalAnimRootMotion);	// allows combination of gravity falling and root motion
}


void UBulletKinematicFallingMode::GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const
{
	const UBulletMoverComponent* MoverComp = GetMoverComponent();
	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletUpdatedMotionState* StartingSyncState = StartState.SyncState.Collection.FindDataByType<FBulletUpdatedMotionState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	FVector UpDirection = MoverComp->GetUpDirection();
	
	// We don't want velocity limits to take the falling velocity component into account, since it is handled 
	//   separately by the terminal velocity of the environment.
	const FVector StartVelocity = StartingSyncState->GetVelocity_WorldSpace();
	const FVector StartHorizontalVelocity =  FVector::VectorPlaneProject(StartVelocity, UpDirection);

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

	Params.MoveInput *= AirControlPercentage;
	// Don't care about up axis input since falling - if up input matters that should probably be a different movement mode
	Params.MoveInput = FVector::VectorPlaneProject(Params.MoveInput, UpDirection);
	
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
	Params.PriorVelocity = StartHorizontalVelocity;
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.DeltaSeconds = DeltaSeconds;
	Params.TurningRate = CommonLegacySettings->TurningRate;
	Params.TurningBoost = CommonLegacySettings->TurningBoost;
	Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
	Params.Acceleration = CommonLegacySettings->Acceleration;
	Params.Deceleration = FallingDeceleration;
	Params.WorldToGravityQuat = MoverComp->GetWorldToGravityTransform();
	Params.bUseAccelerationForVelocityMove = CommonLegacySettings->bUseAccelerationForVelocityMove;
	Params.Friction = FallingLateralFriction;

	// Check if any current velocity values are over our terminal velocity - if so limit the move input in that direction and apply OverTerminalVelocityFallingDeceleration
	if (Params.MoveInput.Dot(StartVelocity) > 0 && StartHorizontalVelocity.Size() >= TerminalMovementPlaneSpeed)
	{
		Params.Deceleration = OverTerminalSpeedFallingDeceleration;
	}
	
	UBulletMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();
	FBulletFloorCheckResult LastFloorResult;
	// limit our moveinput based on the floor we're on
	if (SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult))
	{
		if (LastFloorResult.HitResult.IsValidBlockingHit() && LastFloorResult.HitResult.Normal.Dot(UpDirection) > UE::BulletMoverUtils::VERTICAL_SLOPE_NORMAL_MAX_DOT && !LastFloorResult.IsWalkableFloor())
		{
			// If acceleration is into the wall, limit contribution.
			if (FVector::DotProduct(Params.MoveInput, LastFloorResult.HitResult.Normal) < 0.f)
			{
				// Allow movement parallel to the wall, but not into it because that may push us up.
				const FVector FallingHitNormal = FVector::VectorPlaneProject( LastFloorResult.HitResult.Normal, -UpDirection).GetSafeNormal();
				Params.MoveInput = FVector::VectorPlaneProject(Params.MoveInput, FallingHitNormal);
			}
		}
	}
	
	OutProposedMove = UBulletAirMovementUtils::ComputeControlledFreeMove(Params);
	const FVector VelocityWithGravity = StartVelocity + UBulletMovementUtils::ComputeVelocityFromGravity(MoverComp->GetGravityAcceleration(), DeltaSeconds);

	//  If we are going faster than TerminalVerticalVelocity apply VerticalFallingDeceleration otherwise reset Z velocity to before we applied deceleration 
	if (VelocityWithGravity.GetAbs().Dot(UpDirection) > TerminalVerticalSpeed)
	{
		if (bShouldClampTerminalVerticalSpeed)
		{
			const float ClampedVerticalSpeed = FMath::Sign(VelocityWithGravity.Dot(UpDirection)) * TerminalVerticalSpeed;
			UBulletMovementUtils::SetGravityVerticalComponent(OutProposedMove.LinearVelocity, ClampedVerticalSpeed, UpDirection);
		}
		else
		{
			float DesiredDeceleration = FMath::Abs(TerminalVerticalSpeed - VelocityWithGravity.GetAbs().Dot(UpDirection)) / DeltaSeconds;
			float DecelerationToApply = FMath::Min(DesiredDeceleration, VerticalFallingDeceleration);
			DecelerationToApply = FMath::Sign(VelocityWithGravity.Dot(UpDirection)) * DecelerationToApply * DeltaSeconds;
			FVector MaxUpDirVelocity = VelocityWithGravity * UpDirection - (UpDirection * DecelerationToApply);
			
			UBulletMovementUtils::SetGravityVerticalComponent(OutProposedMove.LinearVelocity, MaxUpDirVelocity.Dot(UpDirection), UpDirection);
		}
	}
	else
	{
		UBulletMovementUtils::SetGravityVerticalComponent(OutProposedMove.LinearVelocity, VelocityWithGravity.Dot(UpDirection), UpDirection);
	}
}

void UBulletKinematicFallingMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
	UBulletMoverComponent* MoverComponent = GetMoverComponent();
	const FBulletMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	FBulletProposedMove ProposedMove = Params.ProposedMove;

	if (!UpdatedComponent)
	{
		return;
	}

	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletUpdatedMotionState* StartingSyncState = StartState.SyncState.Collection.FindDataByType<FBulletUpdatedMotionState>();
	check(StartingSyncState);

	FBulletUpdatedMotionState& OutputSyncState = OutputState.SyncState.Collection.FindOrAddMutableDataByType<FBulletUpdatedMotionState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	float PctTimeApplied = 0.f;

	FBulletMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);
	
	UBulletMoverBlackboard* SimBlackboard = MoverComponent->GetSimBlackboard_Mutable();

	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);	// falling = no valid floor
	SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

	OutputSyncState.MoveDirectionIntent = (ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector);

	// Use the orientation intent directly. If no intent is provided, use last frame's orientation. Note that we are assuming rotation changes can't fail. 
	const FRotator StartingOrient = StartingSyncState->GetOrientation_WorldSpace();
	const FRotator TargetOrient = UBulletMovementUtils::ApplyAngularVelocityToRotator(StartingOrient, ProposedMove.AngularVelocityDegrees, DeltaSeconds);
	
	const FVector StartingFallingVelocity = StartingSyncState->GetVelocity_WorldSpace();

	const FVector UpDirection = MoverComponent->GetUpDirection();

	// If we're just starting to fall downwards, check for a floor first
	if (FMath::IsNearlyZero(StartingFallingVelocity.Dot(UpDirection)) && !ProposedMove.LinearVelocity.IsNearlyZero() &&
		ProposedMove.LinearVelocity.Dot(MoverComponent->GetUpDirection()) < 0.0)
	{
		// If we are very close to a walkable floor, make sure we're maintaining the correct distance from it
		FBulletFloorCheckResult FloorUnderActor;
		UBulletFloorQueryUtils::FindFloor(Params.MovingComps, CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->bUseFlatBaseForFloorChecks, UpdatedComponent->GetComponentLocation(), OUT FloorUnderActor);

		if (FloorUnderActor.IsWalkableFloor())
		{
			UBulletGroundMovementUtils::TryMoveToKeepMinHeightAboveFloor(MoverComponent, FloorUnderActor, CommonLegacySettings->MaxWalkSlopeCosine, MoveRecord);
			CaptureFinalState(UpdatedComponent, *StartingSyncState, FloorUnderActor, DeltaSeconds, DeltaSeconds * PctTimeApplied, ProposedMove.AngularVelocityDegrees, OutputSyncState, OutputState, MoveRecord);
			return;
		}
	}

	//FVector MoveDelta = 0.5f * (PriorFallingVelocity + ProposedMove.LinearVelocity) * DeltaSeconds; 	// TODO: revive midpoint integration
	FVector MoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;

	FHitResult Hit(1.f);
	FQuat TargetOrientQuat = TargetOrient.Quaternion();

	if (CommonLegacySettings->bShouldRemainVertical)
	{
		TargetOrientQuat = FRotationMatrix::MakeFromZX(UpDirection, TargetOrientQuat.GetForwardVector()).ToQuat();
	}

	UBulletMovementUtils::TrySafeMoveUpdatedComponent(Params.MovingComps, MoveDelta, TargetOrientQuat, true, Hit, ETeleportType::None, MoveRecord);

	FBulletFloorCheckResult LandingFloor;

	// Handle impact, whether it's a landing surface or something to slide on
	if (Hit.IsValidBlockingHit() && UpdatedComponent)
	{
		float LastMoveTimeSlice = DeltaSeconds;
		float SubTimeTickRemaining = LastMoveTimeSlice * (1.f - Hit.Time);

		PctTimeApplied += Hit.Time * (1.f - PctTimeApplied);

		// Check for hitting a landing surface
		if (UBulletAirMovementUtils::IsValidLandingSpot(Params.MovingComps, UpdatedComponent->GetComponentLocation(),
			Hit, CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->bUseFlatBaseForFloorChecks, OUT LandingFloor))
		{
			UBulletGroundMovementUtils::TryMoveToKeepMinHeightAboveFloor(MoverComponent, LandingFloor, CommonLegacySettings->MaxWalkSlopeCosine, MoveRecord); // make sure we maintain a small gap over walking surfaces
			CaptureFinalState(UpdatedComponent, *StartingSyncState, LandingFloor, DeltaSeconds, DeltaSeconds * PctTimeApplied, ProposedMove.AngularVelocityDegrees, OutputSyncState, OutputState, MoveRecord);
			return;
		}
		
		LandingFloor.HitResult = Hit;
		SimBlackboard->Set(CommonBlackboard::LastFloorResult, LandingFloor);
		
		FBulletMoverOnImpactParams ImpactParams(DefaultModeNames::Falling, Hit, MoveDelta);
		MoverComponent->HandleImpact(ImpactParams);

		// We didn't land on a walkable surface, so let's try to slide along it
		UBulletAirMovementUtils::TryMoveToFallAlongSurface(Params.MovingComps, MoveDelta,
			(1.f - Hit.Time), TargetOrientQuat, Hit.Normal, Hit, true,
			CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->bUseFlatBaseForFloorChecks, LandingFloor, MoveRecord);

		PctTimeApplied += Hit.Time * (1.f - PctTimeApplied);

		if (LandingFloor.IsWalkableFloor())
		{
			UBulletGroundMovementUtils::TryMoveToKeepMinHeightAboveFloor(MoverComponent, LandingFloor, CommonLegacySettings->MaxWalkSlopeCosine, MoveRecord); // make sure we maintain a small gap over walking surfaces
			CaptureFinalState(UpdatedComponent, *StartingSyncState, LandingFloor, DeltaSeconds, DeltaSeconds * PctTimeApplied, ProposedMove.AngularVelocityDegrees, OutputSyncState, OutputState, MoveRecord);
			return;
		}
	}
	else
	{
		// This indicates an unimpeded full move
		PctTimeApplied = 1.f;
	}
	
	CaptureFinalState(UpdatedComponent, *StartingSyncState, LandingFloor, DeltaSeconds, DeltaSeconds* PctTimeApplied, ProposedMove.AngularVelocityDegrees, OutputSyncState, OutputState, MoveRecord);
}


void UBulletKinematicFallingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UBulletCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings, TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}


void UBulletKinematicFallingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}

void UBulletKinematicFallingMode::ProcessLanded(const FBulletFloorCheckResult& FloorResult, FVector& Velocity, FBulletRelativeBaseInfo& BaseInfo, FBulletMoverTickEndData& TickEndData) const
{
	const UBulletMoverComponent* MoverComp = GetMoverComponent();
	UBulletMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();

	FName NextMovementMode = NAME_None; 
	// if we can walk on the floor we landed on
	if (FloorResult.IsWalkableFloor())
	{
		if (bCancelVerticalSpeedOnLanding)
		{
			const FPlane MovementPlane(FVector::ZeroVector, MoverComp->GetUpDirection());
			Velocity = UBulletMovementUtils::ConstrainToPlane(Velocity, MovementPlane, false);
		}
		else
		{
			Velocity = FVector::VectorPlaneProject(Velocity, FloorResult.HitResult.Normal);
		}
		
		// Transfer to LandingMovementMode (usually walking), and cache any floor / movement base info
		NextMovementMode = CommonLegacySettings->GroundMovementModeName;

		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);

		if (UBulletBasedMovementUtils::IsADynamicBase(FloorResult.HitResult.GetComponent()))
		{
			BaseInfo.SetFromFloorResult(FloorResult);
		}
	}
	// we could check for other surfaces here (i.e. when swimming is implemented we can check the floor hit here and see if we need to go into swimming)

	// This would also be a good spot for implementing some falling physics interactions (i.e. falling into a movable object and pushing it based off of this actors velocity)
	
	// if a new mode was set go ahead and switch to it after this tick and broadcast we landed
	if (!NextMovementMode.IsNone())
	{
		TickEndData.MovementEndState.NextModeName = NextMovementMode;
		OnLanded.Broadcast(NextMovementMode, FloorResult.HitResult);
	}
}

void UBulletKinematicFallingMode::CaptureFinalState(USceneComponent* UpdatedComponent, const FBulletUpdatedMotionState& StartSyncState, const FBulletFloorCheckResult& FloorResult, float DeltaSeconds, float DeltaSecondsUsed, const FVector& AngularVelocityDegrees, FBulletUpdatedMotionState& OutputSyncState, FBulletMoverTickEndData& TickEndData, FBulletMovementRecord& Record) const
{
	UBulletMoverBlackboard* SimBlackboard = GetMoverComponent()->GetSimBlackboard_Mutable();

	const FVector FinalLocation = UpdatedComponent->GetComponentLocation();

	// Check for time refunds
	constexpr float MinRemainingSecondsToRefund = 0.0001f;	// If we have this amount of time (or more) remaining, give it to the next simulation step.

	if ((DeltaSeconds - DeltaSecondsUsed) >= MinRemainingSecondsToRefund)
	{
		const float PctOfTimeRemaining = (1.0f - (DeltaSecondsUsed / DeltaSeconds));
		TickEndData.MovementEndState.RemainingMs = PctOfTimeRemaining * DeltaSeconds * 1000.f;
	}
	else
	{
		TickEndData.MovementEndState.RemainingMs = 0.f;
	}
	
	Record.SetDeltaSeconds( DeltaSecondsUsed );
	
	// If we didn't use any time lets just pass along velocity so we don't lose it when we go into the next mode with refunded time
	FVector EffectiveVelocity = DeltaSecondsUsed <= UE_SMALL_NUMBER ? StartSyncState.GetVelocity_WorldSpace() : Record.GetRelevantVelocity();
	// TODO: Update Main/large movement record with substeps from our local record

	FBulletRelativeBaseInfo MovementBaseInfo;
	ProcessLanded(FloorResult, EffectiveVelocity, MovementBaseInfo, TickEndData);

	if (MovementBaseInfo.HasRelativeInfo())
	{
		SimBlackboard->Set(CommonBlackboard::LastFoundDynamicMovementBase, MovementBaseInfo);

		OutputSyncState.SetTransforms_WorldSpace( FinalLocation,
												  UpdatedComponent->GetComponentRotation(),
												  EffectiveVelocity,
												  AngularVelocityDegrees,
												  MovementBaseInfo.MovementBase.Get(), MovementBaseInfo.BoneName);
	}
	else
	{
		OutputSyncState.SetTransforms_WorldSpace( FinalLocation,
												  UpdatedComponent->GetComponentRotation(),
												  EffectiveVelocity,
												  AngularVelocityDegrees,
												  nullptr); // no movement base
	}

	UpdatedComponent->ComponentVelocity = EffectiveVelocity;
}
