// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MotionWarpingAdapter.h"
#include "BulletMoverComponent.h"
#include "MotionWarpingBulletMoverAdapter.generated.h"

#define UE_API BULLETMOVER_API

// Adapter for MoverComponent actors to participate in motion warping

UCLASS(MinimalAPI)
class UMotionWarpingBulletMoverAdapter : public UMotionWarpingBaseAdapter
{
	GENERATED_BODY()

public:
	UE_API virtual void BeginDestroy() override;

	UE_API void SetMoverComp(UBulletMoverComponent* InMoverComp);

	UE_API virtual AActor* GetActor() const override;
	UE_API virtual USkeletalMeshComponent* GetMesh() const override;
	UE_API virtual FVector GetVisualRootLocation() const override;
	UE_API virtual FVector GetBaseVisualTranslationOffset() const override;
	UE_API virtual FQuat GetBaseVisualRotationOffset() const override;

private:
	// This is called when our Mover actor wants to warp local motion, and passes the responsibility onto the warping component
	FTransform WarpLocalRootMotionOnMoverComp(const FTransform& LocalRootMotionTransform, float DeltaSeconds, const FMotionWarpingUpdateContext* OptionalWarpingContext);

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UBulletMoverComponent> TargetMoverComp;
};

#undef UE_API
