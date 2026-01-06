// Copyright Epic Games, Inc. All Rights Reserved.


#include "MoveLibrary/BulletMoverBlackboard.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMoverBlackboard)



void UBulletMoverBlackboard::Invalidate(FName ObjName)
{
	UE::TWriteScopeLock Lock(ObjectsMapLock);
	ObjectsByName.Remove(ObjName);
}

void UBulletMoverBlackboard::Invalidate(EBulletInvalidationReason Reason)
{
	switch (Reason)
	{
		default:
		case EBulletInvalidationReason::FullReset:
		{
			UE::TWriteScopeLock Lock(ObjectsMapLock);
			ObjectsByName.Empty();
		}
		break;

		// TODO: Support other reasons
	}
}

void UBulletMoverBlackboard::BeginDestroy()
{
	InvalidateAll();
	Super::BeginDestroy();
}
