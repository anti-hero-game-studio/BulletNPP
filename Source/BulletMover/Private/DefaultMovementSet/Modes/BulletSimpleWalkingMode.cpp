// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/Modes/BulletSimpleWalkingMode.h"

#include "BulletMoverComponent.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletSimpleWalkingMode)

void UBulletSimpleWalkingMode::GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep,
                                                   FBulletProposedMove& OutProposedMove) const
{
	const FBulletUpdatedMotionState* StartingSyncState = StartState.SyncState.Collection.FindDataByType<FBulletUpdatedMotionState>();

	if (CommonLegacySettings.Get() == nullptr || !StartingSyncState)
	{
		return;
	}

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	if (DeltaSeconds <= FLT_EPSILON)
	{
		return;
	}

	// Get input
	FVector DesiredVelocity;
	EBulletMoveInputType MoveInputType;
	FVector DesiredFacingDir;
	 
	if (const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>())
	{
		DesiredVelocity = CharacterInputs->GetMoveInput_WorldSpace();
		MoveInputType = CharacterInputs->GetMoveInputType();
		DesiredFacingDir = CharacterInputs->GetOrientationIntentDir_WorldSpace();
	}
	else
	{
		// If no input found, most likely a networked sim proxy
		// Try to deduce an input from the sync state
		DesiredVelocity = StartingSyncState->GetIntent_WorldSpace();
		MoveInputType = EBulletMoveInputType::DirectionalIntent;
		DesiredFacingDir = StartingSyncState->GetOrientation_WorldSpace().Quaternion().GetForwardVector();
	}
	float MaxMoveSpeed = MaxSpeedOverride >= 0.0f ? MaxSpeedOverride : CommonLegacySettings->MaxSpeed;

	// Subtract vertical component but keep same magnitude
	float DesiredVelMag = DesiredVelocity.Length();
	DesiredVelocity -= DesiredVelocity.ProjectOnTo(GetMoverComponent()->GetUpDirection());
	float DesiredVel2DSquaredLength = DesiredVelocity.SquaredLength(); 
	if (DesiredVel2DSquaredLength > 0.0f)
	{
		DesiredVelocity *= DesiredVelMag / FMath::Sqrt(DesiredVel2DSquaredLength); 
	}

	switch (MoveInputType)
	{
	case EBulletMoveInputType::DirectionalIntent:
		{
			OutProposedMove.DirectionIntent = DesiredVelocity;	// here, DesiredVelocity is already in "intent space" (unit length for "max intent") so we can use it directly
			DesiredVelocity *= MaxMoveSpeed;
		}
		break;
	case EBulletMoveInputType::Velocity:
		{
			// Clamp to max move speed
			DesiredVelocity = DesiredVelocity.GetClampedToMaxSize(MaxMoveSpeed);
			OutProposedMove.DirectionIntent = MaxMoveSpeed > UE_KINDA_SMALL_NUMBER ? DesiredVelocity / MaxMoveSpeed : FVector::ZeroVector; // here, DesiredVelocity is converted to "intent space"
		}
		break;
	case EBulletMoveInputType::None:
	case EBulletMoveInputType::Invalid:
	default:
		{
			UE_LOG(LogBulletMover, Warning, TEXT("Unhandled MoveInputType %i in USpringWalkingMode"), MoveInputType);
			DesiredVelocity = FVector::ZeroVector;
			OutProposedMove.DirectionIntent = FVector::ZeroVector;
		}
	break;
	}

	OutProposedMove.bHasDirIntent = !OutProposedMove.DirectionIntent.IsNearlyZero();
	DesiredFacingDir -= DesiredFacingDir.ProjectOnTo(GetMoverComponent()->GetUpDirection());
	FQuat CurrentFacing = StartingSyncState->GetOrientation_WorldSpace().Quaternion();
	FQuat DesiredFacing = CurrentFacing;
	
	if (DesiredFacingDir.Normalize())
	{
		DesiredFacing = FQuat::FindBetween(FVector::ForwardVector, DesiredFacingDir);
	}

	OutProposedMove.LinearVelocity = StartingSyncState->GetVelocity_WorldSpace();
	FVector AngularVelocityDegrees = StartingSyncState->GetAngularVelocityDegrees_WorldSpace();
	
	// Hack const_cast stuff
	// Why is this needed?
	// Because some modes want to mutate their data inside the generate walk move
	UBulletSimpleWalkingMode* MutableSimpleWalkMode  = const_cast<UBulletSimpleWalkingMode*>(this);
	MutableSimpleWalkMode->GenerateWalkMove(const_cast<FBulletMoverTickStartData&>(StartState), DeltaSeconds, DesiredVelocity, DesiredFacing, CurrentFacing, AngularVelocityDegrees, OutProposedMove.LinearVelocity);

	// Calc angular velocity from final facing
	OutProposedMove.AngularVelocityDegrees = AngularVelocityDegrees;
}

void UBulletSimpleWalkingMode::GenerateWalkMove_Implementation(FBulletMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity,
	const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
	InOutVelocity = DesiredVelocity;

	FQuat ToFacing = CurrentFacing.Inverse() * DesiredFacing;
	InOutAngularVelocityDegrees = DeltaSeconds > 0.0f ? FMath::RadiansToDegrees(ToFacing.ToRotationVector() / DeltaSeconds) : FVector::ZeroVector;
}

