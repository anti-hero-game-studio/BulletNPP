// Fill out your copyright notice in the Description page of Project Settings.


#include "DefaultMovementSet/Modes/Physics/BulletPhysicsWalkingMode.h"

#include "BulletMoverComponent.h"
#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"
#include "MoveLibrary/BulletAsyncMovementUtils.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"
#include "MoveLibrary/BulletGroundMovementUtils.h"
#include "MoveLibrary/BulletMovementUtils.h"
#include "MoveLibrary/BulletPhysicsGroundMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletPhysicsWalkingMode)


UBulletPhysicsWalkingMode::UBulletPhysicsWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClasses.Add(UBulletCommonLegacyMovementSettings::StaticClass());
	
	RadialForceLimit = 2000.0f;
	SwingTorqueLimit = 3000.0f;
	TwistTorqueLimit = 1500.0f;

	GameplayTags.AddTag(BulletMover_IsOnGround);
}

void UBulletPhysicsWalkingMode::GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const
{
	const UBulletMoverComponent* MoverComp = GetMoverComponent();
	if (!MoverComp) return;
	const UPrimitiveComponent* UpdatedComponent = MoverComp->GetUpdatedComponent<UPrimitiveComponent>();
	if (!UpdatedComponent) return;
	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletUpdatedMotionState* StartingSyncState = StartState.SyncState.Collection.FindDataByType<FBulletUpdatedMotionState>();
	check(StartingSyncState);

	if (!CommonLegacySettings.IsValid())
	{
		return;
	}

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	FBulletFloorCheckResult LastFloorResult;
	FVector MovementNormal;

	UBulletMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();
	FVector UpDirection = MoverComp->GetUpDirection();

	// Try to use the floor as the basis for the intended move direction (i.e. try to walk along slopes, rather than into them)
	if (SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult) && LastFloorResult.IsWalkableFloor())
	{
		MovementNormal = LastFloorResult.HitResult.ImpactNormal;
	}
	else
	{
		MovementNormal = UpDirection;
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
	
	FBulletGroundMoveParams Params;

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

	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = FVector::VectorPlaneProject(StartingSyncState->GetVelocity_WorldSpace(), MovementNormal);
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.GroundNormal = MovementNormal;
	Params.TurningRate = CommonLegacySettings->TurningRate;
	Params.TurningBoost = CommonLegacySettings->TurningBoost;
	Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
	Params.Acceleration = CommonLegacySettings->Acceleration;
	Params.Deceleration = CommonLegacySettings->Deceleration;
	Params.DeltaSeconds = DeltaSeconds;
	Params.WorldToGravityQuat = MoverComp->GetWorldToGravityTransform();
	Params.UpDirection = UpDirection;
	Params.bUseAccelerationForVelocityMove = CommonLegacySettings->bUseAccelerationForVelocityMove;
	
	if (Params.MoveInput.SizeSquared() > 0.f && !UBulletMovementUtils::IsExceedingMaxSpeed(Params.PriorVelocity, CommonLegacySettings->MaxSpeed))
	{
		Params.Friction = CommonLegacySettings->GroundFriction;
	}
	else
	{
		Params.Friction = CommonLegacySettings->bUseSeparateBrakingFriction ? CommonLegacySettings->BrakingFriction : CommonLegacySettings->GroundFriction;
		Params.Friction *= CommonLegacySettings->BrakingFrictionFactor;
	}
	
	OutProposedMove = UBulletGroundMovementUtils::ComputeControlledGroundMove(Params);
	UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	if (SimBlackboard && Subsystem)
	{
		const FBulletUserData* D = Subsystem->GetUserData(UpdatedComponent);
		if (!D) return;
		// Update the floor result and check the proposed move to prevent movement onto unwalkable surfaces
		FVector OutDeltaPos = FVector::ZeroVector;
		FBulletFloorCheckResult FloorResult;
		GetFloorAndCheckMovement(*StartingSyncState, OutProposedMove, DeltaSeconds, D ,FloorResult, OutDeltaPos);

		OutProposedMove.LinearVelocity = OutDeltaPos / DeltaSeconds;

		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
		//SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);

		if (bMaintainHorizontalGroundVelocity)
		{
			// So far have assumed we are on level ground, so now add velocity up the slope
			OutProposedMove.LinearVelocity -= UpDirection * OutProposedMove.LinearVelocity.Dot(FloorResult.HitResult.ImpactNormal) / UpDirection.Dot(FloorResult.HitResult.ImpactNormal);
		}
	};
}

void UBulletPhysicsWalkingMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
	if (!CommonLegacySettings.IsValid())
	{
		return;
	}
	
	UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	if (!Subsystem) return;

	UBulletMoverComponent* MoverComp = GetMoverComponent();
	const FBulletMoverTickStartData& StartState = Params.StartState;
	FBulletProposedMove ProposedMove = Params.ProposedMove;

	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletUpdatedMotionState* StartingSyncState = StartState.SyncState.Collection.FindDataByType<FBulletUpdatedMotionState>();
	const FBulletMoverTargetSyncState* StartingTargetState = StartState.SyncState.Collection.FindDataByType<FBulletMoverTargetSyncState>();
	check(StartingSyncState);

	FBulletUpdatedMotionState& OutputSyncState = OutputState.SyncState.Collection.FindOrAddMutableDataByType<FBulletUpdatedMotionState>();
	OutputSyncState = *StartingSyncState;
	
	FBulletMoverTargetSyncState& OutputTargetState = OutputState.SyncState.Collection.FindOrAddMutableDataByType<FBulletMoverTargetSyncState>();
	OutputTargetState = *StartingTargetState;


	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	const FVector OrigMoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;

	const FVector StartLocation = StartingSyncState->GetLocation_WorldSpace();
	const FVector TargetLocation = StartLocation + OrigMoveDelta;


	FBulletMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);

	FBulletFloorCheckResult CurrentFloor;
	UBulletMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();

	FVector UpDirection = MoverComp->GetUpDirection();
	
	OutputSyncState.MoveDirectionIntent = (ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector);

	const FRotator StartingOrient = StartingSyncState->GetOrientation_WorldSpace();
	const FRotator TargetOrient = UBulletMovementUtils::ApplyAngularVelocityToRotator(StartingOrient, ProposedMove.AngularVelocityDegrees, DeltaSeconds);
	const bool bIsOrientationChanging = !StartingOrient.Equals(TargetOrient);

	const FQuat StartRotation = StartingOrient.Quaternion();

	FQuat TargetRotation = TargetOrient.Quaternion();
	if (CommonLegacySettings->bShouldRemainVertical)
	{
		TargetRotation = FRotationMatrix::MakeFromZX(UpDirection, TargetRotation.GetForwardVector()).ToQuat();
	}

	FVector LocationInProgress = StartLocation;
	FQuat   RotationInProgress = StartRotation;

	FHitResult MoveHitResult(1.f);
	
	FVector CurMoveDelta = OrigMoveDelta;
	
	
	if (!CharacterInputs)
	{
		UE_LOG(LogBulletMover, Warning, TEXT("Bullet Physics Falling Mode requires FBulletCharacterDefaultInputs"));
		return;
	}
	
	FVector GroundNormal = UpDirection;
	FBulletFloorCheckResult FloorResult;
	if (SimBlackboard)
	{
		SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult);
		GroundNormal = FloorResult.HitResult.ImpactNormal;
	};

	if (FloorResult.IsWalkableFloor())
	{
		const btCollisionObject* GroundParticle = Subsystem->GetCollisionBody(FloorResult.HitResult);
		if (!GroundParticle) return;
		
		const btRigidBody* GroundBody = btRigidBody::upcast(GroundParticle);
		if (!GroundBody) return;
		
		const float InitialHeightAboveFloor = FloorResult.FloorDist - GetTargetHeight();

		// Put the target position on the floor at the target height
		FVector TargetPosition = StartingSyncState->GetLocation_WorldSpace() - UpDirection * InitialHeightAboveFloor;

		// The base movement mode does not apply gravity in walking mode so apply here.
		// Also remove the gravity that will be applied by the physics simulation.
		// This is so that the gravity in this mode will be consistent with the gravity
		// set on the mover, not the default physics gravity
		const FVector ProjectedVelocity = StartingSyncState->GetVelocity_WorldSpace() + BulletHelpers::ToUnrealDirection(GroundBody->getGravity()) * DeltaSeconds;
		FVector TargetVelocity = ProjectedVelocity - BulletHelpers::ToUnrealFloat(GroundBody->getGravity().length()) * FVector::UpVector * DeltaSeconds;

		// If we have movement intent and not moving straight up/down then use the proposed move plane velocity
		// otherwise just fall with gravity
		constexpr float ParallelCosThreshold = 0.999f;
		const bool bNonVerticalVelocity = !FVector::Parallel(TargetVelocity.GetSafeNormal(), UpDirection, ParallelCosThreshold);
		const bool bUseProposedMove = bNonVerticalVelocity || ProposedMove.bHasDirIntent;
		bool bNormalVelocityIntent = false;

		if (bUseProposedMove)
		{
			const FVector ProposedMovePlaneVelocity = ProposedMove.LinearVelocity - ProposedMove.LinearVelocity.ProjectOnToNormal(GroundNormal);

			// If there is velocity intent in the normal direction then use the velocity from the proposed move. Otherwise
			// retain the previous vertical velocity
			FVector ProposedNormalVelocity = ProposedMove.LinearVelocity - ProposedMovePlaneVelocity;
			if (ProposedNormalVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
			{
				TargetVelocity += ProposedNormalVelocity - TargetVelocity.ProjectOnToNormal(GroundNormal);
				bNormalVelocityIntent = true;
			}

			TargetPosition += ProposedMovePlaneVelocity * DeltaSeconds;
		}

		FVector ProjectedGroundVelocity = UBulletPhysicsGroundMovementUtils::ComputeLocalGroundVelocity_Internal(this, StartingSyncState->GetLocation_WorldSpace(), FloorResult);
		const float BodyGravity = BulletHelpers::ToUnrealFloat(GroundBody->getGravity().length());
		if (FMath::Abs(BodyGravity) > 0)
		{
			// This might not be correct if different physics objects have different gravity but is saves having to go
			// to the component to get the gravity on the physics volume.
			ProjectedGroundVelocity += BodyGravity * UpDirection * DeltaSeconds;
			
		}
		const bool bIsGroundMoving = ProjectedGroundVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER;
		const FVector ProjectedRelativeVelocity = TargetVelocity - ProjectedGroundVelocity;
		const float ProjectedRelativeNormalVelocity = FloorResult.HitResult.ImpactNormal.Dot(TargetVelocity - ProjectedGroundVelocity);
		const float ProjectedRelativeVerticalVelocity = GroundNormal.Dot(TargetVelocity - ProjectedGroundVelocity);
		const float VerticalVelocityLimit = bNormalVelocityIntent ? 2.0f / DeltaSeconds : FMath::Abs(GroundNormal.Dot(BulletHelpers::ToUnrealDirection(GroundBody->getGravity())) * DeltaSeconds);

		bool bIsLiftingOffSurface = false;
		if ((ProjectedRelativeNormalVelocity > VerticalVelocityLimit) && bIsGroundMoving && (ProjectedRelativeVerticalVelocity > VerticalVelocityLimit))
		{
			bIsLiftingOffSurface = true;
		}

		// Determine if the character is stepping up or stepping down.
		// If stepping up make sure that the step height is less than the max step height
		// and the new surface has CanCharacterStepUpOn set to true.
		// If stepping down make sure the step height is less than the max step height.
		const float EndHeightAboveFloor = InitialHeightAboveFloor + ProjectedRelativeVerticalVelocity * DeltaSeconds;
		const bool bIsSteppingDown = InitialHeightAboveFloor > UE_KINDA_SMALL_NUMBER;
		const bool bIsWithinReach = EndHeightAboveFloor <= CommonLegacySettings->MaxStepHeight;
		const bool bIsSupported = bIsWithinReach && !bIsLiftingOffSurface;
		const bool bNeedsVerticalVelocityToTarget = bIsSupported && bIsSteppingDown && (EndHeightAboveFloor > 0.0f) && !bIsLiftingOffSurface;
		if (bNeedsVerticalVelocityToTarget)
		{
			TargetVelocity -= FractionalDownwardVelocityToTarget * (EndHeightAboveFloor / DeltaSeconds) * UpDirection;
		}

		// Target orientation
		// This is always applied regardless of whether the character is supported
		const FRotator TargetOrientation = UBulletMovementUtils::ApplyAngularVelocityToRotator(StartingSyncState->GetOrientation_WorldSpace(), ProposedMove.AngularVelocityDegrees, DeltaSeconds);

		OutputTargetState.UpdateTargetVelocity(TargetVelocity, ProposedMove.AngularVelocityDegrees);
		OutputState.MovementEndState.RemainingMs = 0.0f;
		OutputState.MovementEndState.NextModeName = Params.StartState.SyncState.MovementMode;
		OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
	}
	else
	{
		OutputState.MovementEndState.RemainingMs = 0.0f;
		OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
		OutputTargetState.UpdateTargetVelocity(StartingSyncState->GetVelocity_WorldSpace(), StartingSyncState->GetAngularVelocityDegrees_WorldSpace());
		OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
	}
	
	
}

void UBulletPhysicsWalkingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UBulletCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings.IsValid(), TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}

void UBulletPhysicsWalkingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}

bool UBulletPhysicsWalkingMode::CanStepUpOnHitSurface(const FBulletFloorCheckResult& FloorResult) const
{
	const float StepHeight = GetTargetHeight() - FloorResult.FloorDist;

	bool bWalkable = StepHeight <= CommonLegacySettings->MaxStepHeight;
	constexpr float MinStepHeight = 2.0f;
	const bool SteppingUp = StepHeight > MinStepHeight;
	if (bWalkable && SteppingUp)
	{
		bWalkable = UBulletGroundMovementUtils::CanStepUpOnHitSurface(FloorResult.HitResult);
	}

	return bWalkable;
}

void UBulletPhysicsWalkingMode::GetFloorAndCheckMovement(const FBulletUpdatedMotionState& SyncState, const FBulletProposedMove& ProposedMove, float DeltaSeconds, 
	const FBulletUserData* InputData, FBulletFloorCheckResult& FloorResult, FVector& OutDeltaPos) const
{
	const UBulletMoverComponent* MoverComponent = GetMoverComponent();
	if (!MoverComponent) return;
	FVector DeltaPos = ProposedMove.LinearVelocity * DeltaSeconds;
	OutDeltaPos = DeltaPos;
	
	if (DeltaPos.SizeSquared() < UE_SMALL_NUMBER)
	{
		// Stationary
		OutDeltaPos = FVector::ZeroVector;
		return;
	}
	
	if (!InputData)
	{
		UE_LOG(LogBulletMover, Error, TEXT("Could not find User Input Data In UBulletPhysicsWalkingMode::GetFloorAndCheckMovement"));
		return;
	}

	FloorCheck(SyncState.GetLocation_WorldSpace(), ProposedMove.LinearVelocity, DeltaSeconds, FloorResult);

	if (!FloorResult.bBlockingHit)
	{
		// No result at the end position. Fall back on the current floor result
		return;
	}

	bool bWalkableFloor = FloorResult.bWalkableFloor && CanStepUpOnHitSurface(FloorResult);
	if (bWalkableFloor)
	{
		// Walkable floor found
		return;
	}
	
	// Hit something but not walkable. Try a new query to find a walkable surface
	const float StepBlockedHeight = GetTargetHeight() - InputData->ShapeHeight + InputData->ShapeRadius;
	const float StepHeight = GetTargetHeight() - FloorResult.FloorDist;

	if (StepHeight > StepBlockedHeight)
	{
		// Collision should prevent movement. Just try to find ground at start of movement
		FloorCheck(SyncState.GetLocation_WorldSpace(), FVector::Zero(), DeltaSeconds, FloorResult);
		FloorResult.bWalkableFloor = FloorResult.bWalkableFloor && CanStepUpOnHitSurface(FloorResult);
		return;
	}


	

	// Try to limit the movement to remain on a walkable surface
	FVector HorizSurfaceDir = FVector::VectorPlaneProject(FloorResult.HitResult.ImpactNormal, MoverComponent->GetUpDirection());
	float HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();
	bool bFoundOutwardDir = false;
	if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
	{
		HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
		bFoundOutwardDir = true;
	}
	else
	{
		// Flat unwalkable surface. Try and get the horizontal direction from the normal instead
		HorizSurfaceDir = FVector::VectorPlaneProject(FloorResult.HitResult.Normal, MoverComponent->GetUpDirection());
		HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();

		if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
		{
			HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
			bFoundOutwardDir = true;
		}
	}

	if (bFoundOutwardDir)
	{
		const float DP = DeltaPos.Dot(HorizSurfaceDir);
		FVector NewDeltaPos = DeltaPos;
		if (DP < 0.0f)
		{
			// If we're moving away try a ray query at the end of the motion
			NewDeltaPos = DeltaPos - DP * HorizSurfaceDir;
		}

		FloorCheck(SyncState.GetLocation_WorldSpace(), NewDeltaPos, DeltaSeconds, FloorResult);
		FloorResult.bWalkableFloor = FloorResult.bWalkableFloor && CanStepUpOnHitSurface(FloorResult);

		if (FloorResult.bWalkableFloor)
		{
			OutDeltaPos = NewDeltaPos;
		}
	}
	else
	{
		OutDeltaPos = FVector::ZeroVector;
	}
}
