// Fill out your copyright notice in the Description page of Project Settings.


#include "DefaultMovementSet/Modes/Physics/BulletPhysicsMovementMode.h"
#include "BulletMoverComponent.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"



float UBulletPhysicsMovementMode::GetMaxSpeed() const
{
	if (MaxSpeedOverride.IsSet())
	{
		return MaxSpeedOverride.GetValue();
	}
	
	if (const UBulletCommonLegacyMovementSettings* SharedSettingsPtr = GetMoverComponent<UBulletMoverComponent>()->FindSharedSettings<UBulletCommonLegacyMovementSettings>())
	{
		return SharedSettingsPtr->MaxSpeed;
	}

	UE_LOG(LogBulletMover, Warning, TEXT("Invalid max speed on CharacterBulletMoverComponent"));
	return 0.0f;
}

void UBulletPhysicsMovementMode::OverrideMaxSpeed(float Value)
{
	MaxSpeedOverride = Value;
}

void UBulletPhysicsMovementMode::ClearMaxSpeedOverride()
{
	MaxSpeedOverride.Reset();
}

float UBulletPhysicsMovementMode::GetAcceleration() const
{
	if (AccelerationOverride.IsSet())
	{
		return AccelerationOverride.GetValue();
	}
	
	if (const UBulletCommonLegacyMovementSettings* SharedSettingsPtr = GetMoverComponent<UBulletMoverComponent>()->FindSharedSettings<UBulletCommonLegacyMovementSettings>())
	{
		return SharedSettingsPtr->Acceleration;
	}
	
	UE_LOG(LogBulletMover, Warning, TEXT("Invalid acceleration on CharacterBulletMoverComponent"));
	return 0.0f;
}


void UBulletPhysicsMovementMode::OverrideAcceleration(const float Value)
{
	AccelerationOverride = Value;
}

void UBulletPhysicsMovementMode::ClearAccelerationOverride()
{
	AccelerationOverride.Reset();
}