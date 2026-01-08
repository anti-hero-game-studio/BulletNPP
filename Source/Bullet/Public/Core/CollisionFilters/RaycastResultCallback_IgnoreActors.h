// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BulletMain.h"

class btClosestNotMeRaycastResultCallback : public btCollisionWorld::ClosestRayResultCallback
{
public:
	btClosestNotMeRaycastResultCallback(const btCollisionObjectArray* InMe, const btVector3& From, const btVector3& To)
		: btCollisionWorld::ClosestRayResultCallback(From, To), Me(InMe)
	{
	}

	virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
	{
		for (int i = 0; i < Me->size(); ++i)
		{
			btCollisionObject* Obj = Me->at(i);
			if (rayResult.m_collisionObject == Obj)
				return btScalar(1.0);
		}
		
		if (!rayResult.m_collisionObject->hasContactResponse())
			return btScalar(1.0);
		
		return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
	}

protected:
	const btCollisionObjectArray* Me = nullptr;
};
