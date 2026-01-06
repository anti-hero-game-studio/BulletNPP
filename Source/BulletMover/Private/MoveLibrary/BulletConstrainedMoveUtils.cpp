// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/BulletConstrainedMoveUtils.h"
#include "BulletMoverLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletConstrainedMoveUtils)


void UBulletPlanarConstraintUtils::SetPlanarConstraintEnabled(UPARAM(ref) FBulletPlanarConstraint& Constraint, bool bEnabled)
{
	Constraint.bConstrainToPlane = bEnabled;
}


void UBulletPlanarConstraintUtils::SetPlanarConstraintNormal(UPARAM(ref) FBulletPlanarConstraint& Constraint, FVector PlaneNormal)
{
	PlaneNormal = PlaneNormal.GetSafeNormal();

	if (PlaneNormal.IsNearlyZero())
	{
		UE_LOG(LogBulletMover, Warning, TEXT("Can't use SetPlanarConstraintNormal with a zero-length normal. Leaving normal as %s"), *Constraint.PlaneConstraintNormal.ToCompactString());
	}
	else
	{
		Constraint.PlaneConstraintNormal = PlaneNormal;
	}
}


void UBulletPlanarConstraintUtils::SetPlaneConstraintOrigin(UPARAM(ref) FBulletPlanarConstraint& Constraint, FVector PlaneOrigin)
{
	Constraint.PlaneConstraintOrigin = PlaneOrigin;
}


FVector UBulletPlanarConstraintUtils::ConstrainDirectionToPlane(const FBulletPlanarConstraint& Constraint, FVector Direction, bool bMaintainMagnitude)
{
	if (Constraint.bConstrainToPlane)
	{
		float OrigSize = Direction.Size();

		Direction = FVector::VectorPlaneProject(Direction, Constraint.PlaneConstraintNormal);

		if (bMaintainMagnitude)
		{
			Direction = Direction.GetSafeNormal() * OrigSize;
		}
	}

	return Direction;
}

FVector UBulletPlanarConstraintUtils::ConstrainLocationToPlane(const FBulletPlanarConstraint& Constraint, FVector Location)
{
	if (Constraint.bConstrainToPlane)
	{
		Location = FVector::PointPlaneProject(Location, Constraint.PlaneConstraintOrigin, Constraint.PlaneConstraintNormal);
	}

	return Location;
}

FVector UBulletPlanarConstraintUtils::ConstrainNormalToPlane(const FBulletPlanarConstraint& Constraint, FVector Normal)
{
	if (Constraint.bConstrainToPlane)
	{
		Normal = FVector::VectorPlaneProject(Normal, Constraint.PlaneConstraintNormal).GetSafeNormal();
	}

	return Normal;
}
