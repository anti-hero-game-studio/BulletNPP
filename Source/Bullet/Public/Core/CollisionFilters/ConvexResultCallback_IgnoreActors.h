// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "BulletMain.h"

class btClosestNotMeConvexResultCallback : public btCollisionWorld::ClosestConvexResultCallback
{
public:
	btClosestNotMeConvexResultCallback(const btCollisionObjectArray* InMe, const btVector3& From, const btVector3& To)
		: btCollisionWorld::ClosestConvexResultCallback(From, To), Me(InMe)
	{
	}

	virtual btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult, bool normalInWorldSpace)
	{
		for (int i = 0; i < Me->size(); ++i)
		{
			btCollisionObject* Obj = Me->at(i);
			if (convexResult.m_hitCollisionObject == Obj)
				return btScalar(1.0);
		}
		
		if (!convexResult.m_hitCollisionObject->hasContactResponse())
			return btScalar(1.0);
		
		return ClosestConvexResultCallback::addSingleResult(convexResult, normalInWorldSpace);
	}

protected:
	const btCollisionObjectArray* Me = nullptr;
};

