// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "BulletLayeredMove.h"
#include "DefaultMovementSet/LayeredMoves/BulletMontageStateProvider.h"
#include "BulletAnimRootMotionLayeredMove.generated.h"

#define UE_API BULLETMOVER_API

class UAnimMontage;


/** Anim Root Motion Move: handles root motion from a montage played on the primary visual component (skeletal mesh). 
 * In this method, root motion is extracted independently from anim playback. The move will end itself if the animation
 * is interrupted on the mesh.
 */
USTRUCT(BlueprintType)
struct FBulletLayeredMove_AnimRootMotion : public FBulletLayeredMove_MontageStateProvider
{
	GENERATED_BODY()

	UE_API FBulletLayeredMove_AnimRootMotion();
	virtual ~FBulletLayeredMove_AnimRootMotion() {}

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FBulletMoverAnimMontageState MontageState;

	// Generate a movement 
	UE_API virtual bool GenerateMove(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, const UBulletMoverComponent* MoverComp, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutProposedMove) override;

	UE_API virtual FBulletLayeredMoveBase* Clone() const override;

	UE_API virtual void NetSerialize(FArchive& Ar) override;

	UE_API virtual UScriptStruct* GetScriptStruct() const override;

	UE_API virtual FString ToSimpleString() const override;

	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;

	// FBulletLayeredMove_MontageStateProvider
	UE_API virtual FBulletMoverAnimMontageState GetMontageState() const override;
};

template<>
struct TStructOpsTypeTraits< FBulletLayeredMove_AnimRootMotion > : public TStructOpsTypeTraitsBase2< FBulletLayeredMove_AnimRootMotion >
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
