// Fill out your copyright notice in the Description page of Project Settings.


#include "DefaultMovementSet/Modes/Physics/BulletFixedWalkingMode.h"
#include "BulletMoverComponent.h"
#include "Core/Interfaces/BulletPrimitiveComponentInterface.h"
#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"
#include "MoveLibrary/BulletGroundMovementUtils.h"
#include "MoveLibrary/BulletMovementUtils.h"

static bool TryGetCapsuleSizeCm(const UPrimitiveComponent* UpdatedComponent, float& OutHalfHeightCm, float& OutRadiusCm)
{
	if (!UpdatedComponent) return false;

	const UWorld* World = UpdatedComponent->GetWorld();
	if (!World) return false;

	const UBulletPhysicsWorldSubsystem* Subsystem = World->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	if (!Subsystem) return false;
	
	if (const FBulletUserData* UserData = Subsystem->GetUserData(UpdatedComponent))
	{
		OutRadiusCm = UserData->ShapeRadius;
		OutHalfHeightCm = UserData->ShapeHeight;
		return true;
	}
	return false;
}

void UBulletFixedWalkingMode::GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const
{
	const UBulletMoverComponent* MoverComp = GetMoverComponent();
	if (!MoverComp) return;
	const UPrimitiveComponent* UpdatedComponent = MoverComp->GetUpdatedComponent<UPrimitiveComponent>();
	if (!UpdatedComponent) return;
	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletUpdatedMotionState* StartingSyncState = StartState.SyncState.Collection.FindDataByType<FBulletUpdatedMotionState>();
	check(StartingSyncState);

	if (!CommonLegacySettings)
	{
		return;
	}

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	UBulletMoverBlackboard* SimBlackboard = MoverComp->GetSimBlackboard_Mutable();
	FVector UpDirection = MoverComp->GetUpDirection();
	FVector MovementNormal = UpDirection;
	

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
}

void UBulletFixedWalkingMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
	if (!CommonLegacySettings)
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
	
	const UBulletMoverComponent* MoverCompConst = GetMoverComponent();
	UPrimitiveComponent* UpdatedComponent = MoverCompConst ? MoverCompConst->GetUpdatedComponent<UPrimitiveComponent>() : nullptr;
	if (!UpdatedComponent) return;

	const FVector CurrentVelocity = StartingSyncState->GetVelocity_WorldSpace();

	// --- 1) Planar target velocity from ProposedMove (fixes “walking stuck”) ---
	const FVector ProposedPlanarVelocity = ProposedMove.LinearVelocity - ProposedMove.LinearVelocity.ProjectOnToNormal(UpDirection);

	// Preserve existing vertical component for now; suspension will adjust it
	FVector TargetVelocity = ProposedPlanarVelocity + CurrentVelocity.ProjectOnToNormal(UpDirection);

	// --- 2) Compute capsule base height above plane Z=0 (or FloorPlaneZ) ---
	float CapsuleHalfHeightCm = 0.0f;
	float CapsuleRadiusCm = 0.0f;
	if (!TryGetCapsuleSizeCm(UpdatedComponent, CapsuleHalfHeightCm, CapsuleRadiusCm))
	{
	    // If your updated component is not a UCapsuleComponent, replace this with your own geometry source.
	    UE_LOG(LogBulletMover, Error, TEXT("Walking hover requires a capsule UpdatedComponent or a capsule size source."));
	    return;
	}

	// Base of capsule along UpDirection: location minus (halfHeight - radius)
	const FVector Location = StartingSyncState->GetLocation_WorldSpace();
	const FVector CapsuleBaseWS = Location - UpDirection * (CapsuleHalfHeightCm - CapsuleRadiusCm);

	// For plane Z = FloorPlaneZ, the “height” in cm is simply the world Z offset.
	// (If UpDirection is guaranteed (0,0,1), this is exact; otherwise use dot vs UpDirection and a plane point.)
	const float CapsuleBaseHeightCm = CapsuleBaseWS.Z;
	const float DesiredCapsuleBaseHeightCm = FloorPlaneZ + TargetHoverHeight;

	// Height error: positive => capsule is too low => push upward
	float HeightErrorCm = DesiredCapsuleBaseHeightCm - CapsuleBaseHeightCm;

	// Deadzone to avoid micro-jitter
	if (FMath::Abs(HeightErrorCm) <= HoverHeightTolerance)
	{
	    HeightErrorCm = 0.0f;
	}

	// --- 3) Suspension vertical correction (spring-damper in velocity space) ---
	const float CurrentUpSpeedCmPerSec = FVector::DotProduct(CurrentVelocity, UpDirection);

	// Desired acceleration along UpDirection
	float DesiredUpwardAccelerationCmPerSec2 = (SuspensionStiffness * HeightErrorCm) - (SuspensionDamping * CurrentUpSpeedCmPerSec);

	// Clamp accel
	DesiredUpwardAccelerationCmPerSec2 = FMath::Clamp(
	    DesiredUpwardAccelerationCmPerSec2,
	    -MaxDownwardAcceleration,
	    MaxUpwardAcceleration);

	// Convert to delta-v for this step
	float DeltaUpSpeedCmPerSec = DesiredUpwardAccelerationCmPerSec2 * DeltaSeconds;

	// Clamp delta-v per step (primary “no pop” control)
	DeltaUpSpeedCmPerSec = FMath::Clamp(
	    DeltaUpSpeedCmPerSec,
	    -MaxDownwardVelocityChangePerStep,
	    MaxUpwardVelocityChangePerStep);

	// --- 4) Pop suppression: cancel upward velocity while “supported” ---
	const bool bIsSupported = true; // flat floor, always supported unless you implement explicit jump disabling
	if (bIsSupported && (CurrentUpSpeedCmPerSec > CancelUpwardVelocityWhenSupportedThreshold))
	{
	    const float UpwardExcess = CurrentUpSpeedCmPerSec - CancelUpwardVelocityWhenSupportedThreshold;
	    const float CancelAmount = FMath::Min(UpwardExcess, MaxUpwardVelocityCancelPerStep);
	    DeltaUpSpeedCmPerSec -= CancelAmount;
	}

	// Apply vertical correction
	TargetVelocity += UpDirection * DeltaUpSpeedCmPerSec;

	// --- 5) Output target velocity and orientation ---
	OutputTargetState.UpdateTargetVelocity(TargetVelocity, ProposedMove.AngularVelocityDegrees);
	OutputState.MovementEndState.RemainingMs = 0.0f;
	OutputState.MovementEndState.NextModeName = Params.StartState.SyncState.MovementMode;
	OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
}

void UBulletFixedWalkingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);
	
	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UBulletCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings, TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}

void UBulletFixedWalkingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;
	Super::OnUnregistered();
}
