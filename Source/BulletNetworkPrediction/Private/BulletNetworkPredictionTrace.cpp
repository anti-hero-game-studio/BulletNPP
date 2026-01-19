// Copyright Epic Games, Inc. All Rights Reserved.


#include "BulletNetworkPredictionTrace.h"
#include "Containers/StringFwd.h"
#include "Engine/GameInstance.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "BulletNetworkPredictionLog.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"

// TODO:
// Should update string tracing with UE::Trace::AnsiString

namespace NetworkPredictionTraceInternal
{
	enum class EBulletNetworkPredictionTraceVersion : uint32
	{
		Initial = 1,
	};

	static constexpr EBulletNetworkPredictionTraceVersion NetworkPredictionTraceVersion = EBulletNetworkPredictionTraceVersion::Initial;
};

UE_TRACE_CHANNEL_DEFINE(BulletNetworkPredictionChannel)

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, SimScope)
	UE_TRACE_EVENT_FIELD(int32, TraceID)
UE_TRACE_EVENT_END()

// Trace a simulation creation. GroupName is attached as attachment.
UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, SimulationCreated)
	UE_TRACE_EVENT_FIELD(uint32, SimulationID) // server assigned (shared client<->server)
	UE_TRACE_EVENT_FIELD(int32, TraceID) // process unique id
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, DebugName)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, SimulationConfig)
	UE_TRACE_EVENT_FIELD(int32, TraceID)
	UE_TRACE_EVENT_FIELD(uint8, NetRole)
	UE_TRACE_EVENT_FIELD(uint8, bHasNetConnection)
	UE_TRACE_EVENT_FIELD(uint8, TickingPolicy)
	UE_TRACE_EVENT_FIELD(uint8, NetworkLOD)
	UE_TRACE_EVENT_FIELD(int32, ServiceMask)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, SimulationScope)
	UE_TRACE_EVENT_FIELD(int32, TraceID)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, SimState)
	UE_TRACE_EVENT_FIELD(int32, TraceID)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, Version)
	UE_TRACE_EVENT_FIELD(uint32, Version)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, WorldPreInit)
	UE_TRACE_EVENT_FIELD(uint64, EngineFrameNumber)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, PieBegin)
	UE_TRACE_EVENT_FIELD(uint64, EngineFrameNumber)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, WorldFrameStart)
	UE_TRACE_EVENT_FIELD(uint64, EngineFrameNumber)
	UE_TRACE_EVENT_FIELD(float, DeltaSeconds)
UE_TRACE_EVENT_END()

// General system fault. Log message is in attachment
UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, SystemFault)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Message)
UE_TRACE_EVENT_END()

// Traces general tick state (called before ticking N sims)
UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, Tick)
	UE_TRACE_EVENT_FIELD(int32, StartMS)
	UE_TRACE_EVENT_FIELD(int32, DeltaMS)
	UE_TRACE_EVENT_FIELD(int32, OutputFrame)
UE_TRACE_EVENT_END()

// Signals that the given sim has done a tick. Expected to be called after the 'Tick' event has been traced
UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, SimTick)
	UE_TRACE_EVENT_FIELD(int32, TraceID)
UE_TRACE_EVENT_END()

// Signals that we are in are receiving a NetSerialize function
UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, NetRecv)
	UE_TRACE_EVENT_FIELD(int32, Frame)
	UE_TRACE_EVENT_FIELD(int32, TimeMS)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, ShouldReconcile)
	UE_TRACE_EVENT_FIELD(int32, TraceID)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, Reconcile)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, UserString)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, RollbackInject)
	UE_TRACE_EVENT_FIELD(int32, TraceID)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, PushInputFrame)
	UE_TRACE_EVENT_FIELD(int32, Frame)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, FixedTickOffset)
	UE_TRACE_EVENT_FIELD(int32, Offset)
	UE_TRACE_EVENT_FIELD(bool, Changed)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, BufferedInput)
	UE_TRACE_EVENT_FIELD(int32, NumBufferedFrames)
	UE_TRACE_EVENT_FIELD(bool, bFault)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, ProduceInput)
	UE_TRACE_EVENT_FIELD(int32, TraceID)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, OOBStateMod)
	UE_TRACE_EVENT_FIELD(int32, TraceID)
	UE_TRACE_EVENT_FIELD(int32, Frame)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Source)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, InputCmd)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Value)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, SyncState)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Value)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(BulletNetworkPrediction, AuxState)
	UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Value)
UE_TRACE_EVENT_END()

// ---------------------------------------------------------------------------

void FBulletNetworkPredictionTrace::TraceSimulationCreated_Internal(FBulletNetworkPredictionID ID, FStringBuilderBase& Builder)
{
	const uint16 AttachmentSize = Builder.Len() * sizeof(FStringBuilderBase::ElementType);

	UE_TRACE_LOG(BulletNetworkPrediction, SimulationCreated, BulletNetworkPredictionChannel)
		<< SimulationCreated.SimulationID((int32)ID)
		<< SimulationCreated.TraceID(ID.GetTraceID())
		<< SimulationCreated.DebugName(Builder.ToString(), Builder.Len());
}

void FBulletNetworkPredictionTrace::TraceWorldFrameStart(UGameInstance* GameInstance, float DeltaSeconds)
{
	if (!GameInstance || GameInstance->GetWorld()->GetNetMode() == NM_Standalone)
	{
		// No networking yet, don't start tracing
		return;
	}

	UE_TRACE_LOG(BulletNetworkPrediction, WorldFrameStart, BulletNetworkPredictionChannel)
		<< WorldFrameStart.EngineFrameNumber(GFrameNumber)
		<< WorldFrameStart.DeltaSeconds(DeltaSeconds);
}

void FBulletNetworkPredictionTrace::TraceSimulationConfig(int32 TraceID, ENetRole NetRole, bool bHasNetConnection, const FBulletNetworkPredictionInstanceArchetype& Archetype, const FBulletNetworkPredictionInstanceConfig& Config, int32 ServiceMask)
{
	bnpEnsureMsgf(NetRole != ENetRole::ROLE_None && NetRole != ENetRole::ROLE_MAX, TEXT("Invalid NetRole %d"), NetRole);

	UE_TRACE_LOG(BulletNetworkPrediction, SimulationConfig, BulletNetworkPredictionChannel)
		<< SimulationConfig.TraceID(TraceID)
		<< SimulationConfig.NetRole((uint8)NetRole)
		<< SimulationConfig.bHasNetConnection((uint8)bHasNetConnection)
		<< SimulationConfig.TickingPolicy((uint8)Archetype.TickingMode)
		<< SimulationConfig.ServiceMask(ServiceMask);		
}

void FBulletNetworkPredictionTrace::TraceSimulationScope(int32 TraceID)
{
	UE_TRACE_LOG(BulletNetworkPrediction, SimulationScope, BulletNetworkPredictionChannel)
		<< SimulationScope.TraceID(TraceID);
}

void FBulletNetworkPredictionTrace::TraceSimState(int32 TraceID)
{
	UE_TRACE_LOG(BulletNetworkPrediction, SimState, BulletNetworkPredictionChannel)
		<< SimState.TraceID(TraceID);
}

void FBulletNetworkPredictionTrace::TraceTick(int32 StartMS, int32 DeltaMS, int32 OutputFrame)
{
	UE_TRACE_LOG(BulletNetworkPrediction, Tick, BulletNetworkPredictionChannel)
		<< Tick.StartMS(StartMS)
		<< Tick.DeltaMS(DeltaMS)
		<< Tick.OutputFrame(OutputFrame);
}

void FBulletNetworkPredictionTrace::TraceSimTick(int32 TraceID)
{
	UE_TRACE_LOG(BulletNetworkPrediction, SimTick, BulletNetworkPredictionChannel)
		<< SimTick.TraceID(TraceID);
}

void FBulletNetworkPredictionTrace::TraceUserState_Internal(ETraceUserState StateType, FAnsiStringBuilderBase& Builder)
{
	switch(StateType)
	{
		case ETraceUserState::Input:
		{
			UE_TRACE_LOG(BulletNetworkPrediction, InputCmd, BulletNetworkPredictionChannel)
				<< InputCmd.Value(Builder.GetData(), Builder.Len());
			break;
		}
		case ETraceUserState::Sync:
		{
			UE_TRACE_LOG(BulletNetworkPrediction, SyncState, BulletNetworkPredictionChannel)
				<< SyncState.Value(Builder.GetData(), Builder.Len());
			break;
		}
		case ETraceUserState::Aux:
		{
			UE_TRACE_LOG(BulletNetworkPrediction, AuxState, BulletNetworkPredictionChannel)
				<< AuxState.Value(Builder.GetData(), Builder.Len());
			break;
		}
	}
}

void FBulletNetworkPredictionTrace::TraceNetRecv(int32 Frame, int32 TimeMS)
{
	UE_TRACE_LOG(BulletNetworkPrediction, NetRecv, BulletNetworkPredictionChannel)
		<< NetRecv.Frame(Frame)
		<< NetRecv.TimeMS(TimeMS);
}

void FBulletNetworkPredictionTrace::TraceReconcile(const FAnsiStringView& StrView)
{
	UE_TRACE_LOG(BulletNetworkPrediction, Reconcile, BulletNetworkPredictionChannel)
		<< Reconcile.UserString(StrView.GetData(), StrView.Len());
}

void FBulletNetworkPredictionTrace::TraceShouldReconcile(int32 TraceID)
{
	UE_TRACE_LOG(BulletNetworkPrediction, ShouldReconcile, BulletNetworkPredictionChannel)
		<< ShouldReconcile.TraceID(TraceID);
}

void FBulletNetworkPredictionTrace::TraceRollbackInject(int32 TraceID)
{
	UE_TRACE_LOG(BulletNetworkPrediction, RollbackInject, BulletNetworkPredictionChannel)
		<< RollbackInject.TraceID(TraceID);
}

void FBulletNetworkPredictionTrace::TracePIEStart()
{
	UE_TRACE_LOG(BulletNetworkPrediction, PieBegin, BulletNetworkPredictionChannel)
		<< PieBegin.EngineFrameNumber(GFrameNumber);
}

void FBulletNetworkPredictionTrace::TraceWorldPreInit()
{
	UE_TRACE_LOG(BulletNetworkPrediction, Version, BulletNetworkPredictionChannel)
		<< Version.Version((uint32)NetworkPredictionTraceInternal::NetworkPredictionTraceVersion);

	UE_TRACE_LOG(BulletNetworkPrediction, WorldPreInit, BulletNetworkPredictionChannel)
		<< WorldPreInit.EngineFrameNumber(GFrameNumber);
}

void FBulletNetworkPredictionTrace::TracePushInputFrame(int32 Frame)
{
	UE_TRACE_LOG(BulletNetworkPrediction, PushInputFrame, BulletNetworkPredictionChannel)
		<< PushInputFrame.Frame(Frame);
}

void FBulletNetworkPredictionTrace::TraceFixedTickOffset(int32 Offset, bool bChanged)
{
	UE_TRACE_LOG(BulletNetworkPrediction, FixedTickOffset, BulletNetworkPredictionChannel)
		<< FixedTickOffset.Offset(Offset)
		<< FixedTickOffset.Changed(bChanged);
}

void FBulletNetworkPredictionTrace::TraceBufferedInput(int32 NumBufferedFrames, bool bFault)
{
	UE_TRACE_LOG(BulletNetworkPrediction, BufferedInput, BulletNetworkPredictionChannel)
		<< BufferedInput.NumBufferedFrames(NumBufferedFrames)
		<< BufferedInput.bFault(bFault);
}

void FBulletNetworkPredictionTrace::TraceProduceInput(int32 TraceID)
{
	UE_TRACE_LOG(BulletNetworkPrediction, ProduceInput, BulletNetworkPredictionChannel)
		<< ProduceInput.TraceID(TraceID);
}

void FBulletNetworkPredictionTrace::TraceOOBStateMod(int32 TraceID, int32 Frame, const FAnsiStringView& StrView)
{
	UE_TRACE_LOG(BulletNetworkPrediction, OOBStateMod, BulletNetworkPredictionChannel)
		<< OOBStateMod.TraceID(TraceID)
		<< OOBStateMod.Frame(Frame)
		<< OOBStateMod.Source(StrView.GetData(), StrView.Len());
}

void FBulletNetworkPredictionTrace::TraceSystemFault(const TCHAR* Fmt, ...)
{
	TStringBuilder<512> Builder;

	va_list Args;
	va_start(Args, Fmt);
	Builder.AppendV(Fmt, Args);
	va_end(Args);

	UE_LOG(LogBulletNetworkPrediction, Log, TEXT("SystemFault: %s"), Builder.ToString());

	UE_TRACE_LOG(BulletNetworkPrediction, SystemFault, BulletNetworkPredictionChannel)
		<< SystemFault.Message(Builder.GetData(), Builder.Len());
}
