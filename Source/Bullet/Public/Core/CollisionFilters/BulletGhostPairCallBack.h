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
				UPrimitiveComponent* ThisComp = static_cast<UPrimitiveComponent*>(colObj0->getUserPointer());
				UPrimitiveComponent* ThatComp = static_cast<UPrimitiveComponent*>(colObj1->getUserPointer());
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
				UPrimitiveComponent* ThisComp = static_cast<UPrimitiveComponent*>(colObj1->getUserPointer());
				UPrimitiveComponent* ThatComp = static_cast<UPrimitiveComponent*>(colObj0->getUserPointer());
				
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
				UPrimitiveComponent* ThisComp = static_cast<UPrimitiveComponent*>(colObj0->getUserPointer());
				UPrimitiveComponent* ThatComp = static_cast<UPrimitiveComponent*>(colObj1->getUserPointer());
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
				UPrimitiveComponent* ThisComp = static_cast<UPrimitiveComponent*>(colObj1->getUserPointer());
				UPrimitiveComponent* ThatComp = static_cast<UPrimitiveComponent*>(colObj0->getUserPointer());
				ThisComp->OnComponentEndOverlap.Broadcast(ThisComp, ThatComp->GetOwner(), ThatComp, colObj0->getWorldArrayIndex());
			}
		}
			
		
		
		return 0;
	};
};
