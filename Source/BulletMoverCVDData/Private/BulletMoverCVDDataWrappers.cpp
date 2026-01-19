// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverCVDDataWrappers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMoverCVDDataWrappers)

FStringView FBulletMoverCVDSimDataWrapper::WrapperTypeName = TEXT("FBulletMoverCVDSimDataWrapper");

bool FBulletMoverCVDSimDataWrapper::Serialize(FArchive& Ar)
{
	Ar << bHasValidData;

	if (!bHasValidData)
	{
		return !Ar.IsError();
	}

	Ar << SolverID;
	Ar << ParticleID;
	Ar << SyncStateBytes;
	Ar << SyncStateDataCollectionBytes;
	Ar << InputCmdBytes;
	Ar << InputBulletMoverDataCollectionBytes;
	Ar << LocalSimDataBytes;

	return !Ar.IsError();
}
