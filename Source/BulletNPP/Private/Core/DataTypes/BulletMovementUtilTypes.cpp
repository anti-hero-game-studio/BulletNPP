// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/DataTypes/BulletMovementUtilTypes.h"

#include "Core/Simulation/BulletPhysicsEngineSimComp.h"


void FBulletMovingComponentSet::SetFrom(USceneComponent* InUpdatedComponent)
{
	UpdatedComponent = InUpdatedComponent;

	if (UpdatedComponent.IsValid())
	{
		UpdatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
		BulletPhysicsComponent = UpdatedComponent->GetOwner()->FindComponentByClass<UBulletPhysicsEngineSimComp>();

		checkf(!BulletPhysicsComponent.IsValid() || UpdatedComponent == BulletPhysicsComponent->GetUpdatedComponent(), TEXT("Expected MoverComponent to have the same UpdatedComponent"));
	}
}

void FBulletMovingComponentSet::SetFrom(UBulletPhysicsEngineSimComp* InMoverComponent)
{
	BulletPhysicsComponent = InMoverComponent;

	if (BulletPhysicsComponent.IsValid())
	{
		UpdatedComponent = BulletPhysicsComponent->GetUpdatedComponent();
		UpdatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	}
}


static const FName DefaultCollisionTraceTag = "SweepTestMoverComponent";

FBulletCollisionParams::FBulletCollisionParams(const USceneComponent* SceneComp)
{
	if (const UPrimitiveComponent* AsPrimitive = Cast<const UPrimitiveComponent>(SceneComp))
	{
		SetFromPrimitiveComponent(AsPrimitive);
	}
	else
	{
		// TODO: set up a line trace if SceneComp is not a primitive component
		ensureMsgf(0, TEXT("Support for non-primitive components is not yet implemented"));
	}
}

void FBulletCollisionParams::SetFromPrimitiveComponent(const UPrimitiveComponent* PrimitiveComp)
{
	Channel = PrimitiveComp->GetCollisionObjectType();

	Shape = PrimitiveComp->GetCollisionShape();

	PrimitiveComp->InitSweepCollisionParams(QueryParams, ResponseParams);

	const AActor* OwningActor = PrimitiveComp->GetOwner();
	
	QueryParams.TraceTag = DefaultCollisionTraceTag;
	QueryParams.OwnerTag = OwningActor->GetFName();
	QueryParams.AddIgnoredActor(OwningActor);
}
