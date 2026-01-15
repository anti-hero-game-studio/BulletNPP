// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BulletPhysicsGroundMovementUtils.generated.h"

#define UE_API BULLETMOVER_API

struct FBulletFloorCheckResult;
/**
 * 
 */
UCLASS(MinimalAPI)
class UBulletPhysicsGroundMovementUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	/** Computes the local velocity at the supplied position of the hit object in floor result */
	UFUNCTION(BlueprintCallable, Category = BulletMover, meta=(WorldContext = "WorldContextObject"))
	static UE_API FVector ComputeLocalGroundVelocity_Internal(const UObject* WorldContextObject, const FVector& Position, const FBulletFloorCheckResult& FloorResult);
	
};

#undef UE_API
