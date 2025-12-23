// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Simulation/BulletBlackboard.h"


void UBulletBlackboard::Invalidate(FName ObjName)
{
	UE::TWriteScopeLock Lock(ObjectsMapLock);
	ObjectsByName.Remove(ObjName);
}

void UBulletBlackboard::Invalidate(BulletPhysicsEngine::EInvalidationReason Reason)
{
	switch (Reason)
	{
	default:
	case BulletPhysicsEngine::EInvalidationReason::FullReset:
		{
			UE::TWriteScopeLock Lock(ObjectsMapLock);
			ObjectsByName.Empty();
		}
		break;

		// TODO: Support other reasons
	}
}

void UBulletBlackboard::BeginDestroy()
{
	InvalidateAll();
	Super::BeginDestroy();
}