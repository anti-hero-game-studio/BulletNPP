// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "BulletMain.h"
#include "BulletCollision/CollisionDispatch/btCollisionWorld.h"
#include "Core/Libraries/BulletLibrary.h"

struct btClosestNotMeConvexResultCallback : public btCollisionWorld::ClosestConvexResultCallback
{
	btClosestNotMeConvexResultCallback(const btCollisionObjectArray* InMe, const btVector3& From, const btVector3& To, const TEnumAsByte<ECollisionChannel>& InCollisionChannel)
		: btCollisionWorld::ClosestConvexResultCallback(From, To), Me(InMe), CollisionChannel(InCollisionChannel)
	{
	}

	virtual btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult, bool normalInWorldSpace)
	{
		for (int i = 0; i < Me->size(); ++i)
		{
			btCollisionObject* Obj = Me->at(i);
			if (convexResult.m_hitCollisionObject == Obj) return btScalar(1.0);
		}
		
		if (!convexResult.m_hitCollisionObject->hasContactResponse()) return btScalar(1.0);
		
		if (!BulletHelpers::IsBlockingCollisionAllowed(CollisionChannel, convexResult.m_hitCollisionObject)) return btScalar(1.0);
		
		return ClosestConvexResultCallback::addSingleResult(convexResult, normalInWorldSpace);
	}

protected:
	const btCollisionObjectArray* Me = nullptr;
	TEnumAsByte<ECollisionChannel> CollisionChannel;
};

struct btAllNotMeConvexResultCallback : public btCollisionWorld::ConvexResultCallback
{
	btAllNotMeConvexResultCallback(const btCollisionObjectArray* InMe, const btVector3& rayFromWorld, const btVector3& rayToWorld, const TEnumAsByte<ECollisionChannel>& InCollisionChannel)
				: m_rayFromWorld(rayFromWorld),
				  m_rayToWorld(rayToWorld),
				Me(InMe), CollisionChannel(InCollisionChannel)
	{
	}

	btAlignedObjectArray<const btCollisionObject*> m_collisionObjects;

	btVector3 m_rayFromWorld;  //used to calculate hitPointWorld from hitFraction
	btVector3 m_rayToWorld;

	btAlignedObjectArray<btVector3> m_hitNormalWorld;
	btAlignedObjectArray<btVector3> m_hitPointWorld;
	btAlignedObjectArray<btScalar> m_hitFractions;

	virtual btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult, bool normalInWorldSpace)
	{
		
		for (int i = 0; i < Me->size(); ++i)
		{
			btCollisionObject* Obj = Me->at(i);
			if (convexResult.m_hitCollisionObject == Obj)
				return btScalar(1.0);
		}
		
		if (!convexResult.m_hitCollisionObject->hasContactResponse()) return btScalar(1.0);
		
		if (!BulletHelpers::IsBlockingCollisionAllowed(CollisionChannel, convexResult.m_hitCollisionObject)) return btScalar(1.0);
		
		HitCollisionObject = convexResult.m_hitCollisionObject;
		m_collisionObjects.push_back(convexResult.m_hitCollisionObject);
		btVector3 hitNormalWorld;
		if (normalInWorldSpace)
		{
			hitNormalWorld = convexResult.m_hitNormalLocal;
		}
		else
		{
			///need to transform normal into worldspace
			hitNormalWorld = HitCollisionObject->getWorldTransform().getBasis() * convexResult.m_hitNormalLocal;
		}
		m_hitNormalWorld.push_back(hitNormalWorld);
		btVector3 hitPointWorld;
		hitPointWorld.setInterpolate3(m_rayFromWorld, m_rayToWorld, convexResult.m_hitFraction);
		m_hitPointWorld.push_back(hitPointWorld);
		m_hitFractions.push_back(convexResult.m_hitFraction);
		return m_closestHitFraction;
	}

protected:
	const btCollisionObjectArray* Me = nullptr;
	const btCollisionObject* HitCollisionObject = nullptr;
	TEnumAsByte<ECollisionChannel> CollisionChannel;
	
};


