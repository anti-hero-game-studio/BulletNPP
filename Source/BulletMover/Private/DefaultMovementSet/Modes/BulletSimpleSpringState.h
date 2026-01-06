// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BulletMovementMode.h"
#include "BulletMoverTypes.h"
#include "BulletSimpleSpringState.generated.h"

/**
* Internal state data for the SimpleSpringWalkingMode
*/
USTRUCT()
struct FBulletSimpleSpringState : public FBulletMoverDataStructBase
{
	GENERATED_BODY()
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FBulletMoverDataStructBase* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool ShouldReconcile(const FBulletMoverDataStructBase& AuthorityState) const override;
	virtual void Interpolate(const FBulletMoverDataStructBase& From, const FBulletMoverDataStructBase& To, float Pct) override;

	// Acceleration of internal spring model
	UPROPERTY(BlueprintReadOnly, Category = "Bullet Mover|Experimental")
	FVector CurrentAccel = FVector::ZeroVector;
};
