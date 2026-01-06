// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BulletNetworkPredictionModelDef.h"
#include "BulletNetworkPredictionDriver.h"

struct FBulletGenericKinematicActorSyncState
{
	FVector_NetQuantize100	Location;
	FQuat Rotation;
};

// Generic def for kinematic (non physics) actor that doesn't have a backing simulation. This is quite limited in what it can do,
// but hopefully useful is that they can still be recorded and restored
struct FBulletGenericKinematicActorDef : FBulletNetworkPredictionModelDef
{
	JNP_MODEL_BODY();

	using StateTypes = TBulletNetworkPredictionStateTypes<void, FBulletGenericKinematicActorSyncState, void>;
	using Driver = AActor;
	static const TCHAR* GetName() { return TEXT("Generic Kinematic Actor"); }
	static constexpr int32 GetSortPriority() { return (int32)EBulletNetworkPredictionSortPriority::PreKinematicMovers; }
};

template<>
struct FBulletNetworkPredictionDriver<FBulletGenericKinematicActorDef> : FBulletNetworkPredictionDriverBase<FBulletGenericKinematicActorDef>
{
	static void InitializeSimulationState(AActor* ActorDriver, FBulletGenericKinematicActorSyncState* Sync, void* Aux)
	{
		const FTransform& Transform = ActorDriver->GetActorTransform();
		Sync->Location = Transform.GetLocation();
		Sync->Rotation = Transform.GetRotation();
	}
};
