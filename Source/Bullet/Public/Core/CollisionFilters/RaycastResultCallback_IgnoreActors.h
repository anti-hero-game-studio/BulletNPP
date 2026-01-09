// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BulletMain.h"
#include "BulletCollision/CollisionDispatch/btCollisionWorld.h"
#include "Core/Libraries/BulletLibrary.h"
#include "CoreMinimal.h"

struct btClosestNotMeRaycastResultCallback : public btCollisionWorld::ClosestRayResultCallback
{
	public:
	btClosestNotMeRaycastResultCallback(const btCollisionObjectArray* InMe, const btVector3& From, const btVector3& To, const TEnumAsByte<ECollisionChannel>& InCollisionChannel)
		: btCollisionWorld::ClosestRayResultCallback(From, To), Me(InMe), CollisionChannel(InCollisionChannel)
	{
	}

	virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
	{
		for (int i = 0; i < Me->size(); ++i)
		{
			btCollisionObject* Obj = Me->at(i);
			if (rayResult.m_collisionObject == Obj) return btScalar(1.0);
		}
		
		if (!BulletHelpers::IsBlockingCollisionAllowed(CollisionChannel, rayResult.m_collisionObject)) return btScalar(1.0);
		
		if (!rayResult.m_collisionObject->hasContactResponse())
			return btScalar(1.0);
		
		return ClosestRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
	}

protected:
	const btCollisionObjectArray* Me = nullptr;
	TEnumAsByte<ECollisionChannel> CollisionChannel;
};

struct btAllNotMeRaycastResultCallback : public btCollisionWorld::AllHitsRayResultCallback
{
public:
	btAllNotMeRaycastResultCallback(const btCollisionObjectArray* InMe, const btVector3& From, const btVector3& To, const TEnumAsByte<ECollisionChannel>& InCollisionChannel)
		: btCollisionWorld::AllHitsRayResultCallback(From, To), Me(InMe), CollisionChannel(InCollisionChannel)
	{
	}

	virtual btScalar addSingleResult(btCollisionWorld::LocalRayResult& rayResult, bool normalInWorldSpace)
	{
		for (int i = 0; i < Me->size(); ++i)
		{
			btCollisionObject* Obj = Me->at(i);
			if (rayResult.m_collisionObject == Obj) return btScalar(1.0);
		}
		
		if (!BulletHelpers::IsBlockingCollisionAllowed(CollisionChannel, rayResult.m_collisionObject)) return btScalar(1.0);
		
		if (!rayResult.m_collisionObject->hasContactResponse())
			return btScalar(1.0);
		
		return AllHitsRayResultCallback::addSingleResult(rayResult, normalInWorldSpace);
	}

protected:
	const btCollisionObjectArray* Me = nullptr;
	TEnumAsByte<ECollisionChannel> CollisionChannel;
};
