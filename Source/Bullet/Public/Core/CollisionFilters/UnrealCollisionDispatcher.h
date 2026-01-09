// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BulletCollision/CollisionDispatch/btCollisionDispatcher.h"
#include "BulletDynamics/Dynamics/btRigidBody.h"
#include "Core/Libraries/BulletLibrary.h"

/**
 * 
 */

class FUnrealCollisionDispatcher : public btCollisionDispatcher
{
public:
	explicit FUnrealCollisionDispatcher(btCollisionConfiguration* collisionConfiguration)
		: btCollisionDispatcher(collisionConfiguration)
	{
	}
	
	virtual bool needsCollision(const btCollisionObject* body0, const btCollisionObject* body1) override
	{
	
		const bool Super = btCollisionDispatcher::needsCollision(body0, body1);
	
		if (!body0 || !body1) return Super;

		const bool Result = BulletHelpers::IsBlockingCollisionAllowed(body0, body1);
		
		return Result;
	}
};
