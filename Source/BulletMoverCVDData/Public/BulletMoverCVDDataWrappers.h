// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataWrappers/ChaosVDDataSerializationMacros.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "UObject/ObjectMacros.h"

#include "BulletMoverCVDDataWrappers.generated.h"

USTRUCT(DisplayName="BulletMover Sim Data")
struct FBulletMoverCVDSimDataWrapper : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()
	
	BULLETMOVERCVDDATA_API static FStringView WrapperTypeName;

	UPROPERTY(VisibleAnywhere, Category="BulletMover Info")
	int32 SolverID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category="BulletMover Info")
	int32 ParticleID = INDEX_NONE;

	TArray<uint8> SyncStateBytes;
	TArray<uint8> SyncStateDataCollectionBytes;
	TArray<uint8> InputCmdBytes;
	TArray<uint8> InputBulletMoverDataCollectionBytes;
	TArray<uint8> LocalSimDataBytes;

	BULLETMOVERCVDDATA_API bool Serialize(FArchive& Ar);
};

CVD_IMPLEMENT_SERIALIZER(FBulletMoverCVDSimDataWrapper)

USTRUCT()
struct FBulletMoverCVDSimDataContainer
{
	GENERATED_BODY()

	TMap<int32, TArray<TSharedPtr<FBulletMoverCVDSimDataWrapper>>> SimDataBySolverID;
};