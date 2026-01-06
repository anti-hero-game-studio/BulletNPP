// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletMoverTypes.h"
#include "BulletInputContainerStruct.generated.h"



#define UE_API BULLETMOVER_API

/** 
 * Wrapper class that's used to include input structs in the sync state without them causing reconciliation.
 * This is intended only for internal use.
 */

 USTRUCT()
 struct FBulletMoverInputContainerDataStruct : public FBulletMoverDataStructBase
 {
	GENERATED_BODY()

public:
	// All input data in this struct
	FBulletMoverDataCollection InputCollection;

	// Implementation of FBulletMoverDataStructBase

	// This struct never triggers reconciliation
	virtual bool ShouldReconcile(const FBulletMoverDataStructBase& AuthorityState) const override { return false; }
	virtual void Interpolate(const FBulletMoverDataStructBase& From, const FBulletMoverDataStructBase& To, float LerpFactor) override;
	virtual FBulletMoverDataStructBase* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

};


#undef UE_API
