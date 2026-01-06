// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletLayeredMove.h"
#include "BulletLayeredMoveBase.h"
#include "BulletLaunchMove.generated.h"

#define UE_API BULLETMOVER_API

USTRUCT(Blueprintable)
struct FBulletLaunchMoveActivationParams : public FBulletLayeredMoveActivationParams
{
	GENERATED_BODY()

	/** Velocity to apply to the updated component. Could be additive or overriding depending on MixMode setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta=(ForceUnits="cm/s"))
	FVector LaunchVelocity = FVector::ZeroVector;

	// Optional movement mode name to force the actor into before applying the impulse velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FName ForceMovementMode = NAME_None;
	
};

USTRUCT(Blueprintable)
struct FBulletLaunchMoveData : public FBulletLayeredMoveInstancedData
{
	GENERATED_BODY()

	//@todo DanH: This is boilerplate begging for a macro
	virtual FBulletLayeredMoveInstancedData* Clone() const override { return new FBulletLaunchMoveData(*this); }
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	
	/** Velocity to apply to the updated component. Could be additive or overriding depending on MixMode setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta=(ForceUnits="cm/s"))
	FVector LaunchVelocity = FVector::ZeroVector;

	/** Optional movement mode name to force the actor into before applying the impulse velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FName ForceMovementMode;
	
	virtual void ActivateFromContext(const FBulletLayeredMoveActivationParams* ActivationParams) override;
	
	virtual void NetSerialize(FArchive& Ar) override;
};

// TODO: Create data for this? Is it not needed?!
UCLASS()
class ULaunchMoveLogic : public UBulletLayeredMoveLogic
{
	GENERATED_BODY()

public:
	UE_API ULaunchMoveLogic();

	/** Velocity to apply to the updated component. Could be additive or overriding depending on MixMode setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta=(ForceUnits="cm/s"))
	FVector LaunchVelocity = FVector::ZeroVector;

	//@todo DanH: Should forcing a mode be an option at the root UBulletLayeredMoveLogic?
	/** Optional movement mode name to force the actor into before applying the impulse velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FName ForceMovementMode;
	
protected:
	UE_API virtual bool GenerateMove_Implementation(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard, const FBulletMoverTickStartData& StartState, FBulletProposedMove& OutProposedMove) override;
};

/** Launch Move: provides an impulse velocity to the actor after (optionally) forcing them into a particular movement mode */
USTRUCT(BlueprintType)
struct FBulletLayeredMove_Launch : public FBulletLayeredMoveBase
{
	GENERATED_BODY()

	UE_API FBulletLayeredMove_Launch();
	virtual ~FBulletLayeredMove_Launch() {}

	// Velocity to apply to the actor. Could be additive or overriding depending on MixMode setting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta=(ForceUnits="cm/s"))
	FVector LaunchVelocity = FVector::ZeroVector;

	// Optional movement mode name to force the actor into before applying the impulse velocity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FName ForceMovementMode = NAME_None;

	// Generate a movement 
	UE_API virtual bool GenerateMove(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, const UBulletMoverComponent* MoverComp, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutProposedMove) override;

	UE_API virtual FBulletLayeredMoveBase* Clone() const override;

	UE_API virtual void NetSerialize(FArchive& Ar) override;

	UE_API virtual UScriptStruct* GetScriptStruct() const override;

	UE_API virtual FString ToSimpleString() const override;

	UE_API virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};


template<>
struct TStructOpsTypeTraits< FBulletLayeredMove_Launch > : public TStructOpsTypeTraitsBase2< FBulletLayeredMove_Launch >
{
	enum
	{
		WithCopy = true
	};
};

#undef UE_API
