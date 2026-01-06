// Copyright Epic Games, Inc. All Rights Reserved.


#include "BulletInputContainerStruct.h"
#include "BulletMoverLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletInputContainerStruct)

#define LOCTEXT_NAMESPACE "BulletMoverInputContainerStruct"



void FBulletMoverInputContainerDataStruct::Interpolate(const FBulletMoverDataStructBase& From, const FBulletMoverDataStructBase& To, float LerpFactor)
{
	const FBulletMoverInputContainerDataStruct* FromContainer = static_cast<const FBulletMoverInputContainerDataStruct*>(&From);
	const FBulletMoverInputContainerDataStruct* ToContainer = static_cast<const FBulletMoverInputContainerDataStruct*>(&To);

	InputCollection.Interpolate(FromContainer->InputCollection, ToContainer->InputCollection, LerpFactor);
}


FBulletMoverDataStructBase* FBulletMoverInputContainerDataStruct::Clone() const
{
	FBulletMoverInputContainerDataStruct* CopyPtr = new FBulletMoverInputContainerDataStruct(*this);
	return CopyPtr;
}

bool FBulletMoverInputContainerDataStruct::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!Super::NetSerialize(Ar, Map, bOutSuccess))
	{
		bOutSuccess = false;
		return false;
	}

	if (!InputCollection.NetSerialize(Ar, Map, bOutSuccess))
	{
		bOutSuccess = false;
		return false;
	}

	bOutSuccess = true;
	return true;
}


#undef LOCTEXT_NAMESPACE
