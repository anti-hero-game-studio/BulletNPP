// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/BulletAirMovementUtils.h"

#include "BulletMoverComponent.h"
#include "MoveLibrary/BulletAsyncMovementUtils.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"
#include "MoveLibrary/BulletMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletAirMovementUtils)

FBulletProposedMove UBulletAirMovementUtils::ComputeControlledFreeMove(const FBulletFreeMoveParams& InParams)
{
	FBulletProposedMove OutMove;

	const FPlane MovementPlane(FVector::ZeroVector, FVector::UpVector);

	OutMove.DirectionIntent = UBulletMovementUtils::ComputeDirectionIntent(InParams.MoveInput, InParams.MoveInputType, InParams.MaxSpeed);
	OutMove.bHasDirIntent = !OutMove.DirectionIntent.IsNearlyZero();

	FComputeVelocityParams ComputeVelocityParams;
	ComputeVelocityParams.DeltaSeconds = InParams.DeltaSeconds;
	ComputeVelocityParams.InitialVelocity = InParams.PriorVelocity;
	ComputeVelocityParams.MoveDirectionIntent = InParams.MoveInput;
	ComputeVelocityParams.MaxSpeed = InParams.MaxSpeed;
	ComputeVelocityParams.TurningBoost = InParams.TurningBoost;
	ComputeVelocityParams.Deceleration = InParams.Deceleration;
	ComputeVelocityParams.Acceleration = InParams.Acceleration;
	ComputeVelocityParams.MoveInputType = InParams.MoveInputType;
	ComputeVelocityParams.MoveInput = InParams.MoveInput; 
	ComputeVelocityParams.bUseAccelerationForVelocityMove = InParams.bUseAccelerationForVelocityMove;
	ComputeVelocityParams.Friction = InParams.Friction;
	
	OutMove.LinearVelocity = UBulletMovementUtils::ComputeVelocity(ComputeVelocityParams);
	OutMove.AngularVelocityDegrees = UBulletMovementUtils::ComputeAngularVelocityDegrees(InParams.PriorOrientation, InParams.OrientationIntent, InParams.DeltaSeconds, InParams.TurningRate);

	return OutMove;
}

bool UBulletAirMovementUtils::IsValidLandingSpot(const FBulletMovingComponentSet& MovingComps, const FVector& Location, const FHitResult& Hit, float FloorSweepDistance, float MaxWalkSlopeCosine, bool bUseFlatBaseForFloorChecks, FBulletFloorCheckResult& OutFloorResult)
{
	OutFloorResult.Clear();

	if (!Hit.bBlockingHit)
	{
		return false;
	}

	if (Hit.bStartPenetrating)
	{
		return false;
	}

	// Reject unwalkable floor normals.
	if (!UBulletFloorQueryUtils::IsHitSurfaceWalkable(Hit, MovingComps.MoverComponent->GetUpDirection(), MaxWalkSlopeCosine))
	{
		return false;
	}

	// Make sure floor test passes here.
	UBulletFloorQueryUtils::FindFloor(MovingComps, FloorSweepDistance, MaxWalkSlopeCosine, bUseFlatBaseForFloorChecks, Location, OutFloorResult);

	if (!OutFloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

float UBulletAirMovementUtils::TryMoveToFallAlongSurface(const FBulletMovingComponentSet& MovingComps, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, float FloorSweepDistance, float MaxWalkSlopeCosine, bool bUseFlatBaseForFloorChecks, FBulletFloorCheckResult& OutFloorResult, FBulletMovementRecord& MoveRecord)
{
	OutFloorResult.Clear();

	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	float PctOfTimeUsed = 0.f;
	const FVector OldHitNormal = Normal;

	FVector SlideDelta = UBulletMovementUtils::ComputeSlideDelta(MovingComps, Delta, PctOfDeltaToMove, Normal, Hit);

	if ((SlideDelta | Delta) > 0.f)
	{
		// First sliding attempt along surface
		UBulletMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);

		PctOfTimeUsed = Hit.Time;
		if (Hit.IsValidBlockingHit())
		{
			UBulletMoverComponent* MoverComponent = MovingComps.MoverComponent.Get();
			UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();

			// Notify first impact
			if (MoverComponent && bHandleImpact)
			{
				FBulletMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
				MoverComponent->HandleImpact(ImpactParams);
			}

			// Check if we landed
			if (!IsValidLandingSpot(MovingComps, UpdatedPrimitive->GetComponentLocation(),
				Hit, FloorSweepDistance, MaxWalkSlopeCosine, bUseFlatBaseForFloorChecks, OutFloorResult))
			{
				// We've hit another surface during our first move, so let's try to slide along both of them together

				// Compute new slide normal when hitting multiple surfaces.
				SlideDelta = UBulletMovementUtils::ComputeTwoWallAdjustedDelta(MovingComps, SlideDelta, Hit, OldHitNormal);

				// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
				if (!SlideDelta.IsNearlyZero(UE::BulletMoverUtils::SMALL_MOVE_DISTANCE) && (SlideDelta | Delta) > 0.f)
				{
					// Perform second move, taking 2 walls into account
					UBulletMovementUtils::TrySafeMoveUpdatedComponent(MovingComps, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);
					PctOfTimeUsed += (Hit.Time * (1.f - PctOfTimeUsed));

					// Notify second impact
					if (MoverComponent && bHandleImpact && Hit.bBlockingHit)
					{
						FBulletMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
						MoverComponent->HandleImpact(ImpactParams);
					}

					// Check if we've landed, to acquire floor result
					IsValidLandingSpot(MovingComps, UpdatedPrimitive->GetComponentLocation(),
						Hit, FloorSweepDistance, MaxWalkSlopeCosine, bUseFlatBaseForFloorChecks, OutFloorResult);
				}
			}
		}

		return FMath::Clamp(PctOfTimeUsed, 0.f, 1.f);
	}

	return 0.f;
}


/* static */
float UBulletAirMovementUtils::TestFallingMoveAlongHitSurface(const FBulletMovingComponentSet& MovingComps, const FVector& OriginalMoveDelta, const FVector& LocationAtHit, const FQuat& TargetRotation, bool bHandleImpact, float FloorSweepDistance, float MaxWalkSlopeCosine, bool bUseFlatBaseForFloorChecks, FHitResult& InOutHit, FBulletFloorCheckResult& OutFloorResult, FBulletMovementRecord& InOutMoveRecord)
{
	OutFloorResult.Clear();

	if (!InOutHit.bBlockingHit)
	{
		return 0.f;
	}

	float PctOfTimeUsed = 0.f;
	float PctOfOrigDeltaToSlide = 1.f - InOutHit.Time;
	const FVector OrigHitNormal = InOutHit.Normal;

	FVector SlideDelta = UBulletMovementUtils::ComputeSlideDelta(MovingComps, OriginalMoveDelta, PctOfOrigDeltaToSlide, OrigHitNormal, InOutHit);

	if ((SlideDelta | OriginalMoveDelta) > 0.f)
	{
		// First sliding attempt along surface
		UBulletAsyncMovementUtils::TestDepenetratingMove(MovingComps, LocationAtHit, LocationAtHit + SlideDelta, TargetRotation, TargetRotation, /*bShouldSweep*/ true, InOutHit, InOutMoveRecord);

		PctOfTimeUsed = InOutHit.Time;

		if (InOutHit.IsValidBlockingHit())
		{
			UBulletMoverComponent* MoverComponent = MovingComps.MoverComponent.Get();
			const UPrimitiveComponent* UpdatedPrimitive = MovingComps.UpdatedPrimitive.Get();

			// Notify first impact
			if (MoverComponent && bHandleImpact)
			{
				FBulletMoverOnImpactParams ImpactParams(NAME_None, InOutHit, SlideDelta);
				MoverComponent->HandleImpact(ImpactParams);
			}

			const FVector LocationAfter1stSlide = InOutHit.TraceStart + ((InOutHit.TraceEnd - InOutHit.TraceStart) * InOutHit.Time);

			// Check if we landed
			if (!UBulletAirMovementUtils::IsValidLandingSpot(MovingComps, LocationAfter1stSlide,
				InOutHit, FloorSweepDistance, MaxWalkSlopeCosine, bUseFlatBaseForFloorChecks, OutFloorResult))
			{
				// We've hit another surface during our first move, so let's try to slide along both of them together

				// Compute new slide normal when hitting multiple surfaces.
				SlideDelta = UBulletMovementUtils::ComputeTwoWallAdjustedDelta(MovingComps, SlideDelta, InOutHit, OrigHitNormal);

				// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
				if (!SlideDelta.IsNearlyZero(UE::BulletMoverUtils::SMALL_MOVE_DISTANCE) && (SlideDelta | OriginalMoveDelta) > 0.f)
				{
					// Perform second move, taking 2 walls into account
					UBulletAsyncMovementUtils::TestDepenetratingMove(MovingComps, LocationAfter1stSlide, LocationAfter1stSlide + SlideDelta, TargetRotation, TargetRotation, /*bShouldSweep*/ true, InOutHit, InOutMoveRecord);
					PctOfTimeUsed += (InOutHit.Time * (1.f - PctOfTimeUsed));

					// Notify second impact
					if (MoverComponent && bHandleImpact && InOutHit.bBlockingHit)
					{
						FBulletMoverOnImpactParams ImpactParams(NAME_None, InOutHit, SlideDelta);
						MoverComponent->HandleImpact(ImpactParams);
					}

					const FVector LocationAfter2ndSlide = InOutHit.TraceStart + ((InOutHit.TraceEnd - InOutHit.TraceStart) * InOutHit.Time);

					// Check if we've landed, to acquire floor result
					UBulletAirMovementUtils::IsValidLandingSpot(MovingComps, LocationAfter2ndSlide,
						InOutHit, FloorSweepDistance, MaxWalkSlopeCosine, bUseFlatBaseForFloorChecks, OutFloorResult);
				}
			}
		}

		return FMath::Clamp(PctOfTimeUsed, 0.f, 1.f);
	}

	return 0.f;

}

