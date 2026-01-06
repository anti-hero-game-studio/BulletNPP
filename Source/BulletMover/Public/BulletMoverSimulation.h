// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "BulletMoverSimulation.generated.h"

#define UE_API BULLETMOVER_API

class UBulletMoverBlackboard;
struct FBulletMoverSyncState;
class UBulletRollbackBlackboard_InternalWrapper;

/**
* WIP Base class for a Mover simulation.
* The simulation is intended to be the thing that updates the Mover
* state and should be safe to run on an async thread
*/
UCLASS(MinimalAPI, BlueprintType)
class UBulletMoverSimulation : public UObject
{
	GENERATED_BODY()

public:
	UE_API UBulletMoverSimulation();

	// Warning: the regular blackboard will be fully replaced by the rollback blackboard in the future
	UFUNCTION(BlueprintPure, Category = Mover)
	UE_API const UBulletMoverBlackboard* GetBlackboard() const;

	// Warning: the regular blackboard will be fully replaced by the rollback blackboard in the future
	UFUNCTION(BlueprintPure, Category = Mover)
	UE_API UBulletMoverBlackboard* GetBlackboard_Mutable();

	UFUNCTION(BlueprintPure, Category = Mover)
	UE_API const UBulletRollbackBlackboard_InternalWrapper* GetRollbackBlackboard() const;

	UFUNCTION(BlueprintPure, Category = Mover)
	UE_API UBulletRollbackBlackboard_InternalWrapper* GetRollbackBlackboard_Mutable();


	/**
	* Attempt to teleport to TargetTransform. The teleport is not guaranteed to happen. This function is meant to be called by an instant movement effect as part of its effect application.
	* If it succeeds a FBulletTeleportSucceededEventData will be emitted, if it fails a FBulletTeleportFailedEventData will be sent.
	* @param TimeStep The time step of the current step or substep being simulated. This will come from the ApplyMovementEffect function.
	* @param TargetTransform The transform to teleport to. In the case bUseActorRotation is true, the rotation of this transform will be ignored.
	* @param bUseActorRotation If true, the rotation will not be modified upon teleportation. If false, the rotation in TargetTransform will be used to orient the teleported.
	* @param OutputState This is the sync state that me modified as a result of the application of this effect. Like TimeStep, this should come from the ApplyMovementEffect function.
	*/
	UFUNCTION(BlueprintCallable, Category = Mover)
	virtual void AttemptTeleport(const FBulletMoverTimeStep& TimeStep, const FTransform& TargetTransform, bool bUseActorRotation, FBulletMoverSyncState& OutputState) {}


	// Used during initialization only
	UE_API void SetRollbackBlackboard(UBulletRollbackBlackboard_InternalWrapper* RollbackSimBlackboard);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UBulletMoverBlackboard> Blackboard = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBulletRollbackBlackboard_InternalWrapper> RollbackBlackboard = nullptr;

};

#undef UE_API
