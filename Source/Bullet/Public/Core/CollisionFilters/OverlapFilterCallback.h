// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BulletCollision/BroadphaseCollision/btOverlappingPairCache.h"
#include "BulletCollision/CollisionDispatch/btCollisionObject.h"
#include "Core/Libraries/BulletLibrary.h"


struct FBulletOverlapFilterCallback : public btOverlapFilterCallback
{
	
	virtual bool needBroadphaseCollision(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_OVRLP_FILTER);
		TRACE_CPUPROFILER_EVENT_SCOPE(FBulletOverlapFilterCallback::NeedBroadphaseCollision);
		// Preserve Bullet’s standard group/mask filtering.
		const bool bBulletMaskPass =
			(proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0 &&
			(proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask) != 0;

		if (!bBulletMaskPass)
		{
			return false;
		}

		const btCollisionObject* ObjA = static_cast<const btCollisionObject*>(proxy0->m_clientObject);
		const btCollisionObject* ObjB = static_cast<const btCollisionObject*>(proxy1->m_clientObject);
		if ((!ObjA || !ObjB) || (ObjA == ObjB))
		{
			return false;
		}
		
		return BulletHelpers::IsAnyCollisionAllowed(ObjA, ObjB);
	}
};
