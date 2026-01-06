// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/BulletMovementRecordUtils.h"
#include "MoveLibrary/BulletMovementRecord.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMovementRecordUtils)

void UBulletMovementRecordUtils::K2_SetDeltaSeconds(FBulletMovementRecord& OutMovementRecord, float DeltaSeconds)
{
	OutMovementRecord.SetDeltaSeconds(DeltaSeconds);
}

const FVector& UBulletMovementRecordUtils::K2_GetTotalMoveDelta(const FBulletMovementRecord& MovementRecord)
{
	return MovementRecord.GetTotalMoveDelta();
}

const FVector& UBulletMovementRecordUtils::K2_GetRelevantMoveDelta(const FBulletMovementRecord& MovementRecord)
{
	return MovementRecord.GetRelevantMoveDelta();
}

FVector UBulletMovementRecordUtils::K2_GetRelevantVelocity(const FBulletMovementRecord& MovementRecord)
{
	return MovementRecord.GetRelevantVelocity();
}	
