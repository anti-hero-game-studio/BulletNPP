// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ChaosVDSolverDataComponent.h"
#include "BulletMoverCVDSimDataComponent.generated.h"

struct FBulletMoverSyncState;
struct FBulletMoverCVDSimDataWrapper;
struct FBulletMoverInputCmdContext;
struct FBulletMoverDataCollection;

struct FDeserializedBulletMoverStates
{
	TSharedPtr<FBulletMoverSyncState> SyncState;
	TSharedPtr<FBulletMoverInputCmdContext> InputCommand;
	TSharedPtr<FBulletMoverDataCollection> LocalSimData;
};

/** Component holding BulletMover data for the current visualized frame */
UCLASS()
class UBulletMoverCVDSimDataComponent : public UChaosVDSolverDataComponent
{
	GENERATED_BODY()

public:
	// That we chose to implement this function and not UpdateFromNewGameFrameData or UpdateFromNewSolverStageData is tied
	// to the implementation of FBulletMoverCVDSimDataProcessor, which currently add ths information to FChaosVDTraceProvider::GetCurrentSolverFrame()
	// Eventually we will record information at different stages of a solver frame and will be using UpdateFromNewSolverStageData instead,
	// to show the state of the sync state at the beginning of the frame, then at the end
	virtual void UpdateFromSolverFrameData(const FChaosVDSolverFrameData& InSolverFrameData) override;

	virtual void ClearData() override;
	
	TConstArrayView<TSharedPtr<FBulletMoverCVDSimDataWrapper>> GetFrameSimDataArray() const
	{
		return FrameSimDataArray;
	}

	bool FindAndUnwrapSimDataForParticle(
		uint32 ParticleID,
		TSharedPtr<FBulletMoverCVDSimDataWrapper>& OutSimDataWrapper,
		TSharedPtr<FBulletMoverSyncState>& OutSyncState,
		TSharedPtr<FBulletMoverInputCmdContext>& OutInputCmd,
		TSharedPtr<FBulletMoverDataCollection>& OutLocalSimData);

private:
	// This is the array of FBulletMoverCVDSimDataWrapper for the current frame (as it is updated in UpdateFromNewGameFrameData)
	// and corresponding to this DataComponent's SolverID
	TArray<TSharedPtr<FBulletMoverCVDSimDataWrapper>> FrameSimDataArray;

	TMap<FBulletMoverCVDSimDataWrapper*, TSharedPtr<FDeserializedBulletMoverStates>> DeserializedStates;
};
