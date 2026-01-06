// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletNetworkPredictionDriver.h"
#include "BulletNetworkPredictionUtil.h"
#include "Services/BulletNetworkPredictionInstanceData.h"

class IBulletFinalizeService
{
public:

	virtual ~IBulletFinalizeService() = default;
	virtual void FinalizeFrame(float DeltaTimeSeconds, const int32 SimFrame, const int32 SimTimeMS, const int32 FixedStepMS) = 0;
};

template<typename InModelDef>
class TBulletFinalizeService : public IBulletFinalizeService
{
public:

	using ModelDef = InModelDef;
	using DriverType = typename ModelDef::Driver;
	using StateTypes = typename ModelDef::StateTypes;
	using InputType = typename StateTypes::InputType;
	using SyncType = typename StateTypes::SyncType;
	using AuxType = typename StateTypes::AuxType;

	TBulletFinalizeService(TBulletModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	void RegisterInstance(FBulletNetworkPredictionID ID)
	{
		const int32 InstanceIdx = DataStore->Instances.GetIndex(ID);
		JnpResizeAndSetBit(FinalizeBitArray, InstanceIdx);
	}

	void UnregisterInstance(FBulletNetworkPredictionID ID)
	{
		const int32 InstanceIdx = DataStore->Instances.GetIndex(ID);
		FinalizeBitArray[InstanceIdx] = false;
	}

	void FinalizeFrame(float DeltaTimeSeconds, const int32 ServerSimFrame, const int32 SimTimeMS, const int32 FixedStepMS) final override
	{
		for (TConstSetBitIterator<> BitIt(FinalizeBitArray); BitIt; ++BitIt)
		{
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(BitIt.GetIndex());

			jnpCheckSlow(InstanceData.Info.View);
			jnpCheckSlow(InstanceData.Info.View->PendingSyncState && InstanceData.Info.View->PendingAuxState);

			SyncType* SyncState = (SyncType*)InstanceData.Info.View->PendingSyncState;
			AuxType* AuxState = (AuxType*)InstanceData.Info.View->PendingAuxState;

			FBulletNetworkPredictionDriver<ModelDef>::FinalizeFrame(InstanceData.Info.Driver, SyncState, AuxState);

			// Dispatch Cues: it may be better to take two passes here. Do all FinalizeFrame calls then all Dispatch Cues
			// (Dispatch Cues can go deep into user code, may be more cache efficient to take two passes).
			FBulletNetworkPredictionDriver<ModelDef>::DispatchCues(&InstanceData.CueDispatcher.Get(), InstanceData.Info.Driver, ServerSimFrame, SimTimeMS, FixedStepMS);
		}
	}

private:

	TBitArray<> FinalizeBitArray; // index into DataStore->Instances
	TBulletModelDataStore<ModelDef>* DataStore;
};

// ---------------------------------------------------------------------------------------

class IBulletRemoteFinalizeService
{
public:

	virtual ~IBulletRemoteFinalizeService() = default;
	virtual void FinalizeFrame(float DeltaTimeSeconds) = 0;
};

template<typename InModelDef>
class TBulletRemoteFinalizeService : public IBulletRemoteFinalizeService
{
public:

	using ModelDef = InModelDef;
	using DriverType = typename ModelDef::Driver;
	using StateTypes = typename ModelDef::StateTypes;
	using InputType = typename StateTypes::InputType;
	using SyncType = typename StateTypes::SyncType;
	using AuxType = typename StateTypes::AuxType;

	TBulletRemoteFinalizeService(TBulletModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	void RegisterInstance(FBulletNetworkPredictionID ID)
	{
		const int32 InstanceIdx = DataStore->ServerRecv_IndependentTick.GetIndex(ID);
		JnpResizeAndSetBit(FinalizeBitArray, InstanceIdx);
	}

	void UnregisterInstance(FBulletNetworkPredictionID ID)
	{
		const int32 InstanceIdx = DataStore->ServerRecv_IndependentTick.GetIndex(ID);
		FinalizeBitArray[InstanceIdx] = false;
	}

	void FinalizeFrame(float DeltaTimeSeconds) final override
	{
		for (TConstSetBitIterator<> BitIt(FinalizeBitArray); BitIt; ++BitIt)
		{
			TBulletServerRecvData_Independent<ModelDef>& ServerRecvData = DataStore->ServerRecv_IndependentTick.GetByIndexChecked(BitIt.GetIndex());
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(ServerRecvData.InstanceIdx);

			jnpCheckSlow(InstanceData.Info.View);
			jnpCheckSlow(InstanceData.Info.View->PendingSyncState && InstanceData.Info.View->PendingAuxState);

			SyncType* SyncState = (SyncType*)InstanceData.Info.View->PendingSyncState;
			AuxType* AuxState = (AuxType*)InstanceData.Info.View->PendingAuxState;

			FBulletNetworkPredictionDriver<ModelDef>::FinalizeFrame(InstanceData.Info.Driver, SyncState, AuxState);

			// Dispatch Cues: it may be better to take two passes here. Do all FinalizeFrame calls then all Dispatch Cues
			// (Dispatch Cues can go deep into user code, may be more cache efficient to take two passes).
			FBulletNetworkPredictionDriver<ModelDef>::DispatchCues(&InstanceData.CueDispatcher.Get(), InstanceData.Info.Driver, ServerRecvData.PendingFrame, ServerRecvData.TotalSimTimeMS, 0);
		}
	}

private:

	TBitArray<> FinalizeBitArray; // index into DataStore->ServerRecv_IndependentTick
	TBulletModelDataStore<ModelDef>* DataStore;
};
