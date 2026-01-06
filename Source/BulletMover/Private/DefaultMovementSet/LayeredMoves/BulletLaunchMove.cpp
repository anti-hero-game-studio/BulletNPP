// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/LayeredMoves/BulletLaunchMove.h"
#include "BulletMoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletLaunchMove)

void FBulletLaunchMoveData::ActivateFromContext(const FBulletLayeredMoveActivationParams* ActivationParams)
{
	if (ActivationParams)
	{
		if (const FBulletLaunchMoveActivationParams* LaunchMoveActivationParams = static_cast<const FBulletLaunchMoveActivationParams*>(ActivationParams))
		{
			LaunchVelocity = LaunchMoveActivationParams->LaunchVelocity;
			DurationMs = LaunchMoveActivationParams->DurationMs;
			ForceMovementMode = LaunchMoveActivationParams->ForceMovementMode;
		}
	}
}

void FBulletLaunchMoveData::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10, 16>(LaunchVelocity, Ar);

	bool bUsingForcedMovementMode = !ForceMovementMode.IsNone();

	Ar.SerializeBits(&bUsingForcedMovementMode, 1);

	if (bUsingForcedMovementMode)
	{
		Ar << ForceMovementMode;
	}
	else
	{
		ForceMovementMode = NAME_None;
	}
}

ULaunchMoveLogic::ULaunchMoveLogic()
{
	DefaultDurationMs = 0.f;
	MixMode = EBulletMoveMixMode::OverrideVelocity;
	InstancedDataStructType = FBulletLaunchMoveData::StaticStruct();
}

bool ULaunchMoveLogic::GenerateMove_Implementation(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard, const FBulletMoverTickStartData& StartState, FBulletProposedMove& OutProposedMove)
{
	const FBulletLaunchMoveData& LaunchMoveData = AccessExecutionMoveData<FBulletLaunchMoveData>();
	
	OutProposedMove.MixMode = MixMode;
	OutProposedMove.LinearVelocity = LaunchMoveData.LaunchVelocity;
	OutProposedMove.PreferredMode = LaunchMoveData.ForceMovementMode;

	return true;
}

FBulletLayeredMove_Launch::FBulletLayeredMove_Launch()
{
	DurationMs = 0.f;
	MixMode = EBulletMoveMixMode::OverrideVelocity;
}

bool FBulletLayeredMove_Launch::GenerateMove(const FBulletMoverTickStartData& SimState, const FBulletMoverTimeStep& TimeStep, const UBulletMoverComponent* MoverComp, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutProposedMove)
{
	OutProposedMove.MixMode = MixMode;
	OutProposedMove.LinearVelocity = LaunchVelocity;
	OutProposedMove.PreferredMode = ForceMovementMode;

	return true;
}

FBulletLayeredMoveBase* FBulletLayeredMove_Launch::Clone() const
{
	FBulletLayeredMove_Launch* CopyPtr = new FBulletLayeredMove_Launch(*this);
	return CopyPtr;
}

void FBulletLayeredMove_Launch::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10, 16>(LaunchVelocity, Ar);

	bool bUsingForcedMovementMode = !ForceMovementMode.IsNone();

	Ar.SerializeBits(&bUsingForcedMovementMode, 1);

	if (bUsingForcedMovementMode)
	{
		Ar << ForceMovementMode;
	}

}

UScriptStruct* FBulletLayeredMove_Launch::GetScriptStruct() const
{
	return FBulletLayeredMove_Launch::StaticStruct();
}

FString FBulletLayeredMove_Launch::ToSimpleString() const
{
	return FString::Printf(TEXT("Launch"));
}

void FBulletLayeredMove_Launch::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
