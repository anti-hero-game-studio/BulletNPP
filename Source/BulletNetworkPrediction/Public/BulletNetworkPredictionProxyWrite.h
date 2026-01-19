// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletNetworkPredictionProxy.h"
#include "BulletNetworkPredictionTrace.h"

inline void FBulletNetworkPredictionProxy::TraceViaConfigFunc(EConfigAction Action)
{
	// The ConfigFunc allows use to use the registered ModelDef type to access FBulletNetworkPredictionDriver<ModelDef>::TraceUserState
	// this allows for per-ModelDef customizations but more importantly will call State->ToString on the correct child class.
	// consider FChildSyncState : FBaseSyncState{}; with a base driver class that calls WriteSyncState<FBaseSyncState>(...);
#if UE_BNP_TRACE_USER_STATES_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(BulletNetworkPredictionChannel))
	{
		ConfigFunc(this, FBulletNetworkPredictionID(), Action);
	}
#endif
}

template<typename TInputCmd>
const TInputCmd* FBulletNetworkPredictionProxy::WriteInputCmd(TFunctionRef<void(TInputCmd&)> WriteFunc,const FAnsiStringView& TraceMsg)
{
	if (TInputCmd* InputCmd = static_cast<TInputCmd*>(View.PendingInputCmd))
	{
		WriteFunc(*InputCmd);
		
		UE_BNP_TRACE_OOB_STATE_MOD(ID.GetTraceID(), View.PendingFrame, TraceMsg);
		TraceViaConfigFunc(EConfigAction::TraceInput);
		return InputCmd;
	}
	return nullptr;
}

template<typename TSyncState>
const TSyncState* FBulletNetworkPredictionProxy::WriteSyncState(TFunctionRef<void(TSyncState&)> WriteFunc, const FAnsiStringView& TraceMsg)
{
	if (TSyncState* SyncState = static_cast<TSyncState*>(View.PendingSyncState))
	{
		WriteFunc(*SyncState);
		UE_BNP_TRACE_OOB_STATE_MOD(ID.GetTraceID(), View.PendingFrame, TraceMsg);
		ConfigFunc(this, FBulletNetworkPredictionID(), EConfigAction::TraceSync);
		return SyncState;
	}
	return nullptr;
}

template<typename TSyncState>
const TSyncState* FBulletNetworkPredictionProxy::WritePresentationSyncState(TFunctionRef<void(TSyncState&)> WriteFunc, const FAnsiStringView& TraceMsg)
{
	if (TSyncState* SyncState = static_cast<TSyncState*>(View.PresentationSyncState))
	{
		WriteFunc(*SyncState);
		UE_BNP_TRACE_OOB_STATE_MOD(ID.GetTraceID(), View.PendingFrame, TraceMsg);
		ConfigFunc(this, FBulletNetworkPredictionID(), EConfigAction::TraceSync);
		return SyncState;
	}
	return nullptr;
}

template<typename TSyncState>
const TSyncState* FBulletNetworkPredictionProxy::WritePrevPresentationSyncState(TFunctionRef<void(TSyncState&)> WriteFunc, const FAnsiStringView& TraceMsg)
{
	if (TSyncState* SyncState = static_cast<TSyncState*>(View.PrevPresentationSyncState))
	{
		WriteFunc(*SyncState);
		UE_BNP_TRACE_OOB_STATE_MOD(ID.GetTraceID(), View.PendingFrame, TraceMsg);
		ConfigFunc(this, FBulletNetworkPredictionID(), EConfigAction::TraceSync);
		return SyncState;
	}
	return nullptr;
}

template<typename TAuxState>
const TAuxState* FBulletNetworkPredictionProxy::WriteAuxState(TFunctionRef<void(TAuxState&)> WriteFunc, const FAnsiStringView& TraceMsg)
{
	if (TAuxState* AuxState = static_cast<TAuxState*>(View.PendingAuxState))
	{
		WriteFunc(*AuxState);
		UE_BNP_TRACE_OOB_STATE_MOD(ID.GetTraceID(), View.PendingFrame, TraceMsg);
		ConfigFunc(this, FBulletNetworkPredictionID(), EConfigAction::TraceAux);
		return AuxState;
	}
	return nullptr;
}

template<typename TAuxState>
const TAuxState* FBulletNetworkPredictionProxy::WritePresentationAuxState(TFunctionRef<void(TAuxState&)> WriteFunc, const FAnsiStringView& TraceMsg)
{
	if (TAuxState* AuxState = static_cast<TAuxState*>(View.PresentationAuxState))
	{
		WriteFunc(*AuxState);
		UE_BNP_TRACE_OOB_STATE_MOD(ID.GetTraceID(), View.PendingFrame, TraceMsg);
		ConfigFunc(this, FBulletNetworkPredictionID(), EConfigAction::TraceAux);
		return AuxState;
	}
	return nullptr;
}

template<typename TAuxState>
const TAuxState* FBulletNetworkPredictionProxy::WritePrevPresentationAuxState(TFunctionRef<void(TAuxState&)> WriteFunc, const FAnsiStringView& TraceMsg)
{
	if (TAuxState* AuxState = static_cast<TAuxState*>(View.PrevPresentationAuxState))
	{
		WriteFunc(*AuxState);
		UE_BNP_TRACE_OOB_STATE_MOD(ID.GetTraceID(), View.PendingFrame, TraceMsg);
		ConfigFunc(this, FBulletNetworkPredictionID(), EConfigAction::TraceAux);
		return AuxState;
	}
	return nullptr;
}
