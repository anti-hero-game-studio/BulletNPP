// Fill out your copyright notice in the Description page of Project Settings.


#include "DefaultMovementSet/Modes/Physics/BulletFloatingWalkingMode.h"

#include "BulletMoverComponent.h"
#include "Core/Interfaces/BulletPrimitiveComponentInterface.h"
#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"
#include "MoveLibrary/BulletGroundMovementUtils.h"
#include "MoveLibrary/BulletMovementUtils.h"

void UBulletFloatingWalkingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UBulletCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings.IsValid(), TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}

void UBulletFloatingWalkingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}


void UBulletFloatingWalkingMode::GenerateMove_Implementation(const FBulletMoverTickStartData& StartState,
	const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const
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
}

void UBulletFloatingWalkingMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
	
	if (!CommonLegacySettings.IsValid())
	{
		return;
	}

	UBulletMoverComponent* MoverComp = GetMoverComponent();
	if (!MoverComp) return;
	
	const FBulletMoverTickStartData& StartState = Params.StartState;
	
	UPrimitiveComponent* P = MoverComp->GetBulletPhysicsBodyComponent();
	if (!P) return;
	
	IBulletPrimitiveComponentInterface* I = Cast<IBulletPrimitiveComponentInterface>(P);	
	if (!I) return;

	FBulletProposedMove ProposedMove = Params.ProposedMove;
	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletUpdatedMotionState* StartingSyncState = StartState.SyncState.Collection.FindDataByType<FBulletUpdatedMotionState>();
	check(StartingSyncState);

	FBulletUpdatedMotionState& OutputSyncState = OutputState.SyncState.Collection.FindOrAddMutableDataByType<FBulletUpdatedMotionState>();


	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;


	if (UBulletPhysicsWorldSubsystem* S = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>())
	{
		
		btRigidBody* MyBody = S->GetRigidBody(P);
		if (!MyBody) return;
		
		FTransform T;
		FVector V,A,F;
		S->GetPhysicsState(P,T, V, A, F);
		const FVector Start = T.GetLocation();
		const FVector End = Start + (-MoverComp->GetUpDirection() * 200.f);
		int32 HitBodyId;
		const FHitResult& Hit = S->LineTraceSingleByChannel(Start, End, ECC_WorldStatic, {MoverComp->GetOwner()}, HitBodyId );


		if (Hit.bBlockingHit)
		{
			FVector Velocity = StartingSyncState->GetVelocity_WorldSpace_Quantized();
			FVector RayDir = -MoverComp->GetUpDirection();
			
			FVector OtherVelocity = FVector::Zero();
			btRigidBody* Body = S->GetRigidBody(Hit);
			if (Body)
			{
				OtherVelocity = BulletHelpers::ToUnrealVector3(Body->getLinearVelocity());
			}
			
			float RayDirectionalVelocity = RayDir.Dot(Velocity);
			float OtherDirectionalVelocity = RayDir.Dot(OtherVelocity);
			
			float RelativeVelocity = RayDirectionalVelocity - OtherDirectionalVelocity;
			float X = Hit.Distance - RideHeight;
			
			float SpringForce = (X * RideSpringStrength) - (RelativeVelocity * RideSpringDamper);
			
			const FVector VelocityWithSpring = RayDir * SpringForce;
			
			
			
			OutputSyncState.SetLinearAndAngularVelocity_WorldSpace(ProposedMove.LinearVelocity + VelocityWithSpring, ProposedMove.AngularVelocityDegrees);
		}
		else
		{
			OutputState.MovementEndState.NextModeName = CommonLegacySettings->AirMovementModeName;
			OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs - (Params.TimeStep.StepMs * Hit.Time);
		}
	}
	else
	{
		OutputSyncState = *StartingSyncState;
	}
	
	
}
