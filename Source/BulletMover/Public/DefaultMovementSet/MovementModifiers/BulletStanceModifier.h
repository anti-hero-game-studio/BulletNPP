// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletMovementModifier.h"
#include "BulletStanceModifier.generated.h"

#define UE_API BULLETMOVER_API

class UCapsuleComponent;
class UCharacterBulletMoverComponent;

UENUM(BlueprintType)
enum class EStanceMode : uint8
{
	// Invalid default stance
	Invalid = 0,
	// Actor goes into crouch
	Crouch,
	// Actor goes into prone - not currently implemented
	Prone,
};

/**
 * Stances: Applies settings to the actor to make them go into different stances like crouch or prone(not implemented), affects actor maxacceleration and capsule height
 * Note: This modifier currently uses the CDO of the actor to reset values to "standing" values.
 *		 This modifier also assumes the actor is using a capsule as it's updated component for now
 */
USTRUCT(BlueprintType)
struct FStanceModifier : public FBulletMovementModifierBase
{
	GENERATED_BODY()

public:
	UE_API FStanceModifier();
	virtual ~FStanceModifier() override {}

	EStanceMode ActiveStance;
	
	UE_API virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const override;
	
	/** Fired when this modifier is activated. */
	UE_API virtual void OnStart(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState) override;
	
	/** Fired when this modifier is deactivated. */
	UE_API virtual void OnEnd(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState) override;
	
	/** Fired just before a Substep */
	UE_API virtual void OnPreMovement(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep) override;

	/** Fired after a Substep */
	UE_API virtual void OnPostMovement(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState) override;
	
	// @return newly allocated copy of this FBulletMovementModifier. Must be overridden by child classes
	UE_API virtual FBulletMovementModifierBase* Clone() const override;

	UE_API virtual void NetSerialize(FArchive& Ar) override;

	UE_API virtual UScriptStruct* GetScriptStruct() const override;

	UE_API virtual FString ToSimpleString() const override;

	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;

	UE_API virtual bool CanExpand(const UCharacterBulletMoverComponent* MoverComp) const;
	
	// Whether expanding should be from the base of the capsule or not
	UE_API virtual bool ShouldExpandingMaintainBase(const UCharacterBulletMoverComponent* MoverComp) const;

protected:
	// Modifies the updated component casted to a capsule component
	UE_API virtual void AdjustCapsule(UBulletMoverComponent* MoverComp, float OldHalfHeight, float NewHalfHeight, float NewEyeHeight);

	// Applies any movement settings like acceleration or max speed changes
	UE_API void ApplyMovementSettings(UBulletMoverComponent* MoverComp);
	
	// Reverts any movement settings like acceleration or max speed changes
	UE_API void RevertMovementSettings(UBulletMoverComponent* MoverComp);
};

template<>
struct TStructOpsTypeTraits< FStanceModifier > : public TStructOpsTypeTraitsBase2< FStanceModifier >
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
