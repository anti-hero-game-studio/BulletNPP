// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/EngineTypes.h"
#include "BulletNetworkPredictionBuffer.h"
#include "BulletNetworkPredictionConditionalState.h"
#include "BulletNetworkPredictionInstanceMap.h"
#include "BulletNetworkPredictionCues.h"
#include "BulletNetworkPredictionModelDef.h"
#include "BulletNetworkPredictionTickState.h"

// Enum that maps to internal NetworkPrediction services, see notes in NetworkPredictionServiceRegistry.h
enum class EBulletNetworkPredictionService : uint32
{
	None = 0,

	// Common services that fix/independent can share
	
	//MAX_COMMON				= ServerRPC,

	// Services exclusive to fix tick mode
	FixedServerRPC				= 1 << 0,
	FixedRollback				= 1 << 1,
	FixedExtrapolate			= 1 << 2,	// TODO
	FixedInterpolate			= 1 << 3,
	FixedInputLocal				= 1 << 4,
	FixedInputRemote			= 1 << 5,
	FixedTick					= 1 << 6,
    FixedPhysics			    = 1 << 7,
	FixedSmoothing				= 1 << 8,
	FixedFinalize				= 1 << 9,
	MAX_FIXED					= FixedFinalize,

	// Services exclusive to independent tick mode
	ServerRPC               =  1 << 10,
	IndependentRollback		= 1 << 11,
	IndependentExtrapolate	= 1 << 12,	// TODO
	IndependentInterpolate	= 1 << 13,

	IndependentLocalInput	= 1 << 14,
	IndependentLocalTick	= 1 << 15,
	IndependentLocalPhysics	= 1 << 16,
	IndependentRemoteTick	= 1 << 17,
	IndependentRemotePhysics	= 1 << 18,
	
	IndependentSmoothingFinalize	= 1 << 19,	// TODO
	IndependentLocalFinalize		= 1 << 20,
	IndependentRemoteFinalize		= 1 << 21,
	MAX_INDEPENDENT					= IndependentRemoteFinalize,

	// Helper masks
	//ANY_COMMON = (MAX_COMMON<<1)-1,
	ANY_FIXED = ((MAX_FIXED<<1)-1),
	ANY_INDEPENDENT = (((MAX_INDEPENDENT<<1)-1) & ~ANY_FIXED),
};

ENUM_CLASS_FLAGS(EBulletNetworkPredictionService);

// Basic data that all instances have
template<typename ModelDef=FBulletNetworkPredictionModelDef>
struct TInstanceData
{
	TBulletNetworkPredictionModelInfo<ModelDef>		Info;
	
	ENetRole NetRole = ROLE_None;
	TUniqueObj<TBulletNetSimCueDispatcher<ModelDef>>	CueDispatcher;	// Should maybe be moved out?

	int32 TraceID;
	EBulletNetworkPredictionService ServiceMask = EBulletNetworkPredictionService::None;
};

// Frame data that instances with StateTypes will have.
template<typename ModelDef=FBulletNetworkPredictionModelDef>
struct TBulletInstanceFrameState
{
	using StateTypes = typename ModelDef::StateTypes;
	using InputType = typename StateTypes::InputType;
	using SyncType = typename StateTypes::SyncType;
	using AuxType = typename StateTypes::AuxType;

	struct FFrame
	{
		float InterpolationTimeMS = 0.f;
		TBulletConditionalState<InputType>	InputCmd;
		TBulletConditionalState<SyncType>	SyncState;
		TBulletConditionalState<AuxType>	AuxState;
	};

	TBulletNetworkPredictionBuffer<FFrame> Buffer;

	TBulletInstanceFrameState()
		: Buffer(64) { } // fixme
};

// Data the client receives from the server
template<typename ModelDef=FBulletNetworkPredictionModelDef>
struct TBulletClientRecvData
{
	using StateTypes = typename ModelDef::StateTypes;
	using InputType = typename StateTypes::InputType;
	using SyncType = typename StateTypes::SyncType;
	using AuxType = typename StateTypes::AuxType;

	int32 ServerFrame; // Fixed tick || Independent AP only
	int32 SimTimeMS; // Independent tick only
	
	TBulletConditionalState<InputType> InputCmd; // SP Only
	TBulletConditionalState<SyncType>	SyncState;
	TBulletConditionalState<AuxType>	AuxState;

	// Delta Serialization
	struct AckedFrame
	{
		TBulletConditionalState<InputType> InputCmd; // SP Only
		TBulletConditionalState<SyncType>	SyncState;
		TBulletConditionalState<AuxType>	AuxState;
	};
	
	TMap<int32, typename TBulletInstanceFrameState<ModelDef>::FFrame> AckedFrames;
	// Acceleration data.
	int32 ID = INDEX_NONE;
	int32 TraceID = INDEX_NONE;
	int32 InstanceIdx = INDEX_NONE;	// Index into TBulletModelDataStore::Instances
	int32 FramesIdx = INDEX_NONE;	// Index into TBulletModelDataStore::Frames
	ENetRole NetRole = ROLE_None;
};

// Data the server receives from a fixed ticking AP client
template<typename ModelDef=FBulletNetworkPredictionModelDef>
struct TBulletServerRecvData_Fixed
{
	using StateTypes = typename ModelDef::StateTypes;
	using InputType = typename StateTypes::InputType;

	TBulletNetworkPredictionBuffer<TPair<double,TBulletConditionalState<InputType>>> InputBuffer;

	// Note that these are client frame numbers, they do not match the servers local PendingFrame
	int32 LastConsumedFrame = INDEX_NONE;
	int32 LastRecvFrame = INDEX_NONE;

	int32 InputFault = 0;
	int32 TraceID = INDEX_NONE;
	//Added By Kai , Delta Serialization Support
	int32 ID = INDEX_NONE;

	TBulletServerRecvData_Fixed()
		: InputBuffer(32) {} // fixme
};

// Data the server receives from an independent ticking AP client
template<typename ModelDef=FBulletNetworkPredictionModelDef>
struct TBulletServerRecvData_Independent
{
	using StateTypes = typename ModelDef::StateTypes;
	using InputType = typename StateTypes::InputType;

	struct FFrame
	{
		TBulletConditionalState<InputType>	InputCmd;
		int32	DeltaTimeMS;
	};

	TBulletServerRecvData_Independent()
		: InputBuffer(16) { }

	int32 PendingFrame = 0;
	int32 TotalSimTimeMS = 0;
	float UnspentTimeMS = 0.f;

	int32 LastConsumedFrame = INDEX_NONE;
	int32 LastRecvFrame = INDEX_NONE;

	TBulletNetworkPredictionBuffer<FFrame> InputBuffer;

	// Acceleration data.
	int32 TraceID = INDEX_NONE;
	int32 InstanceIdx = INDEX_NONE;	// Index into TBulletModelDataStore::Instances
	int32 FramesIdx = INDEX_NONE;	// Index into TBulletModelDataStore::Frames
};

// Stores all public data for a given model def
template<typename ModelDef=FBulletNetworkPredictionModelDef>
struct TBulletModelDataStore
{
	TBulletStableInstanceMap<TInstanceData<ModelDef>>	Instances;

	TBulletInstanceMap<TBulletInstanceFrameState<ModelDef>> Frames;
	
	TBulletInstanceMap<TBulletClientRecvData<ModelDef>> ClientRecv;
	TBitArray<> ClientRecvBitMask;

	TBulletInstanceMap<TBulletServerRecvData_Fixed<ModelDef>> ServerRecv;

	TBulletInstanceMap<TBulletServerRecvData_Independent<ModelDef>> ServerRecv_IndependentTick;

	TBulletInstanceMap<FDelegateHandle> DeferredRegisterHandle;
};
