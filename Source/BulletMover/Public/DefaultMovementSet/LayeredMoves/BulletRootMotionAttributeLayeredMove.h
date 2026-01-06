// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletLayeredMove.h"
#include "NativeGameplayTags.h"
#include "RootMotionModifier.h"
#include "BulletRootMotionAttributeLayeredMove.generated.h"

#define UE_API BULLETMOVER_API

BULLETMOVER_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BulletMover_AnimRootMotion_MeshAttribute);

/** 
 * Root Motion Attribute Move: handles root motion from a mesh's custom attribute, ignoring scaling.
 * Currently only supports Independent ticking mode, and allows controlled movement while jumping/falling or when a SkipAnimRootMotion tag is active.
 */
USTRUCT(BlueprintType)
struct FBulletLayeredMove_RootMotionAttribute : public FBulletLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FBulletLayeredMove_RootMotionAttribute();
	virtual ~FBulletLayeredMove_RootMotionAttribute() {}

	// If true, any root motion rotations will be projected onto the movement plane (in worldspace), relative to the "up" direction. Otherwise, they'll be taken as-is.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bConstrainWorldRotToMovementPlane = true;

protected:
	// These member variables are NOT replicated. They are used if we rollback and resimulate when the root motion attribute is no longer in sync.
	bool bDidAttrHaveRootMotionForResim = false;
	FTransform LocalRootMotionForResim;
	FMotionWarpingUpdateContext WarpingContextForResim;

	// Generate a movement 
	UE_API virtual bool GenerateMove(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, const UBulletMoverComponent* MoverComp, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutProposedMove) override;

	UE_API virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;

	UE_API virtual FBulletLayeredMoveBase* Clone() const override;

	UE_API virtual void NetSerialize(FArchive& Ar) override;

	UE_API virtual UScriptStruct* GetScriptStruct() const override;

	UE_API virtual FString ToSimpleString() const override;

	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits< FBulletLayeredMove_RootMotionAttribute > : public TStructOpsTypeTraitsBase2< FBulletLayeredMove_RootMotionAttribute >
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
