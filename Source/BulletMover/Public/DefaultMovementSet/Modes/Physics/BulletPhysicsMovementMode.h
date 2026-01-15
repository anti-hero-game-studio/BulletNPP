// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletMovementMode.h"
#include "BulletPhysicsMovementMode.generated.h"

/**
 * 
 */
UCLASS(Abstract, Within = BulletMoverComponent, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class BULLETMOVER_API UBulletPhysicsMovementMode : public UBulletBaseMovementMode
{
	GENERATED_BODY()
	
	
public:
	
	virtual float GetMaxSpeed() const;
	virtual void OverrideMaxSpeed(float Value);
	virtual void ClearMaxSpeedOverride();

	virtual float GetAcceleration() const;
	virtual void OverrideAcceleration(float Value);
	virtual void ClearAccelerationOverride();
	
	EBulletMoverFrictionOverrideMode GetFrictionOverrideMode() const {return FrictionOverrideMode; };
	
	
protected:
	
	// Allows the mode to override friction on collision with other physics bodies.
	UPROPERTY(EditAnywhere, Category = "Collision Settings")
	EBulletMoverFrictionOverrideMode FrictionOverrideMode = EBulletMoverFrictionOverrideMode::OverrideToZeroWhenMoving;

private:
	TOptional<float> MaxSpeedOverride;
	TOptional<float> AccelerationOverride;
};
