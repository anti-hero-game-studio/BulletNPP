// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletNetworkPredictionConfig.generated.h"

// Must be kept in sync with EJNP_TickingPolicy
UENUM()
enum class EBulletNetworkPredictionTickingPolicy : uint8
{
	// Client ticks at local frame rate. Server ticks clients independently at client input cmd rate.
	Independent	= 1 << 0,
	// Everyone ticks at same fixed rate. Supports group rollback
	Fixed		= 1 << 1,

	All = Independent | Fixed UMETA(Hidden),
};
ENUM_CLASS_FLAGS(EBulletNetworkPredictionTickingPolicy);

enum class EBulletNetworkPredictionLocalInputPolicy : uint8
{
	// Up to the user to write input via FBulletNetSimProxy::WriteInputCmd.
	Passive,
	// ProduceInput is called on the driver before every simulation frame. This may be necessary for things like aim assist and fixed step simulations that run multiple sim frames per engine frame
	PollPerSimFrame,
};

// Must be kept in sync with EJNP_NetworkLOD. Note: SimExtrapolate Not currently implemented so it is hidden
UENUM()
enum class EBulletNetworkLOD : uint8
{
	Interpolated	= 1 << 0,
	SimExtrapolate	= 1 << 1 UMETA(Hidden),
	ForwardPredict	= 1 << 2,

	All = Interpolated | SimExtrapolate | ForwardPredict UMETA(Hidden),
};
ENUM_CLASS_FLAGS(EBulletNetworkLOD);

static constexpr EBulletNetworkLOD GetHighestNetworkLOD(EBulletNetworkLOD Mask)
{
	if ((uint8)Mask >= (uint8)EBulletNetworkLOD::ForwardPredict)
	{
		return EBulletNetworkLOD::ForwardPredict;
	}

	if ((uint8)Mask >= (uint8)EBulletNetworkLOD::SimExtrapolate)
	{
		return EBulletNetworkLOD::SimExtrapolate;
	}

	return EBulletNetworkLOD::Interpolated;
}

// -------------------------------------------------------------------------------------------------------------

// What a ModelDef of capable of
struct FBulletNetworkPredictionModelDefCapabilities
{
	struct FSupportedNetworkLODs
	{
		EBulletNetworkLOD	AP;
		EBulletNetworkLOD	SP;
	};

	FSupportedNetworkLODs FixedNetworkLODs = FSupportedNetworkLODs{ EBulletNetworkLOD::All, EBulletNetworkLOD::All };
	FSupportedNetworkLODs IndependentNetworkLODs = FSupportedNetworkLODs{ EBulletNetworkLOD::All, EBulletNetworkLOD::Interpolated | EBulletNetworkLOD::SimExtrapolate };

	EBulletNetworkPredictionTickingPolicy SupportedTickingPolicies = EBulletNetworkPredictionTickingPolicy::All;
};

// How a registered instance should behave globally. That is, independent of any instance state (local role, connection, significance, local budgets). E.g, everyone agrees on this.
// This can be changed explicitly by the user or simulation. For example, a sim that transitions between fixed and independent ticking modes.
struct FBulletNetworkPredictionInstanceArchetype
{
	EBulletNetworkPredictionTickingPolicy	TickingMode;
	void NetSerialize(FArchive& Ar)
	{
		Ar << TickingMode;
	}
};

// The config should tell us what services we should be subscribed to. See UBulletNetworkPredictionWorldManager::ConfigureInstance
// This probably needs to be split into two parts:
//	1. What settings/config that the server is authority over and must be agreed on (TickingPolicy)
//	2. What are the local settings that can be lodded around?
struct FBulletNetworkPredictionInstanceConfig
{
	EBulletNetworkPredictionLocalInputPolicy InputPolicy = EBulletNetworkPredictionLocalInputPolicy::Passive;
	EBulletNetworkLOD NetworkLOD = EBulletNetworkLOD::ForwardPredict;
};
