// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "ChaosVDTabSpawnerBase.h"
#include "Widgets/SChaosVDDetailsView.h"
#include "ChaosVDSolverDataSelection.h"
#include "ChaosVDObjectDetailsTab.h"
#include "TEDS/ChaosVDStructTypedElementData.h"

class SOutputLog;
class FOutputLogHistory;
class FName;
struct FBulletMoverSyncState;
struct FBulletMoverInputCmdContext;
struct FBulletMoverCVDSimDataWrapper;
struct FBulletMoverDataCollection;
class UBulletMoverCVDSimDataComponent;

/** This tab is an additional details tab displaying BulletMover info corresponding to the selected particles if they are moved by a BulletMover component */
class FBulletMoverCVDTab : public FChaosVDObjectDetailsTab
{
public:

	FBulletMoverCVDTab(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, TWeakPtr<SChaosVDMainTab> InOwningTabWidget);
	virtual ~FBulletMoverCVDTab();

	// Implementation of FChaosVDObjectDetailsTab
	virtual TSharedRef<SDockTab> HandleTabSpawnRequest(const FSpawnTabArgs& Args) override;
	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangedSelectionSet) override;

	virtual void HandleSolverDataSelectionChange(const TSharedPtr<FChaosVDSolverDataSelectionHandle>& SelectionHandle) override;

	// Scene callbacks
	void HandleSceneUpdated();

private:
	void DisplaySingleParticleInfo(int32 SelectedSolverID, int32 SelectedParticleID);
	void DisplayBulletMoverInfoForSelectedElements(const TArray<FTypedElementHandle>& SelectedElementHandles);

	// This function retrieves and caches all the BulletMover data components for all solvers, populating SolverToSimDataComponentMap
	void RetrieveAllSolversBulletMoverDataComponents();

	TWeakObjectPtr<UBulletMoverCVDSimDataComponent>* FindBulletMoverDataComponentForSolver(int32 SolverID);

	TWeakPtr<FChaosVDScene> SceneWeakPtr;

	TMap<int32, TWeakObjectPtr<UBulletMoverCVDSimDataComponent>> SolverToSimDataComponentMap;

	int32 CurrentlyDisplayedParticleID = INDEX_NONE;
	int32 CurrentlyDisplayedSolverID = INDEX_NONE;

	FChaosVDSelectionMultipleView MultiViewWrapper;

	TSharedPtr<FBulletMoverCVDSimDataWrapper> BulletMoverSimDataWrapper;
	TSharedPtr<FBulletMoverSyncState> BulletMoverSyncState;
	TSharedPtr<FBulletMoverInputCmdContext> BulletMoverInputCmd;
	TSharedPtr<FBulletMoverDataCollection> BulletMoverLocalSimData;
};


