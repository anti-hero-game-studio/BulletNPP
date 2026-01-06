// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BulletMovementMode.h"
#include "BulletAsyncFlyingMode.generated.h"

#define UE_API BULLETMOVER_API

class UBulletCommonLegacyMovementSettings;


/**
 * AsyncFlyingMode: a default movement mode for moving through the air freely, but still interacting with blocking geometry. The
 * moving actor will remain upright vs the movement plane.
 * This mode is threadsafe, and simulates movement without actually modifying any scene component(s).
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, Experimental)
class UBulletAsyncFlyingMode : public UBulletBaseMovementMode
{
	GENERATED_UCLASS_BODY()


public:
	/**
	 * If true, the actor will 'float' above any walkable surfaces to maintain the same height as ground-based modes.
	 * This can prevent pops when transitioning to ground-based movement, at the cost of performing floor checks while flying.
	 */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite)
	bool bRespectDistanceOverWalkableSurfaces = false;

	UE_API virtual void OnRegistered(const FName ModeName) override;
	UE_API virtual void OnUnregistered() override;
	UE_API virtual void GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const override;
	UE_API virtual void SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState) override;

protected:
	TObjectPtr<const UBulletCommonLegacyMovementSettings> CommonLegacySettings;
};

#undef UE_API
