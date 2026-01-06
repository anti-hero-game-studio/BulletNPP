// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/SceneComponent.h"
#include "BulletMoverDataModelTypes.h"
#include "BulletMovementUtilsTypes.h"

#include "BulletAsyncMovementUtils.generated.h"

#define UE_API BULLETMOVER_API

struct FBulletMovementRecord;
class UBulletMoverComponent;
struct FBulletOptionalFloorCheckResult;


/**
 * AsyncMovementUtils: a collection of stateless static BP-accessible functions focused on testing potential movements in a 
 * threadsafe manner without actually causing immediate changes.
 */
UCLASS(MinimalAPI, Experimental)
class UBulletAsyncMovementUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Tests potential movement of a component without actually moving it, taking penetration resolution issues into account first.  
	 * Returns true if any movement was possible
	 * Modifies OutHit with final movement hit data
	 * Appends to InOutMoveRecord with any movement substeps
	 */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static UE_API bool TestDepenetratingMove(const FBulletMovingComponentSet& MovingComps, const FVector& StartLocation, const FVector& TargetLocation, const FQuat& StartRotation, const FQuat& TargetRotation, bool bShouldSweep, FHitResult& OutHit, UPARAM(ref) FBulletMovementRecord& InOutMoveRecord);

	/** Tests potential movement of a component without actually moving it, taking penetration resolution issues into account first.  Relies on CollisionParams to describe the query.
	 * Returns true if any movement was possible
	 * Modifies OutHit with final movement hit data
	 * Appends to InOutMoveRecord with any movement substeps
	 */
	static UE_API bool TestDepenetratingMove(const FBulletMovingComponentSet& MovingComps, const FVector& StartLocation, const FVector& TargetLocation, const FQuat& StartRotation, const FQuat& TargetRotation, bool bShouldSweep, FBulletMoverCollisionParams& CollisionParams, FHitResult& OutHit, FBulletMovementRecord& InOutMoveRecord);

	/** Tests potential movement of a component sliding along a surface, without actually moving it. 
	 * Returns the percent of time applied, with 0.0 meaning no movement would occur. 
	 * Modifies InOutHit with final movement hit data
	 * Appends to InOutMoveRecord with any movement substeps
	 */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static UE_API float TestSlidingMoveAlongHitSurface(const FBulletMovingComponentSet& MovingComps, const FVector& OriginalMoveDelta, const FVector& LocationAtHit, const FQuat& TargetRotation, UPARAM(ref) FHitResult& InOutHit, UPARAM(ref) FBulletMovementRecord& InOutMoveRecord);
	

	/** Tests potential movement of a component sliding along a surface, without actually moving it. Relies on CollisionParams to describe the query.
	 * Returns the percent of time applied, with 0.0 meaning no movement would occur. 
	 * Modifies InOutHit with final movement hit data
	 * Appends to InOutMoveRecord with any movement substeps
	 */
	static UE_API float TestSlidingMoveAlongHitSurface(const FBulletMovingComponentSet& MovingComps, const FVector& OriginalMoveDelta, const FVector& LocationAtHit, const FQuat& TargetRotation, FBulletMoverCollisionParams& CollisionParams, FHitResult& InOutHit, FBulletMovementRecord& InOutMoveRecord);


	/** Attempts to find a move that would resolve an initially penetrating blockage.
	 * Returns true if an adjustment was found. The adjustment will be in OutAdjustmentDelta.
	 */
	static UE_API bool FindMoveToResolveInitialPenetration_Internal(const FBulletMovingComponentSet& MovingComps, const FVector& StartLocation, const FQuat& StartRotation, const FHitResult& PenetratingHit, FBulletMoverCollisionParams& CollisionParams, FVector& OutAdjustmentDelta);


	/** Tests a move of a component without actually moving it. 
	 * Returns true if any motion could occur. Detailed blocking hit results are written to OutHit.  
	 * If not sweeping, full movement will always be allowed even if new blocking overlaps would occur.
	 */
	static UE_API bool TestMoveComponent_Internal(const FBulletMovingComponentSet& MovingComps, const FVector& StartLocation, const FVector& TargetLocation, const FQuat& StartRotation, const FQuat& TargetRotation, bool bShouldSweep, FBulletMoverCollisionParams& CollisionParams, FHitResult& OutHit);

	
};

#undef UE_API
