// Fill out your copyright notice in the Description page of Project Settings.


#include "DefaultMovementSet/Modes/Physics/BulletPhysicsFallingMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "BulletMoverComponent.h"
#include "MoveLibrary/BulletAirMovementUtils.h"
#include "MoveLibrary/BulletAsyncMovementUtils.h"
#include "MoveLibrary/BulletBasedMovementUtils.h"
#include "MoveLibrary/BulletGroundMovementUtils.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"
#include "MoveLibrary/BulletMovementUtils.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"

#include "DrawDebugHelpers.h"
#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletPhysicsFallingMode)

UBulletPhysicsFallingMode::UBulletPhysicsFallingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCancelVerticalSpeedOnLanding(true)
	, AirControlPercentage(0.4f)
	, FallingDeceleration(200.0f)
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


void UBulletPhysicsFallingMode::GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const
{
	const UBulletMoverComponent* MoverComp = GetMoverComponent();
	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletUpdatedMotionState* StartingSyncState = StartState.SyncState.Collection.FindDataByType<FBulletUpdatedMotionState>();
	check(StartingSyncState);

	if (!CommonLegacySettings.IsValid())
	{
		return;
	}

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	FVector UpDirection = MoverComp->GetUpDirection();
	
	// We don't want velocity limits to take the falling velocity component into account, since it is handled 
	//   separately by the terminal velocity of the environment.
	const FVector StartVelocity = StartingSyncState->GetVelocity_WorldSpace_Quantized();
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
		IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace_Quantized();
	}
	else
	{
		IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
	}

	IntendedOrientation_WorldSpace = UBulletMovementUtils::ApplyGravityToOrientationIntent(IntendedOrientation_WorldSpace, MoverComp->GetWorldToGravityTransform(), CommonLegacySettings->bShouldRemainVertical);
	
	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = StartHorizontalVelocity;
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace_Quantized();
	Params.DeltaSeconds = DeltaSeconds;
	Params.TurningRate = CommonLegacySettings->TurningRate;
	Params.TurningBoost = CommonLegacySettings->TurningBoost;
	Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
	Params.Acceleration = CommonLegacySettings->Acceleration;
	Params.Deceleration = FallingDeceleration;
	Params.WorldToGravityQuat = MoverComp->GetWorldToGravityTransform();
	Params.bUseAccelerationForVelocityMove = CommonLegacySettings->bUseAccelerationForVelocityMove;

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
	
	
	UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	const UBulletCommonLegacyMovementSettings* SharedSettingsPtr = GetMoverComponent<UBulletMoverComponent>()->FindSharedSettings<UBulletCommonLegacyMovementSettings>();
	if (SimBlackboard && Subsystem && SharedSettingsPtr)
	{
		FBulletFloorCheckResult FloorResult;
		FloorCheck(StartingSyncState->GetLocation_WorldSpace(), OutProposedMove.LinearVelocity,TimeStep.StepMs * 0.001f, FloorResult);
		
		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
	}
}

void UBulletPhysicsFallingMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
	if (!CommonLegacySettings.IsValid())
	{
		return;
	}

	
	UBulletMoverComponent* MoverComponent = GetMoverComponent();
	const FBulletMoverTickStartData& StartState = Params.StartState;
	const UPrimitiveComponent* UpdatedComponent = Cast<UPrimitiveComponent>(Params.MovingComps.UpdatedComponent.Get());
	
	if (!UpdatedComponent) return;

	const FBulletProposedMove ProposedMove = Params.ProposedMove;

	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletUpdatedMotionState* StartingSyncState = StartState.SyncState.Collection.FindDataByType<FBulletUpdatedMotionState>();
	check(StartingSyncState);
	
	
	FBulletUpdatedMotionState& OutputSyncState = OutputState.SyncState.Collection.FindOrAddMutableDataByType<FBulletUpdatedMotionState>();
	
	FBulletFloorCheckResult FloorResult;
	FloorCheck(StartingSyncState->GetLocation_WorldSpace(), ProposedMove.LinearVelocity,Params.TimeStep.StepMs * 0.001f, FloorResult);

	if (FloorResult.bBlockingHit)
	{
		// We are grounded and need to switch movement modes
		OutputState.MovementEndState.RemainingMs = 0.0f;
		OutputState.MovementEndState.NextModeName = DefaultModeNames::Walking;
		OutputSyncState.SetLinearAndAngularVelocity_WorldSpace(StartingSyncState->GetVelocity_WorldSpace_Quantized(), StartingSyncState->GetAngularVelocityDegrees_WorldSpace_Quantized());
		return;
	}

	
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	if (!Subsystem) return;
	
	// The physics simulation applies Z-only gravity acceleration via physics volumes, so we need to account for it here 
	const FVector TargetVel = ProposedMove.LinearVelocity - MoverComponent->GetGravityAcceleration() * FVector::UpVector;
	const FVector DeltaLinearVelocity = (TargetVel - StartingSyncState->GetVelocity_WorldSpace_Quantized()).GetClampedToMaxSize(TerminalVerticalSpeed) * DeltaSeconds;
	const FVector DeltaAngularVelocity = (ProposedMove.AngularVelocityDegrees - StartingSyncState->GetAngularVelocityDegrees_WorldSpace_Quantized()) * DeltaSeconds;

	OutputState.MovementEndState.RemainingMs = 0.0f;
	OutputState.MovementEndState.NextModeName = Params.StartState.SyncState.MovementMode;
	OutputSyncState.SetLinearAndAngularVelocity_WorldSpace(ProposedMove.LinearVelocity, ProposedMove.AngularVelocityDegrees);
}


void UBulletPhysicsFallingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UBulletCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings.IsValid(), TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}


void UBulletPhysicsFallingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}
