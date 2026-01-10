// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BulletCollision/BroadphaseCollision/btOverlappingPairCache.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "Core/Libraries/BulletLibrary.h"

/**
 * 
 */
class FBulletOverlappingPairCache : public btHashedOverlappingPairCache
{
	virtual btBroadphasePair* addOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) override
	{
		const int CurrentNum = getNumOverlappingPairs();
		btBroadphasePair* Result = btHashedOverlappingPairCache::addOverlappingPair(proxy0, proxy1);
		const int newNum = getNumOverlappingPairs();
		
		if (newNum > CurrentNum)
		{
			// we have new overlaps
			btCollisionObject* colObj0 = (btCollisionObject*)proxy0->m_clientObject;
			btCollisionObject* colObj1 = (btCollisionObject*)proxy1->m_clientObject;
			
			if (!BulletHelpers::IsBlockingCollisionAllowed(colObj0, colObj1)) return Result;
			
			UPrimitiveComponent* ThisComp = static_cast<UPrimitiveComponent*>(colObj0->getUserPointer());
			UPrimitiveComponent* ThatComp = static_cast<UPrimitiveComponent*>(colObj1->getUserPointer());

			ThisComp->OnComponentHit.Broadcast(ThisComp, ThatComp->GetOwner(), ThatComp, ThatComp->GetComponentVelocity().GetSafeNormal(), FHitResult());
			ThatComp->OnComponentHit.Broadcast(ThatComp, ThisComp->GetOwner(), ThisComp, ThisComp->GetComponentVelocity().GetSafeNormal(), FHitResult());
			// we only want to handle blocking collisions here, ghosts overlaps can be handled in ghost overlap pairing cache.
		}
		
		return Result; 
	};
	virtual void* removeOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1, btDispatcher* dispatcher) override
	{
		return btHashedOverlappingPairCache::removeOverlappingPair(proxy0, proxy1, dispatcher);
	};
};
