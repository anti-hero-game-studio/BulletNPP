// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletPhysicsCharacterMovementMode.h"
#include "BulletFloatingWalkingMode.generated.h"

class UBulletCommonLegacyMovementSettings;
/**
 * 
 */
UCLASS()
class BULLETMOVER_API UBulletFloatingWalkingMode : public UBulletPhysicsCharacterMovementMode
{
	GENERATED_BODY()
	
public:
	
	virtual void GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState) override;
	
	
	virtual void OnRegistered(const FName ModeName) override; 
	virtual void OnUnregistered() override;
	
protected:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spring Settings")
	float RideHeight = 100.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spring Settings")
	float RideSpringStrength = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spring Settings")
	float RideSpringDamper = 1.f;
	
	TWeakObjectPtr<const UBulletCommonLegacyMovementSettings> CommonLegacySettings;
	
};

