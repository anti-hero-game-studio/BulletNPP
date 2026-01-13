// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletNetworkPredictionPlayerControllerComponent.h"
#include "BulletNetworkPredictionReplicationProxy.h"
#include "BulletNetworkPredictionTrace.h"
#include "Services/BulletNetworkPredictionInstanceData.h"


namespace NetworkPredictionCVars
{
	// *** Modified By Kai Time Dilation Support *** //
	BULLETNETSIM_DEVCVAR_SHIPCONST_INT(DisableTimeDilation, 0, "b.np.TimeDilation.Disable", "Time Dilation Effects Autonomous Proxy Client, Suggested by Server to slow down or speed us To Make Input Buffer healthy (Healthy input Buffer Always has Input And Is buffering as low as possible)");
	BULLETNETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationAmount, 0.01f, "b.np.TimeDilation.Amount", "Server-side CVar, Disable TimeDilation by setting to 0 | Default: 0.01 | Value is in percent where 0.01 = 1% dilation. Example: 1.0/0.01 = 100, meaning that over the time it usually takes to tick 100 Fixed steps we will tick 99 or 101 depending on if we dilate up or down.");
	BULLETNETSIM_DEVCVAR_SHIPCONST_INT(TimeDilationEscalation, 1, "b.np.TimeDilation.Escalation", "Server-side CVar, Dilate the time more depending on how many ticks we need to adjust. When set to false we use the set TimeDilationAmount and wait the amount of time it takes to perform correct the offset. When set to true we multiply the TimeDilationAmount with the buffer offset count which will correct the offset in one TimeDilationAmount cycle.");
	BULLETNETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationEscalationDecay, 0.05f, "b.np.TimeDilation.EscalationDecay", "Value is a multiplier, Default: 0.05. For each escalated TimeDilation amount, also decay by this much. Disable by setting to 0");
	BULLETNETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationEscalationDecayMax, 0.5f, "b.np.TimeDilation.EscalationDecayMax", "Value is a multiplier, Default: 0.5. The max decay value for escalated time dilation. Lower value means higher decay.");
	BULLETNETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationMax, 1.1f, "b.np.TimeDilation.Max", "Max value of the time dilation multiplier.");
	BULLETNETSIM_DEVCVAR_SHIPCONST_FLOAT(TimeDilationMin, 0.9f, "b.np.TimeDilation.Min", "Min value of the time dilation multiplier.");
	// *** END Modified By Kai Smoothing Support *** //
}
// InputService's job is to write InputCmds to a subscribed instance's FrameBuffer[PendingFrame].InputCmd.

class IBulletInputService
{
public:

	virtual ~IBulletInputService() = default;
	virtual void ProduceInput(int32 DeltaTimeMS, const float& InterpolationTimeMS) = 0;
	virtual void OnFixedInputReceived(const int32& ClientFrame,const float& InterpolationTime , const TArray<FBulletSimulationReplicatedInput>& Inputs
		,UBulletNetworkPredictionPlayerControllerComponent* InputHandler
		,FBulletFixedTickState* TickState) = 0;
};

// Calls ProduceInput on driver to get local input
//	Requires valid FBulletNetworkPredictionDriver::ProduceInput function
template<typename InModelDef>
class TBulletLocalInputService : public IBulletInputService
{
public:

	using ModelDef = InModelDef;
	using DriverType = typename ModelDef::Driver;
	using StateTypes = typename ModelDef::StateTypes;
	using InputType= typename StateTypes::InputType;


	TBulletLocalInputService(TBulletModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	void RegisterInstance(FBulletNetworkPredictionID ID)
	{
		bnpCheckfSlow(!std::is_void_v<InputType>, TEXT("ModelDef %s with null InputCmd type was registered for local input service."), ModelDef::GetName());

		TInstanceData<ModelDef>* InstanceData = DataStore->Instances.Find(ID);
		bnpCheckSlow(InstanceData);

		InstanceMap.Add((int32)ID, FInstance{ID.GetTraceID(), InstanceData->Info.View, InstanceData->Info.Driver});
	}

	void UnregisterInstance(FBulletNetworkPredictionID ID)
	{
		InstanceMap.Remove((int32)ID);
	}

	void ProduceInput(int32 DeltaTimeMS , const float& InterpolationTimeMS) final override
	{
		for (auto& MapIt : InstanceMap)
		{
			FInstance& Instance = MapIt.Value;

			bnpCheckSlow(Instance.Driver);
			bnpCheckSlow(Instance.View);
			bnpCheckSlow(Instance.View->PendingInputCmd);

			FBulletNetworkPredictionDriver<ModelDef>::ProduceInput(Instance.Driver, DeltaTimeMS, (InputType*)Instance.View->PendingInputCmd);
			*Instance.View->InterpolationTimeMS = InterpolationTimeMS;

			UE_BNP_TRACE_PRODUCE_INPUT(Instance.TraceID);
			UE_BNP_TRACE_USER_STATE_INPUT(ModelDef, (InputType*)Instance.View->PendingInputCmd);
		}
	}

	void OnFixedInputReceived(const int32& ClientFrame,const float& InterpolationTime, const TArray<FBulletSimulationReplicatedInput>& Inputs
		, UBulletNetworkPredictionPlayerControllerComponent* InputHandler
		,FBulletFixedTickState* TickState) final override
	{
	};

private:
	
	struct FInstance
	{
		int32 TraceID;
		FBulletNetworkPredictionStateView* View;
		DriverType* Driver;
	};

	TSortedMap<int32, FInstance, TInlineAllocator<1>> InstanceMap;
	TBulletModelDataStore<ModelDef>* DataStore;
};

// Pulls input from ServerRecvData. E.g: server side input buffering
template<typename InModelDef>
class TBulletRemoteInputService : public IBulletInputService
{
public:

	using ModelDef = InModelDef;
	using DriverType = typename ModelDef::Driver;
	using StateTypes = typename ModelDef::StateTypes;
	using InputType= typename StateTypes::InputType;

	TBulletRemoteInputService(TBulletModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	static void SetMaxFaultLimit(int32 InMaxFaultLimit) 
	{ 
		MaxFaultLimit = InMaxFaultLimit; 
	}

	static void SetDesiredBufferedInputs(int32 InDesiredBufferedInputs) 
	{ 
		DesiredBufferedInputs = InDesiredBufferedInputs; 
	}

	void RegisterInstance(FBulletNetworkPredictionID ID)
	{
		const int32 InstanceIndex = DataStore->Instances.GetIndexChecked(ID);
		
		const int32 ServerRecvIdx = DataStore->ServerRecv.GetIndex(ID);
		InstanceMap.Add((int32)ID, FInstance{ID.GetTraceID(), InstanceIndex, ServerRecvIdx});
	}

	void UnregisterInstance(FBulletNetworkPredictionID ID)
	{
		InstanceMap.Remove(ID);
	}

	void OnFixedInputReceived(const int32& ClientFrame,const float& InterpolationTime,const TArray<FBulletSimulationReplicatedInput>& Inputs
		,UBulletNetworkPredictionPlayerControllerComponent* RPCHandler
		,FBulletFixedTickState* TickState) final override
	{
		bnpEnsure(ClientFrame >= 0);
		for (auto& MapIt : InstanceMap)
		{
			FInstance& Instance = MapIt.Value;
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(Instance.InstanceIndex);
			if (RPCHandler != InstanceData.Info.RPCHandler || !InstanceData.Info.RPCHandler)
			{
				continue; 
			}
			//ToDO : Make const TArray<FBulletSimulationReplicatedInput> a TMap, Loop through InstanceMap and Lookup in TMap
			for (const FBulletSimulationReplicatedInput& Input : Inputs)
			{
				if (Input.ID != MapIt.Key)
				{
					continue;
				}

				UNetConnection* NetConnection = InstanceData.Info.RPCHandler->GetNetConnection();
				FNetBitReader Reader(NetConnection->PackageMap,Input.InputData.GetData(),Input.DataSize);
				FBulletNetSerializeParams Params(Reader,NetConnection->PackageMap,EBulletReplicationProxyTarget::ServerRPC);
			
				UE_BNP_TRACE_SIM(Instance.TraceID);
				TBulletServerRecvData_Fixed<ModelDef>& ServerRecvData = DataStore->ServerRecv.GetByIndexChecked(Instance.ServerRecvIdx);
				for (int32 DroppedFrame = RPCHandler->LastReceivedFrame+1; DroppedFrame < ClientFrame; ++DroppedFrame)
				{
					UE_BNP_TRACE_SYSTEM_FAULT("Gap in input stream detected on server. Client frames involved: LastConsumedFrame: %d LastRecvFrame: %d. DroppedFrame: %d", ServerRecvData.LastConsumedFrame, ServerRecvData.LastRecvFrame, DroppedFrame);
					if (DroppedFrame > 0)
					{
						// FixedTick can't skip frames like independent, so copy previous input
						ServerRecvData.InputBuffer[DroppedFrame] = ServerRecvData.InputBuffer[DroppedFrame-1];
					}
				}
			
				FBulletNetworkPredictionDriver<ModelDef>::NetSerialize(ServerRecvData.InputBuffer[ClientFrame].Value, Params); // 2. InputCmd
				ServerRecvData.InputBuffer[ClientFrame].Key = InterpolationTime;
				ServerRecvData.LastRecvFrame = ClientFrame;
				// Trace what we received
				const int32 ExpectedFrameDelay = ClientFrame - RPCHandler->LastConsumedFrame;
				const int32 ExpectedConsumeFrame = TickState->PendingFrame + ExpectedFrameDelay - 1;
				UE_BNP_TRACE_NET_RECV(ExpectedConsumeFrame, ExpectedConsumeFrame * TickState->FixedStepMS);
				UE_BNP_TRACE_USER_STATE_INPUT(ModelDef, ServerRecvData.InputBuffer[ClientFrame].Value.Get());
			}
		}
	};

	void Register(FBulletNetworkPredictionID ID,UBulletNetworkPredictionPlayerControllerComponent* Component)
	{
		Component->RegisterInputReceiver((int32)ID,
			[this](int32 Frame, float Interp, const FBulletSimulationReplicatedInput& Input
				,const FBulletFixedTickState& TickState)
			{
				//OnFixedInputReceived(Frame, Interp, Input,TickState);
			});
	}

	void Unregister(FBulletNetworkPredictionID ID,UBulletNetworkPredictionPlayerControllerComponent* Component)
	{
		Component->UnregisterInputReceiver(ID);
	}

	void ProduceInput(int32 DeltaTimeMS, const float& InterpolationTime) final override
	{
		for (auto& MapIt : InstanceMap)
		{
			FInstance& Remote = MapIt.Value;
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(Remote.InstanceIndex);
			bnpCheckSlow(InstanceData.Info.View);
			bnpCheckSlow(InstanceData.Info.View->PendingInputCmd);
			bnpCheckSlow(InstanceData.Info.View->PendingFrame >= 0);
			
			UE_BNP_TRACE_PRODUCE_INPUT(Remote.TraceID);
			UE_BNP_TRACE_PUSH_INPUT_FRAME(InstanceData.Info.View->PendingFrame);

			TBulletServerRecvData_Fixed<ModelDef>& ServerRecvData = DataStore->ServerRecv.GetByIndexChecked(Remote.ServerRecvIdx);
			if (!InstanceData.Info.RPCHandler)
			{
				continue;
			}
			
			// Consume next InputCmd
			ServerRecvData.LastConsumedFrame = InstanceData.Info.RPCHandler->LastConsumedFrame;
			ServerRecvData.InputBuffer[InstanceData.Info.RPCHandler->LastConsumedFrame].Value.CopyTo((InputType*)InstanceData.Info.View->PendingInputCmd);
			*InstanceData.Info.View->InterpolationTimeMS = ServerRecvData.InputBuffer[InstanceData.Info.RPCHandler->LastConsumedFrame].Key;
			// Set Time Dilation Based On Consumed Input
			
			if (NetworkPredictionCVars::DisableTimeDilation() == 0)
			{
				UNetConnection* NetConnection = InstanceData.Info.RPCHandler->GetNetConnection();
				const int32 LastReceivedFrame = InstanceData.Info.RPCHandler->LastReceivedFrame;
				const int32 LastConsumedFrame = InstanceData.Info.RPCHandler->LastConsumedFrame;
				const float PingMS = NetConnection->AvgLag * 1000.f;
				// Jitter up to fixed tick time is covered by the 1 frame of fixed buffered frames.
				const float PacketLossFrames = NetConnection->InPacketsLost + NetConnection->OutPacketsLost;
				int32 JitterFrames = FMath::RoundToInt32(NetConnection->GetAverageJitterInMS() / DeltaTimeMS);
				const int32 DesiredBufferedFrames = DesiredBufferedInputs + JitterFrames + PacketLossFrames;
				int32 BufferOffset =  (LastReceivedFrame - LastConsumedFrame)  - DesiredBufferedFrames;// - ServerRecvState->InputFault;
				if (LastReceivedFrame == INDEX_NONE || LastConsumedFrame == INDEX_NONE)
				{
					BufferOffset = INT8_MAX;
				}

				/*if (BufferOffset == 0)
				{
					if (ServerRecvState->InputFault > 0)
					{
						ServerRecvState->InputFault = FMath::Max(ServerRecvState->InputFault - 1,0);
					}
					else if (ServerRecvState->InputFault < 0)
					{
						ServerRecvState->InputFault = FMath::Min(ServerRecvState->InputFault + 1,6);
					}
				}
				else if (BufferOffset > 0)
				{
					ServerRecvState->InputFault = FMath::Min(ServerRecvState->InputFault + 1,6);
				}
				else
				{
					ServerRecvState->InputFault = FMath::Max(ServerRecvState->InputFault - 1,0);
				}*/
			
				float CalculatedTimeDilation = 1.f;
				if (BufferOffset != 0 && BufferOffset != INT8_MAX)
				{
					const float TimeDilationDecay = FMath::Clamp(1.0f - (NetworkPredictionCVars::TimeDilationEscalationDecay() * FMath::Abs(BufferOffset)), NetworkPredictionCVars::TimeDilationEscalationDecayMax(), 1.0f);
					CalculatedTimeDilation = 1.0f + ((NetworkPredictionCVars::TimeDilationAmount() * BufferOffset) * TimeDilationDecay);
					CalculatedTimeDilation = FMath::Clamp(CalculatedTimeDilation, NetworkPredictionCVars::TimeDilationMin(), NetworkPredictionCVars::TimeDilationMax());
				}
				InstanceData.Info.RPCHandler->UpdateTimeDilation(CalculatedTimeDilation);
			}
			else
			{
				InstanceData.Info.RPCHandler->UpdateTimeDilation(1.f);
			}
			
			const int32 NumBufferedInputCmds = InstanceData.Info.RPCHandler->LastReceivedFrame - InstanceData.Info.RPCHandler->LastConsumedFrame;
			UE_BNP_TRACE_BUFFERED_INPUT(NumBufferedInputCmds, false);
			UE_BNP_TRACE_USER_STATE_INPUT(ModelDef, (InputType*)InstanceData.Info.View->PendingInputCmd);
		}
	}

private:
	
	struct FInstance
	{
		int32 TraceID;
		int32 InstanceIndex; // idx into DataStore->Instances::GetByIndexChecked
		
		int32 ServerRecvIdx;  // idx into DataStore->ServerRecv::GetByIndexChecked
		int32 FaultLimit = 2; // InputBuffer must have >= this number of unprocessed commands before resuming consumption
		bool bFault=true;	  // Recently starved on input buffer, wait until we reach FaultLimit before consuming input again. (Note you start out in fault to let the buffer fill up)
	};

	TSortedMap<int32, FInstance> InstanceMap;
	TBulletModelDataStore<ModelDef>* DataStore;
	
	inline static int32 MaxFaultLimit = 6;

	static void EatCmd(const FBulletNetSerializeParams& P)
	{
		TBulletConditionalState<InputType> Empty;
		FBulletNetworkPredictionDriver<ModelDef>::NetSerialize(Empty, P);  // 2. InputCmd	
	}
	
	inline static int32 DesiredBufferedInputs = 4;
};

