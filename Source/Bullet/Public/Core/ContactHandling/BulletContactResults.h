// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BulletContactGatherer.h"
#include "BulletMain.h"
#include "Core/Libraries/BulletLibrary.h"

/**
 * 
 */
struct BULLET_API FBulletContactResult_FirstHit : btCollisionWorld::ContactResultCallback
{
	
	virtual btScalar addSingleResult(btManifoldPoint& Pt, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1) override
	{
		if (!colObj0Wrap || !colObj1Wrap) return 0.f;
		FBulletHitEvent Event;
		Event.ImpactPoint = BulletHelpers::ToUnrealPosition(Pt.getPositionWorldOnA(), FVector::ZeroVector);
		Event.ImpactNormal = BulletHelpers::ToUnrealNormal(Pt.m_normalWorldOnB);
		Event.PenetrationDepth = BulletHelpers::ToUnrealFloat(Pt.getDistance());
		Event.AppliedImpulse = BulletHelpers::ToUnrealFloat(Pt.getAppliedImpulse());
		Event.ImpulseDir = BulletHelpers::ToUnrealNormal(Pt.m_normalWorldOnB);
		return 1.0f;
	}
	
	TArray<FBulletHitEvent> Results;
};
