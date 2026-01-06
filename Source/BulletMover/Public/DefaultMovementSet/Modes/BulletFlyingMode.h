// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BulletMovementMode.h"
#include "BulletFlyingMode.generated.h"

#define UE_API BULLETMOVER_API

class UBulletCommonLegacyMovementSettings;


/**
 * FlyingMode: a default movement mode for moving through the air freely, but still interacting with blocking geometry. The
 * moving actor will remain upright vs the movement plane.
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType)
class UBulletFlyingMode : public UBulletBaseMovementMode
{
	GENERATED_UCLASS_BODY()


public:
	/**
	 * If true, the actor will 'float' above any walkable surfaces to maintain the same height as ground-based modes. 
	 * This can prevent pops when transitioning to ground-based movement, at the cost of performing floor checks while flying.
	 */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite)
	bool bRespectDistanceOverWalkableSurfaces = false;

	UE_API virtual void GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const override;

	UE_API virtual void SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState) override;

protected:
	UE_API virtual void OnRegistered(const FName ModeName) override;
	UE_API virtual void OnUnregistered() override;

	UE_API void CaptureFinalState(USceneComponent* UpdatedComponent, FBulletMovementRecord& Record, const FBulletMoverDefaultSyncState& StartSyncState, const FVector& AngularVelocityDegrees, FBulletMoverDefaultSyncState& OutputSyncState, const float DeltaSeconds) const;

	TObjectPtr<const UBulletCommonLegacyMovementSettings> CommonLegacySettings;
};

#undef UE_API
