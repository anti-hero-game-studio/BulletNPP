// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverSimulation.h"

#include "MoveLibrary/BulletMoverBlackboard.h"
#include "MoveLibrary/BulletRollbackBlackboard.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMoverSimulation)

UBulletMoverSimulation::UBulletMoverSimulation()
{
	Blackboard = CreateDefaultSubobject<UBulletMoverBlackboard>(TEXT("BulletMoverSimulationBlackboard"));
}

const UBulletMoverBlackboard* UBulletMoverSimulation::GetBlackboard() const
{
	return Blackboard;
}

UBulletMoverBlackboard* UBulletMoverSimulation::GetBlackboard_Mutable()
{
	return Blackboard;
}

const UBulletRollbackBlackboard_InternalWrapper* UBulletMoverSimulation::GetRollbackBlackboard() const
{
	return RollbackBlackboard;
}

UBulletRollbackBlackboard_InternalWrapper* UBulletMoverSimulation::GetRollbackBlackboard_Mutable()
{
	return RollbackBlackboard;
}

void UBulletMoverSimulation::SetRollbackBlackboard(UBulletRollbackBlackboard_InternalWrapper* RollbackSimBlackboard)
{
	RollbackBlackboard = RollbackSimBlackboard;
}