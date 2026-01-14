// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BulletCollision/CollisionDispatch/btGhostObject.h"

/**
 * 
 */
class BULLET_API FBulletGhostPairCallBack : public btGhostPairCallback
{
	virtual btBroadphasePair* addOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBulletGhostPairCallBack::AddOverlappingPair);
		btCollisionObject* colObj0 = (btCollisionObject*)proxy0->m_clientObject;
		btCollisionObject* colObj1 = (btCollisionObject*)proxy1->m_clientObject;
		btGhostObject* ghost0 = btGhostObject::upcast(colObj0);
		btGhostObject* ghost1 = btGhostObject::upcast(colObj1);
		if (ghost0)
		{
			const int CurrentNum = ghost0->getNumOverlappingObjects();
			ghost0->addOverlappingObjectInternal(proxy1, proxy0);
			const int NewNum = ghost0->getNumOverlappingObjects();
			if (NewNum > CurrentNum)
			{
				const FBulletUserData* UD0 = BulletHelpers::GetUserData(colObj0);
				const FBulletUserData* UD1 = BulletHelpers::GetUserData(colObj1);
				
				if (!UD0 || !UD1) return 0;
				
				
				UPrimitiveComponent* ThisComp = Cast<UPrimitiveComponent>(UD0->Component);
				UPrimitiveComponent* ThatComp = Cast<UPrimitiveComponent>(UD1->Component);
				
				if (!ThisComp || !ThatComp) return 0;
				
				ThisComp->OnComponentBeginOverlap.Broadcast(ThisComp, ThatComp->GetOwner(), ThatComp, colObj1->getWorldArrayIndex(), false, FHitResult());
			}
			
		}
		if (ghost1)
		{
			const int CurrentNum = ghost1->getNumOverlappingObjects();
			ghost1->addOverlappingObjectInternal(proxy1, proxy0);
			const int NewNum = ghost1->getNumOverlappingObjects();
			if (NewNum > CurrentNum)
			{
				const FBulletUserData* UD0 = BulletHelpers::GetUserData(colObj1);
				const FBulletUserData* UD1 = BulletHelpers::GetUserData(colObj0);
				
				if (!UD0 || !UD1) return 0;
				
				
				UPrimitiveComponent* ThisComp = Cast<UPrimitiveComponent>(UD0->Component);
				UPrimitiveComponent* ThatComp = Cast<UPrimitiveComponent>(UD1->Component);
				
				if (!ThisComp || !ThatComp) return 0;
				
				ThisComp->OnComponentBeginOverlap.Broadcast(ThisComp, ThatComp->GetOwner(), ThatComp, colObj0->getWorldArrayIndex(), false, FHitResult());
			}
			
		}
		
		return 0; 
	};
	virtual void* removeOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1, btDispatcher* dispatcher) override
	{
		btCollisionObject* colObj0 = (btCollisionObject*)proxy0->m_clientObject;
		btCollisionObject* colObj1 = (btCollisionObject*)proxy1->m_clientObject;
		btGhostObject* ghost0 = btGhostObject::upcast(colObj0);
		btGhostObject* ghost1 = btGhostObject::upcast(colObj1);
		if (ghost0)
		{
			
			const int CurrentNum = ghost0->getNumOverlappingObjects();
			ghost0->removeOverlappingObjectInternal(proxy1, dispatcher, proxy0);
			const int NewNum = ghost0->getNumOverlappingObjects();
			if (NewNum < CurrentNum)
			{
				
				const FBulletUserData* UD0 = BulletHelpers::GetUserData(colObj0);
				const FBulletUserData* UD1 = BulletHelpers::GetUserData(colObj1);
				
				if (!UD0 || !UD1) return 0;
				
				
				UPrimitiveComponent* ThisComp = Cast<UPrimitiveComponent>(UD0->Component);
				UPrimitiveComponent* ThatComp = Cast<UPrimitiveComponent>(UD1->Component);
				
				if (!ThisComp || !ThatComp) return 0;
				
				ThisComp->OnComponentEndOverlap.Broadcast(ThisComp, ThatComp->GetOwner(), ThatComp, colObj1->getWorldArrayIndex());
				
				
			}
		}
		if (ghost1)
		{
			const int CurrentNum = ghost1->getNumOverlappingObjects();
			ghost1->removeOverlappingObjectInternal(proxy1, dispatcher, proxy0);
			const int NewNum = ghost1->getNumOverlappingObjects();
			if (NewNum < CurrentNum)
			{
				const FBulletUserData* UD0 = BulletHelpers::GetUserData(colObj1);
				const FBulletUserData* UD1 = BulletHelpers::GetUserData(colObj0);
				
				if (!UD0 || !UD1) return 0;
				
				
				UPrimitiveComponent* ThisComp = Cast<UPrimitiveComponent>(UD0->Component);
				UPrimitiveComponent* ThatComp = Cast<UPrimitiveComponent>(UD1->Component);
				
				if (!ThisComp || !ThatComp) return 0;
				
				ThisComp->OnComponentEndOverlap.Broadcast(ThisComp, ThatComp->GetOwner(), ThatComp, colObj1->getWorldArrayIndex());
			}
		}
			
		
		
		return 0;
	};
};
