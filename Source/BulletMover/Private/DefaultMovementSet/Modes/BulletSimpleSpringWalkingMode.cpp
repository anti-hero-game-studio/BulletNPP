// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/Modes/BulletSimpleSpringWalkingMode.h"
#include "DefaultMovementSet/Modes/BulletSimpleSpringState.h"
#include "BulletMoverComponent.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"
#include "Animation/SpringMath.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletSimpleSpringWalkingMode)

void UBulletSimpleSpringWalkingMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
	Super::SimulationTick_Implementation(Params, OutputState);

	// We've already updated the spring state during GenerateMove, and just need to copy it into the output simulation state
	if (const FBulletSimpleSpringState* InSpringState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FBulletSimpleSpringState>())
	{
		FBulletSimpleSpringState& OutputSpringState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FBulletSimpleSpringState>();
		OutputSpringState = *InSpringState;
	}
}

void UBulletSimpleSpringWalkingMode::GenerateWalkMove_Implementation(FBulletMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity,
	const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity)
{
	FBulletSimpleSpringState& SpringState = StartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FBulletSimpleSpringState>();

	// Linear //
	
	SpringMath::CriticalSpringDamper(InOutVelocity, SpringState.CurrentAccel, DesiredVelocity, VelocitySmoothingTime, DeltaSeconds);
	
	// Angular //
	
	FVector CurrentAngularVelocityRad = FMath::DegreesToRadians(InOutAngularVelocityDegrees);
	FQuat UpdatedFacing = CurrentFacing;
	SpringMath::CriticalSpringDamperQuat(UpdatedFacing, CurrentAngularVelocityRad, DesiredFacing, FacingSmoothingTime, DeltaSeconds);
	InOutAngularVelocityDegrees = FMath::RadiansToDegrees(CurrentAngularVelocityRad);
}

