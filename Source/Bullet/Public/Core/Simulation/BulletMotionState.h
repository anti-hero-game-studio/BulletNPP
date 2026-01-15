// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletMain.h"
#include "Core/Interfaces/BulletActorInterface.h"
#include "Core/Libraries/BulletLibrary.h"


class BULLET_API FBulletMotionState : public btMotionState
{
	
protected:
		TWeakObjectPtr<USceneComponent> UpdatedComponent;
		TWeakObjectPtr<USceneComponent> VisualComponent;
		// Bullet is made local so that all sims are close to origin
		// This world origin must be in *UE dimensions*
		FVector WorldOrigin;
		btTransform CenterOfMassTransform;
		FTransform FinalTransform;
		FTransform BaseVisualComponentTransform;


	public:
		FBulletMotionState()
		{

		}

		FBulletMotionState(const AActor* ParentActor, const FVector& WorldCentre, const btTransform& CenterOfMassOffset = btTransform::getIdentity())
		{
			
			const IBulletActorInterface* I = Cast<IBulletActorInterface>(ParentActor);
			if (ParentActor->Implements<UBulletActorInterface>())
			{
				if (UPrimitiveComponent* P  = IBulletActorInterface::Execute_GetVisualProxyRootComponent(ParentActor))
				{
					BaseVisualComponentTransform = P->GetRelativeTransform();
					VisualComponent = P; 
				}
				
					
			}
			
			UpdatedComponent = ParentActor->GetRootComponent(); //TODO:@GreggoryAddison::CodeUpgrade | This needs to be more dynamic to allow for runtime changes.
			WorldOrigin=WorldCentre;
			CenterOfMassTransform=CenterOfMassOffset;
		}

		///synchronizes world transform from UE to physics (typically only called at start)
		void getWorldTransform(btTransform& OutCenterOfMassWorldTrans) const override
		{
			if (UpdatedComponent.IsValid())
			{
				OutCenterOfMassWorldTrans = BulletHelpers::ToBulletTransform(UpdatedComponent->GetComponentTransform(), WorldOrigin) * CenterOfMassTransform.inverse();
			}

		}

		///synchronizes world transform from physics to UE
		void setWorldTransform(const btTransform& CenterOfMassWorldTrans) override
		{// send this to actor
			QUICK_SCOPE_CYCLE_COUNTER(STAT_BNP_TICK_FIXED);
			TRACE_CPUPROFILER_EVENT_SCOPE(BulletMotionState::SetUpdatedComponentTransform);
			if (UpdatedComponent.IsValid(false))
			{
				FinalTransform = BulletHelpers::ToUnrealTransform(CenterOfMassWorldTrans * CenterOfMassTransform, WorldOrigin);
				FinalTransform.SetScale3D(UpdatedComponent->GetComponentScale());

				if (!UpdatedComponent.Get()->GetComponentTransform().Equals(FinalTransform))
				{
					UpdatedComponent->SetWorldTransform(FinalTransform);
				}
				
				
			}
		}
	
		FTransform GetFinalTransform() const
		{
			return FinalTransform;
		}
};


class BULLET_API FBulletUEMotionState: public btMotionState
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
				OutCenterOfMassWorldTrans = BulletHelpers::ToBulletTransform(Parent->GetComponentTransform(), WorldOrigin)*CenterOfMassTransform.inverse();
			}
		}

		///synchronizes world transform from physics to UE
		void setWorldTransform(const btTransform& CenterOfMassWorldTrans) override
		{// send this to actor
			if (Parent.IsValid(false))
			{
				btTransform GraphicTrans = CenterOfMassWorldTrans * CenterOfMassTransform;
				Parent->SetWorldTransform(LocalTransform.Inverse()* BulletHelpers::ToUnrealTransform(GraphicTrans, WorldOrigin));
			}
		}
};
