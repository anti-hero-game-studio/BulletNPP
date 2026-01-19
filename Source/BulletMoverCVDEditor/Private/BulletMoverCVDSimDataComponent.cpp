// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverCVDSimDataComponent.h"

#include "ChaosVDRecording.h"
#include "ChaosVDScene.h"
#include "BulletMoverCVDDataWrappers.h"
#include "Actors/ChaosVDDataContainerBaseActor.h"
#include "BulletMoverCVDTab.h"
#include "BulletMoverSimulationTypes.h"
#include "ChaosVisualDebugger/BulletMoverCVDRuntimeTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMoverCVDSimDataComponent)

void UBulletMoverCVDSimDataComponent::UpdateFromSolverFrameData(const FChaosVDSolverFrameData& InSolverFrameData)
{
	Super::UpdateFromSolverFrameData(InSolverFrameData);

	if (TSharedPtr<FBulletMoverCVDSimDataContainer> SimDataContainer = InSolverFrameData.GetCustomData().GetData<FBulletMoverCVDSimDataContainer>())
	{
		if (const TArray<TSharedPtr<FBulletMoverCVDSimDataWrapper>>* RecordedData = SimDataContainer->SimDataBySolverID.Find(SolverID))
		{
			// Load the recorded data into the Physics BulletMover CVD component
			FrameSimDataArray.Reset(RecordedData->Num());
			FrameSimDataArray = *RecordedData;

			// Also, clear all cached deserialized data, we're starting from scratch at a new frame
			DeserializedStates.Empty();
		}
	}
}

void UBulletMoverCVDSimDataComponent::ClearData()
{
	FrameSimDataArray.Reset();
}

bool UBulletMoverCVDSimDataComponent::FindAndUnwrapSimDataForParticle(uint32 ParticleID, TSharedPtr<FBulletMoverCVDSimDataWrapper>& OutSimDataWrapper, TSharedPtr<FBulletMoverSyncState>& OutSyncState, TSharedPtr<FBulletMoverInputCmdContext>& OutInputCmd, TSharedPtr<FBulletMoverDataCollection>& OutLocalSimData)
{
	// Look for a sim data corresponding to ParticleID
	TSharedPtr<FBulletMoverCVDSimDataWrapper>* FoundSimData = FrameSimDataArray.FindByPredicate
		(
			[&](const TSharedPtr<FBulletMoverCVDSimDataWrapper>& SimData)
			{
				return (SimData->HasValidData() && (SimData->ParticleID == ParticleID));
			}
		);

	if (!FoundSimData || !*FoundSimData)
	{
		return false;
	}

	// We use the data wrapper pointer as key in the arrays of deserialized structs (input command, sync state)
	FBulletMoverCVDSimDataWrapper* SimDataPtr = (*FoundSimData).Get();
	OutSimDataWrapper = *FoundSimData;

	// Did we previously deserialize?
	TSharedPtr<FDeserializedBulletMoverStates> DeserializedBulletMoverStates = DeserializedStates.FindOrAdd(SimDataPtr);
	if (!DeserializedBulletMoverStates)
	{
		DeserializedBulletMoverStates = MakeShared<FDeserializedBulletMoverStates>();
		// This means that the SimData wasn't deserialized yet, so we do it now
		// Otherwise we use the cached version
		UE::BulletMoverUtils::FBulletMoverCVDRuntimeTrace::UnwrapSimData(*SimDataPtr, DeserializedBulletMoverStates->InputCommand, DeserializedBulletMoverStates->SyncState, DeserializedBulletMoverStates->LocalSimData);
	}
	if (DeserializedBulletMoverStates)
	{
		OutSyncState = DeserializedBulletMoverStates->SyncState;
		OutInputCmd = DeserializedBulletMoverStates->InputCommand;
		OutLocalSimData = DeserializedBulletMoverStates->LocalSimData;
	}

	return true;
}
