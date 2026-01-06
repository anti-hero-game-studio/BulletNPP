// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletMoverTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "BulletUserDefinedStructSupport.generated.h"

class UUserDefinedStruct;


#define UE_API BULLETMOVER_API

/** Wrapper class that's used to add User-Defined Struct instances to Mover Data Collections (input or state).
 * This allows devs to add custom data to inputs and/or state without requiring native code.
 * Note that these are typically less efficient than natively-defined structs, and the logic of operations
 * like interpolation, merging, and serialization may be simplistic for a project's needs.
 * At present:
 * - any differences between any struct contents will trigger reconciliation, even small floating point number differences
 * - only boolean values can be merged
 * - interpolation will take the entire struct instance from the highest weight frame
 */
USTRUCT()
struct FBulletMoverUserDefinedDataStruct : public FBulletMoverDataStructBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FInstancedStruct StructInstance;

	// Implementation of FBulletMoverDataStructBase
	virtual bool ShouldReconcile(const FBulletMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FBulletMoverDataStructBase& From, const FBulletMoverDataStructBase& To, float LerpFactor) override;
	virtual void Merge(const FBulletMoverDataStructBase& From) override;
	virtual FBulletMoverDataStructBase* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	// Note: this is the FBulletMoverUserDefinedDataStruct type, NOT the User-Defined Struct type
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	// This returns the User-Defined Struct type
	virtual const UScriptStruct* GetDataScriptStruct() const override;


};


#undef UE_API
