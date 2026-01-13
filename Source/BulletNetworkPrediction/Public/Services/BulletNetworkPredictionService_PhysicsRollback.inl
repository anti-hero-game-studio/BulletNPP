// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_SKIP - Not included directly

#include "BulletNetworkPredictionCVars.h"
#include "BulletNetworkPredictionLog.h"
#include "BulletNetworkPredictionService_PhysicsTick.inl"
#include "Services/BulletNetworkPredictionInstanceData.h"

namespace NetworkPredictionCVars
{
	BULLETNETSIM_DEVCVAR_SHIPCONST_INT(ForcePhysicsReconcile,			0, "b.np.ForcePhysicsReconcile",				"Force a single reconcile back to the last server-acknoledged frame. When used with np.ForceReconcileExtraFrames, additional frames can be rolled back. No effect on server. Resets after use.");
	BULLETNETSIM_DEVCVAR_SHIPCONST_INT(ForcePhysicsReconcileExtraFrames, 0, "b.np.ForcePhysicsReconcileExtraFrames",	"Roll back this extra number of frames during the next targeted reconcile. Must be positive and reasonable given the buffer sizes.");
	BULLETNETSIM_DEVCVAR_SHIPCONST_INT(SkipPhysicsReconcile,				0, "b.np.SkipPhysicsReconcile",				"Skip all reconciles");
	BULLETNETSIM_DEVCVAR_SHIPCONST_INT(PrintPhysicsReconciles,			0, "b.np.PrintPhysicsReconciles",			"Print reconciles to log");
}

class IBulletFixedPhysicsRollbackService
{
public:

	virtual ~IBulletFixedPhysicsRollbackService() = default;
	virtual int32 QueryRollback(FBulletFixedTickState* TickState) = 0;

	virtual void PreStepRollback(const FBulletNetSimTimeStep& Step, const FBulletServiceTimeStep& ServiceStep, const int32 Offset, const bool bFirstStepInResim) = 0;
	virtual void StepRollback(const FBulletNetSimTimeStep& Step, const FBulletServiceTimeStep& ServiceStep) = 0;
};

template<typename InModelDef>
class TBulletFixedPhysicsRollbackService : public IBulletFixedPhysicsRollbackService
{
public:

	using ModelDef = InModelDef;
	using StateTypes = typename ModelDef::StateTypes;
	using SyncAuxType = TBulletSyncAuxPair<StateTypes>;

	static constexpr bool bNeedsTickService = FBulletNetworkPredictionDriver<ModelDef>::HasSimulation();

	TBulletFixedPhysicsRollbackService(TBulletModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore), InternalTickService(InDataStore) { }

	void RegisterInstance(FBulletNetworkPredictionID ID)
	{
		const int32 ClientRecvIdx = DataStore->ClientRecv.GetIndexChecked(ID);
		BnpResizeAndSetBit(InstanceBitArray, ClientRecvIdx);

		if (bNeedsTickService)
		{
			InternalTickService.RegisterInstance(ID);
		}
	}

	void UnregisterInstance(FBulletNetworkPredictionID ID)
	{
		const int32 ClientRecvIdx = DataStore->ClientRecv.GetIndexChecked(ID);
		InstanceBitArray[ClientRecvIdx] = false;
		
		if (bNeedsTickService)
		{
			InternalTickService.UnregisterInstance(ID);
		}
	}

	int32 QueryRollback(FBulletFixedTickState* TickState) final override
	{
		bnpCheckSlow(TickState);
		BnpClearBitArray(RollbackBitArray);

		// DataStore->ClientRecvBitMask size can change without us knowing so make sure out InstanceBitArray size stays in sync
		BnpResizeBitArray(InstanceBitArray, DataStore->ClientRecvBitMask.Num());

		const int32 Offset = TickState->Offset;
		int32 RollbackFrame = INDEX_NONE;
		for (TConstDualSetBitIterator<FDefaultBitArrayAllocator,FDefaultBitArrayAllocator> BitIt(InstanceBitArray, DataStore->ClientRecvBitMask); BitIt; ++BitIt)
		{
			const int32 ClientRecvIdx = BitIt.GetIndex();
			TBulletClientRecvData<ModelDef>& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);
			TBulletInstanceFrameState<ModelDef>& Frames = DataStore->Frames.GetByIndexChecked(ClientRecvData.FramesIdx);

			UE_BNP_TRACE_SIM(ClientRecvData.TraceID);
			
			const int32 LocalFrame = ClientRecvData.ServerFrame - Offset;
			typename TBulletInstanceFrameState<ModelDef>::FFrame& LocalFrameData = Frames.Buffer[LocalFrame];

			bool bDoRollback = false;

			if (NetworkPredictionCVars::ForceReconcile() > 0)
			{
				UE_BNP_TRACE_SHOULD_RECONCILE(ClientRecvData.TraceID);
				bDoRollback = true;
				RollbackFrame = LocalFrame - FMath::Max(0, NetworkPredictionCVars::ForceReconcileExtraFrames());

				if (NetworkPredictionCVars::PrintReconciles())
				{				
					UE_LOG(LogBulletNetworkPrediction, Warning, TEXT("Reconcile activated due to ForceReconcile (to RollbackFrame=%i, including %i extra rollback frames)"), RollbackFrame, -(RollbackFrame-LocalFrame));
				}

				NetworkPredictionCVars::SetForceReconcile(0); // reset
			}
			else if (FBulletNetworkPredictionDriver<ModelDef>::ShouldReconcile( SyncAuxType(LocalFrameData.SyncState, LocalFrameData.AuxState), SyncAuxType(ClientRecvData.SyncState, ClientRecvData.AuxState) ))
			{
				UE_BNP_TRACE_SHOULD_RECONCILE(ClientRecvData.TraceID);
				bDoRollback = true;
				
				if (NetworkPredictionCVars::PrintReconciles())
				{
					UE_LOG(LogBulletNetworkPrediction, Warning, TEXT("Reconcile required due to Sync/Aux mismatch. LocalFrame: %d. Recv Frame: %d. Offset: %d. Idx: %d"), LocalFrame, ClientRecvData.ServerFrame, Offset, LocalFrame % Frames.Buffer.Capacity());

					UE_LOG(LogBulletNetworkPrediction, Warning, TEXT("Received:"));
					FBulletNetworkPredictionDriver<ModelDef>::LogUserStates({ClientRecvData.InputCmd, ClientRecvData.SyncState, ClientRecvData.AuxState });

					UE_LOG(LogBulletNetworkPrediction, Warning, TEXT("Local:"));
					FBulletNetworkPredictionDriver<ModelDef>::LogUserStates({LocalFrameData.InputCmd, LocalFrameData.SyncState, LocalFrameData.AuxState });
				}
			}

			if (bDoRollback && !NetworkPredictionCVars::SkipReconcile())
			{
				RollbackFrame = (RollbackFrame == INDEX_NONE) ? LocalFrame : FMath::Min(RollbackFrame, LocalFrame);
			}
			else
			{
				// Copy received InputCmd to head. This feels a bit out of place here but is ok for now.
				//	-If we rollback, this isn't needed since rollback will copy the cmd (someone else could cause the rollback though, making this redundant)
				//	-Making a second "no rollback happening" pass on all SPs is an option but the branch here seems better, this is the only place we are touching the head frame buffer though...
				if (ClientRecvData.NetRole == ROLE_SimulatedProxy)
				{
					typename TBulletInstanceFrameState<ModelDef>::FFrame& PendingFrameData = Frames.Buffer[TickState->PendingFrame];
					PendingFrameData.InputCmd = ClientRecvData.InputCmd;
				}
			}
			// Regardless if this instance needs to rollback or not, we are marking it in the RollbackBitArray.
			// This could be a ModelDef setting ("Rollback everyone" or "Just who needs it") 
			// Or maybe something more dynamic/spatial ("rollback all instances within this radius", though to do this you may need to consider some ModelDef independent way of doing so)
			BnpResizeAndSetBit(RollbackBitArray, ClientRecvIdx);

			// We've taken care of this instance, reset it for next time
			DataStore->ClientRecvBitMask[ClientRecvIdx] = false;
		}
		
		return RollbackFrame;
	}

	void PreStepRollback(const FBulletNetSimTimeStep& Step, const FBulletServiceTimeStep& ServiceStep, const int32 Offset, const bool bFirstStepInResim)
	{
		if (bFirstStepInResim)
		{
			// Apply corrections for the instances that have corrections on this frame
			ApplyCorrection<false>(ServiceStep.LocalInputFrame, Offset);

			// Everyone must rollback Cue dispatcher and flush
			InternalTickService.BeginRollback(ServiceStep.LocalInputFrame, Step.TotalSimulationTime, Step.Frame);
			
			// Everyone we are managing needs to rollback to this frame, even if they don't have a correction 
			// (this frame or this rollback - they will need to restore their collision data since we are about to retick everyone in step)

			QUICK_SCOPE_CYCLE_COUNTER(BNP_Rollback_RestorePhysicsFrame);
			TRACE_CPUPROFILER_EVENT_SCOPE(BulletNetworkPrediction::RestorePhysicsFrame);

			for (TConstSetBitIterator<> BitIt(InstanceBitArray); BitIt; ++BitIt)
			{
				TBulletClientRecvData<ModelDef>& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(BitIt.GetIndex());
				TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(ClientRecvData.InstanceIdx);
				TBulletInstanceFrameState<ModelDef>& Frames = DataStore->Frames.GetByIndexChecked(ClientRecvData.FramesIdx);
				typename TBulletInstanceFrameState<ModelDef>::FFrame& LocalFrameData = Frames.Buffer[ServiceStep.LocalInputFrame];

				FBulletNetworkPredictionDriver<ModelDef>::RestorePhysicsFrame(InstanceData.Info.Driver, LocalFrameData.SyncState.Get(), LocalFrameData.AuxState.Get());
			}
		}
		else
		{
			ApplyCorrection<true>(ServiceStep.LocalInputFrame, Offset);
		}
	}

	void StepRollback(const FBulletNetSimTimeStep& Step, const FBulletServiceTimeStep& ServiceStep) final override
	{
		if (bNeedsTickService)
		{
			InternalTickService.TickResim(Step, ServiceStep);
		}
	}	

private:

	template<bool FlushCorrection>
	void ApplyCorrection(const int32 LocalInputFrame, const int32 Offset)
	{
		// Insert correction data on the right frame
		for (TConstSetBitIterator<> BitIt(RollbackBitArray); BitIt; ++BitIt)
		{
			const int32 ClientRecvIdx = BitIt.GetIndex();
			TBulletClientRecvData<ModelDef>& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);

			const int32 LocalFrame = ClientRecvData.ServerFrame - Offset;
			if (LocalFrame == LocalInputFrame)
			{
				// Time to inject
				TBulletInstanceFrameState<ModelDef>& Frames = DataStore->Frames.GetByIndexChecked(ClientRecvData.FramesIdx);
				typename TBulletInstanceFrameState<ModelDef>::FFrame& LocalFrameData = Frames.Buffer[LocalFrame];
				LocalFrameData.SyncState = ClientRecvData.SyncState;
				LocalFrameData.AuxState = ClientRecvData.AuxState;

				TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(ClientRecvData.InstanceIdx);

				// Copy input cmd if SP
				if (ClientRecvData.NetRole == ROLE_SimulatedProxy)
				{
					LocalFrameData.InputCmd = ClientRecvData.InputCmd;
				}

				RollbackBitArray[ClientRecvIdx] = false;
				UE_BNP_TRACE_ROLLBACK_INJECT(ClientRecvData.TraceID);

				if (FlushCorrection)
				{
					// Push to component/collision scene immediately (we aren't garunteed to tick next, so get our collision right)
					FBulletNetworkPredictionDriver<ModelDef>::RestorePhysicsFrame(InstanceData.Info.Driver, LocalFrameData.SyncState.Get(), LocalFrameData.AuxState.Get());
				}
			}
		}
	}
	
	TBitArray<> InstanceBitArray; // Indices into DataStore->ClientRecv that we are managing
	TBitArray<> RollbackBitArray; // Indices into DataStore->ClientRecv that we should rollback

	TBulletModelDataStore<ModelDef>* DataStore;

	TBulletLocalPhysicsService<ModelDef>	InternalTickService;
};

// ------------------------------------------------------------------------------------------------

class IBulletIndependentPhysicsRollbackService
{
public:

	virtual ~IBulletIndependentPhysicsRollbackService() = default;
	virtual void Reconcile(const FBulletVariableTickState* TickState) = 0;
};

template<typename InModelDef>
class TBulletIndependentPhysicsRollbackService : public IBulletIndependentPhysicsRollbackService
{
public:

	using ModelDef = InModelDef;
	using StateTypes = typename ModelDef::StateTypes;
	using SyncAuxType = TBulletSyncAuxPair<StateTypes>;

	TBulletIndependentPhysicsRollbackService(TBulletModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore) { }

	void RegisterInstance(FBulletNetworkPredictionID ID)
	{
		const int32 ClientRecvIdx = DataStore->ClientRecv.GetIndexChecked(ID);
		BnpResizeAndSetBit(InstanceBitArray, ClientRecvIdx);

		// Only APs should register for this service. We do not support rollback for independent tick SP actors.
		bnpEnsureSlow(DataStore->Instances.GetByIndexChecked( DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx).InstanceIdx ).NetRole == ROLE_AutonomousProxy);
	}

	void UnregisterInstance(FBulletNetworkPredictionID ID)
	{
		const int32 ClientRecvIdx = DataStore->ClientRecv.GetIndexChecked(ID);
		InstanceBitArray[ClientRecvIdx] = false;
	}

	void Reconcile(const FBulletVariableTickState* TickState) final override
	{
		// DataStore->ClientRecvBitMask size can change without us knowing so make sure out InstanceBitArray size stays in sync
		BnpResizeBitArray(InstanceBitArray, DataStore->ClientRecvBitMask.Num());

		for (TConstDualSetBitIterator<FDefaultBitArrayAllocator,FDefaultBitArrayAllocator> BitIt(InstanceBitArray, DataStore->ClientRecvBitMask); BitIt; ++BitIt)
		{
			const int32 ClientRecvIdx = BitIt.GetIndex();
			TBulletClientRecvData<ModelDef>& ClientRecvData = DataStore->ClientRecv.GetByIndexChecked(ClientRecvIdx);
			TBulletInstanceFrameState<ModelDef>& Frames = DataStore->Frames.GetByIndexChecked(ClientRecvData.FramesIdx);

			const int32 LocalFrame = ClientRecvData.ServerFrame;
			typename TBulletInstanceFrameState<ModelDef>::FFrame& LocalFrameData = Frames.Buffer[LocalFrame];

			if (FBulletNetworkPredictionDriver<ModelDef>::ShouldReconcile( SyncAuxType(LocalFrameData.SyncState, LocalFrameData.AuxState), SyncAuxType(ClientRecvData.SyncState, ClientRecvData.AuxState) ))
			{
				UE_BNP_TRACE_SHOULD_RECONCILE(ClientRecvData.TraceID);
				if (NetworkPredictionCVars::PrintReconciles())
				{
					UE_LOG(LogBulletNetworkPrediction, Warning, TEXT("ShouldReconcile. Frame: %d."), LocalFrame);

					UE_LOG(LogBulletNetworkPrediction, Warning, TEXT("Received:"));
					FBulletNetworkPredictionDriver<ModelDef>::LogUserStates({ClientRecvData.InputCmd, ClientRecvData.SyncState, ClientRecvData.AuxState });

					UE_LOG(LogBulletNetworkPrediction, Warning, TEXT("Local:"));
					FBulletNetworkPredictionDriver<ModelDef>::LogUserStates({LocalFrameData.InputCmd, LocalFrameData.SyncState, LocalFrameData.AuxState });
				}

				LocalFrameData.SyncState = ClientRecvData.SyncState;
				LocalFrameData.AuxState = ClientRecvData.AuxState;

				TInstanceData<ModelDef>& Instance = DataStore->Instances.GetByIndexChecked(ClientRecvData.InstanceIdx);

				FBulletNetworkPredictionDriver<ModelDef>::RestorePhysicsFrame(Instance.Info.Driver, LocalFrameData.SyncState.Get(), LocalFrameData.AuxState.Get());

				// Do rollback
				const int32 EndFrame = TickState->PendingFrame;
				for (int32 Frame = LocalFrame; Frame < EndFrame; ++Frame)
				{
					const int32 InputFrame = Frame;
					const int32 OutputFrame = Frame+1;

					typename TBulletInstanceFrameState<ModelDef>::FFrame& InputFrameData = Frames.Buffer[InputFrame];
					typename TBulletInstanceFrameState<ModelDef>::FFrame& OutputFrameData = Frames.Buffer[OutputFrame];

					const FBulletVariableTickState::FFrame& TickData = TickState->Frames[InputFrame];

					FBulletNetSimTimeStep Step {TickData.DeltaMS, TickData.TotalMS, OutputFrame };

					const int32 EndTimeMS = TickData.TotalMS + TickData.DeltaMS;

					TBulletTickUtil<ModelDef>::DoTick(Instance, InputFrameData, OutputFrameData, Step, EndTimeMS, EBulletSimulationTickContext::Resimulate);

					UE_BNP_TRACE_PUSH_TICK(Step.TotalSimulationTime, Step.StepMS, Step.Frame);
					UE_BNP_TRACE_SIM_TICK(ClientRecvData.TraceID);
				}
			}

			// We've taken care of this instance, reset it for next time
			DataStore->ClientRecvBitMask[ClientRecvIdx] = false;
		}
	}

private:

	TBitArray<> InstanceBitArray; // Indices into DataStore->ClientRecv that we are managing
	TBulletModelDataStore<ModelDef>* DataStore;
};
