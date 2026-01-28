// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/BulletMovementUtilsTypes.h"
#include "Components/PrimitiveComponent.h"
#include "BulletMoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMovementUtilsTypes)



void FBulletMovingComponentSet::SetFrom(USceneComponent* InUpdatedComponent)
{
	UpdatedComponent = InUpdatedComponent;

	if (UpdatedComponent.IsValid())
	{
		UpdatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
		MoverComponent = UpdatedComponent->GetOwner()->FindComponentByClass<UBulletMoverComponent>();

		checkf(!MoverComponent.IsValid() || UpdatedComponent == MoverComponent->GetUpdatedComponent(), TEXT("Expected MoverComponent to have the same UpdatedComponent"));
	}
}

void FBulletMovingComponentSet::SetFrom(UBulletMoverComponent* InMoverComponent)
{
	MoverComponent = InMoverComponent;

	if (MoverComponent.IsValid())
	{
		UpdatedComponent = MoverComponent->GetUpdatedComponent();
		UpdatedPrimitive = MoverComponent->GetUpdatedPrimitive();
	}
}


static const FName DefaultCollisionTraceTag = "SweepTestMoverComponent";

FBulletMoverCollisionParams::FBulletMoverCollisionParams(const USceneComponent* SceneComp)
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

void FBulletMoverCollisionParams::SetFromPrimitiveComponent(const UPrimitiveComponent* PrimitiveComp)
{
	Channel = PrimitiveComp->GetCollisionObjectType();

	Shape = PrimitiveComp->GetCollisionShape();

	PrimitiveComp->InitSweepCollisionParams(QueryParams, ResponseParams);

	const AActor* OwningActor = PrimitiveComp->GetOwner();
	
	QueryParams.TraceTag = DefaultCollisionTraceTag;
	QueryParams.OwnerTag = OwningActor->GetFName();
	QueryParams.AddIgnoredActor(OwningActor);
}
