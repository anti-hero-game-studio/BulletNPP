// Fill out your copyright notice in the Description page of Project Settings.


#include "MoveLibrary/BulletPhysicsGroundMovementUtils.h"
#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"

FVector UBulletPhysicsGroundMovementUtils::ComputeLocalGroundVelocity_Internal(const UObject* WorldContextObject,
const FVector& Position, const FBulletFloorCheckResult& FloorResult)
{

	FVector GroundVelocity = FVector::ZeroVector;
	
	if (!WorldContextObject) return GroundVelocity;

	UBulletPhysicsWorldSubsystem* Subsystem = WorldContextObject->GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	if (!Subsystem) return GroundVelocity;
	
	if (const btRigidBody* Rigid = Subsystem->GetRigidBody(FloorResult.HitResult))
	{
		const FTransform ComTransform = BulletHelpers::ToUnrealTransform(Rigid->getCenterOfMassTransform(), FVector(0));
		FVector Offset = Position - ComTransform.GetLocation();
		Offset -= Offset.ProjectOnToNormal(FloorResult.HitResult.ImpactNormal);
		GroundVelocity = BulletHelpers::ToUnrealVector3(Rigid->getLinearVelocity()) + BulletHelpers::ToUnrealVector3(Rigid->getAngularVelocity()).Cross(Offset);
	}
	return GroundVelocity;
}
