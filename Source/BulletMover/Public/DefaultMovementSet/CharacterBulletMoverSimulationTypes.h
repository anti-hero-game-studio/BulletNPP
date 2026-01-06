// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletMoverSimulationTypes.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"

#include "CharacterBulletMoverSimulationTypes.generated.h"

USTRUCT()
struct FBulletLandedEventData : public FBulletMoverSimulationEventData
{
	GENERATED_BODY()

	FBulletLandedEventData(double InEventTimeMs, const FHitResult& InHitResult, const FName InNewModeName)
		: FBulletMoverSimulationEventData(InEventTimeMs)
		, HitResult(InHitResult)
		, NewModeName(InNewModeName)
	{
	}
	FBulletLandedEventData() {}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FBulletLandedEventData::StaticStruct();
	}

	FHitResult HitResult;
	FName NewModeName = NAME_None;
};

USTRUCT()
struct FBulletJumpedEventData : public FBulletMoverSimulationEventData
{
	GENERATED_BODY()

	FBulletJumpedEventData(double InEventTimeMs, float InJumpStartHeight)
		: FBulletMoverSimulationEventData(InEventTimeMs)
		, JumpStartHeight(InJumpStartHeight)
	{
	}
	FBulletJumpedEventData() {}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FBulletJumpedEventData::StaticStruct();
	}

	float JumpStartHeight = 0.0f;
};

USTRUCT(BlueprintType)
struct FBulletFloorResultData : public FBulletMoverDataStructBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FBulletFloorCheckResult FloorResult;

	FBulletFloorResultData() = default;
	virtual ~FBulletFloorResultData() = default;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}

	virtual FBulletMoverDataStructBase* Clone() const override
	{
		return new FBulletFloorResultData(*this);
	}

	BULLETMOVER_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	BULLETMOVER_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	BULLETMOVER_API virtual bool ShouldReconcile(const FBulletMoverDataStructBase& AuthorityState) const override;
	BULLETMOVER_API virtual void Interpolate(const FBulletMoverDataStructBase& From, const FBulletMoverDataStructBase& To, float Pct) override;
	BULLETMOVER_API virtual void Merge(const FBulletMoverDataStructBase& From) override;
	BULLETMOVER_API virtual void Decay(float DecayAmount) override;
};

template<>
struct TStructOpsTypeTraits< FBulletFloorResultData > : public TStructOpsTypeTraitsBase2< FBulletFloorResultData >
{
	enum
	{
		WithCopy = true
	};
};