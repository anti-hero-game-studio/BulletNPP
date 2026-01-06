// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoveLibrary/BulletMovementUtilsTypes.h"

#include "BulletInstantMovementEffect.generated.h"

#define UE_API BULLETMOVER_API

class UBulletMoverComponent;
class UBulletMoverSimulation;
struct FBulletMoverTimeStep;
struct FBulletMoverTickStartData;
struct FBulletMoverSyncState;
struct FBulletMoverSimulationEventData;

struct FBulletApplyMovementEffectParams
{
	USceneComponent* UpdatedComponent;

	UPrimitiveComponent* UpdatedPrimitive;

	const UBulletMoverComponent* MoverComp;

	const FBulletMoverTickStartData* StartState;

	const FBulletMoverTimeStep* TimeStep;

	TArray<TSharedPtr<FBulletMoverSimulationEventData>> OutputEvents;
};

/** 
 * Async safe parameters passed to ApplyMovementEffect_Async. 
 * It is almost certainly missing the Physics Object handle and other things, this is just a first pass
 */
struct FBulletApplyMovementEffectParams_Async
{
	UBulletMoverSimulation* Simulation;
	const FBulletMoverTickStartData* StartState;
	const FBulletMoverTimeStep* TimeStep;
};

/**
 * Instant Movement Effects are methods of affecting movement state directly on a Mover-based actor for one tick.
 * Note: This is only applied one tick and then removed
 * Common uses would be for Teleporting, Changing Movement Modes directly, one time force application, etc.
 * Multiple Instant Movement Effects can be active at the time
 */
USTRUCT(BlueprintInternalUseOnly)
struct FBulletInstantMovementEffect
{
	GENERATED_BODY()

	FBulletInstantMovementEffect() { }

	virtual ~FBulletInstantMovementEffect() { }
	
	// @return newly allocated copy of this FBulletInstantMovementEffect. Must be overridden by child classes
	UE_API virtual FBulletInstantMovementEffect* Clone() const;

	UE_API virtual void NetSerialize(FArchive& Ar);

	UE_API virtual UScriptStruct* GetScriptStruct() const;

	UE_API virtual FString ToSimpleString() const;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) {}

	virtual bool ApplyMovementEffect(FBulletApplyMovementEffectParams& ApplyEffectParams, FBulletMoverSyncState& OutputState) { return false; }
	virtual bool ApplyMovementEffect_Async(FBulletApplyMovementEffectParams_Async& ApplyEffectParams, FBulletMoverSyncState& OutputState) { return false; }
};

template<>
struct TStructOpsTypeTraits< FBulletInstantMovementEffect > : public TStructOpsTypeTraitsBase2< FBulletInstantMovementEffect >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};

#undef UE_API
