// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletMain.h"
#include "Core/Libraries/BulletMathLibrary.h"


class BULLETNPP_API FBulletMotionState : public btMotionState
{
	
protected:
		TWeakObjectPtr<USceneComponent> UpdatedComponent;
		// Bullet is made local so that all sims are close to origin
		// This world origin must be in *UE dimensions*
		FVector WorldOrigin;
		btTransform CenterOfMassTransform;


	public:
		FBulletMotionState()
		{

		}

		FBulletMotionState(AActor* ParentActor, const FVector& WorldCentre, const btTransform& CenterOfMassOffset = btTransform::getIdentity())
		{
			UpdatedComponent=ParentActor->GetRootComponent(); //TODO:@GreggoryAddison::CodeUpgrade | This needs to be more dynamic to allow for runtime changes.
			WorldOrigin=WorldCentre;
			CenterOfMassTransform=CenterOfMassOffset;
		}

		///synchronizes world transform from UE to physics (typically only called at start)
		void getWorldTransform(btTransform& OutCenterOfMassWorldTrans) const override
		{
			if (UpdatedComponent.IsValid())
			{
				auto&& Xform = UpdatedComponent->GetComponentTransform();
				OutCenterOfMassWorldTrans = BulletHelpers::ToBt(UpdatedComponent->GetComponentTransform(), WorldOrigin) * CenterOfMassTransform.inverse();
			}

		}

		///synchronizes world transform from physics to UE
		void setWorldTransform(const btTransform& CenterOfMassWorldTrans) override
		{// send this to actor
			if (UpdatedComponent.IsValid(false))
			{
				btTransform GraphicTrans = CenterOfMassWorldTrans * CenterOfMassTransform;
				UpdatedComponent->SetWorldTransform(BulletHelpers::ToUE(GraphicTrans, WorldOrigin));
			}
		}
};


class BULLETNPP_API FBulletUEMotionState: public btMotionState
{
	protected:
		TWeakObjectPtr<USkeletalMeshComponent> Parent;
		// Bullet is made local so that all sims are close to origin
		// This world origin must be in *UE dimensions*
		FVector WorldOrigin;
		FTransform LocalTransform;
		btTransform CenterOfMassTransform;


	public:
		FBulletUEMotionState()
		{

		}
		FBulletUEMotionState(
				USkeletalMeshComponent* ParentActor,
				const FVector& WorldCentre,
				const FTransform& localTransform,
				const btTransform& CenterOfMassOffset = btTransform::getIdentity()
				):
			Parent(ParentActor),
			WorldOrigin(WorldCentre),LocalTransform(localTransform),
			CenterOfMassTransform(CenterOfMassOffset)
		{}

		///synchronizes world transform from UE to physics (typically only called at start)
		void getWorldTransform(btTransform& OutCenterOfMassWorldTrans) const override
		{
			if (Parent.IsValid())
			{
				OutCenterOfMassWorldTrans = BulletHelpers::ToBt(Parent->GetComponentTransform(), WorldOrigin)*CenterOfMassTransform.inverse();
			}
		}

		///synchronizes world transform from physics to UE
		void setWorldTransform(const btTransform& CenterOfMassWorldTrans) override
		{// send this to actor
			if (Parent.IsValid(false))
			{
				btTransform GraphicTrans = CenterOfMassWorldTrans * CenterOfMassTransform;
				Parent->SetWorldTransform(LocalTransform.Inverse()* BulletHelpers::ToUE(GraphicTrans, WorldOrigin));
			}
		}
};
