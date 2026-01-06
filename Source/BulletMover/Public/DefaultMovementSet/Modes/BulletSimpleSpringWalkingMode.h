// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BulletSimpleWalkingMode.h"
#include "BulletSimpleSpringWalkingMode.generated.h"

#define UE_API BULLETMOVER_API

/**
 * A walking mode that uses a critically damped spring for translation and rotation.
 * The strength of the critically damped spring is set via smoothing times (separate for translation and rotation)
 */
UCLASS(BlueprintType)
class UBulletSimpleSpringWalkingMode : public UBulletSimpleWalkingMode
{
	GENERATED_BODY()

public:
	UE_API virtual void SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState) override;
	UE_API virtual void GenerateWalkMove_Implementation(FBulletMoverTickStartData& StartState, float DeltaSeconds, const FVector& DesiredVelocity,
									 const FQuat& DesiredFacing, const FQuat& CurrentFacing, FVector& InOutAngularVelocityDegrees, FVector& InOutVelocity) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet Mover|Spring Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float VelocitySmoothingTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet Mover|Spring Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "s"))
	float FacingSmoothingTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet Mover|Spring Settings", meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	// Below this speed we set velocity to 0
	float VelocityDeadzoneThreshold = 0.1f;
};

#undef UE_API
