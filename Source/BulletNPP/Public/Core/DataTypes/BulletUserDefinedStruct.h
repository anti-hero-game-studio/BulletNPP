// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletPhysicsTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "BulletUserDefinedStruct.generated.h"

class UUserDefinedStruct;


#define UE_API BULLETNPP_API

/** Wrapper class that's used to add User-Defined Struct instances to Bullet Data Collections (input or state).
 * This allows devs to add custom data to inputs and/or state without requiring native code.
 * Note that these are typically less efficient than natively-defined structs, and the logic of operations
 * like interpolation, merging, and serialization may be simplistic for a project's needs.
 * At present:
 * - any differences between any struct contents will trigger reconciliation, even small floating point number differences
 * - only boolean values can be merged
 * - interpolation will take the entire struct instance from the highest weight frame
 */
USTRUCT()
struct FBulletUserDefinedDataStruct : public FBulletDataStructBase
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FInstancedStruct StructInstance;

	// Implementation of FBulletDataStructBase
	virtual bool ShouldReconcile(const FBulletDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FBulletDataStructBase& From, const FBulletDataStructBase& To, float LerpFactor) override;
	virtual void Merge(const FBulletDataStructBase& From) override;
	virtual FBulletDataStructBase* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	// Note: this is the FBulletUserDefinedDataStruct type, NOT the User-Defined Struct type
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	// This returns the User-Defined Struct type
	virtual const UScriptStruct* GetDataScriptStruct() const override;


};


#undef UE_API