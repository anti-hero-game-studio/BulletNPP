// Fill out your copyright notice in the Description page of Project Settings.


#include "DefaultMovementSet/Modes/Physics/BulletPhysicsCharacterMovementMode.h"
#include "BulletMoverComponent.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"


float UBulletPhysicsCharacterMovementMode::GetMaxWalkSlopeCosine() const
{
	if (const UBulletCommonLegacyMovementSettings* SharedSettingsPtr = GetMoverComponent<UBulletMoverComponent>()->FindSharedSettings<UBulletCommonLegacyMovementSettings>())
	{
		return SharedSettingsPtr->MaxStepHeight;
	}

	return 0.707f;
}


void UBulletPhysicsCharacterMovementMode::SetTargetHeightOverride(const float InTargetHeight)
{
	TargetHeightOverride = InTargetHeight;
	TargetHeight = InTargetHeight;
}

void UBulletPhysicsCharacterMovementMode::ClearTargetHeightOverride()
{
	TargetHeightOverride.Reset();

	TargetHeight = GetDefault<UBulletPhysicsCharacterMovementMode>(GetClass())->TargetHeight;
}

void UBulletPhysicsCharacterMovementMode::SetQueryRadiusOverride(const float InQueryRadius)
{
	QueryRadiusOverride = InQueryRadius;
	QueryRadius = InQueryRadius;
}

void UBulletPhysicsCharacterMovementMode::ClearQueryRadiusOverride()
{
	QueryRadiusOverride.Reset();

	QueryRadius = GetDefault<UBulletPhysicsCharacterMovementMode>(GetClass())->QueryRadius;
}
