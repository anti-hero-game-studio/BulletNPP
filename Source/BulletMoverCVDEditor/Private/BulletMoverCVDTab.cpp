// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverCVDTab.h"

#include "ChaosLog.h"
#include "ChaosVDModule.h"
#include "ChaosVDScene.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SChaosVDMainTab.h"
#include "BulletMoverCVDStyle.h"
#include "BulletMoverSimulationTypes.h"
#include "BulletMoverCVDDataWrappers.h"
#include "BulletMoverCVDSimDataComponent.h"
#include "TEDS/ChaosVDSelectionInterface.h"
#include "ChaosVDSceneParticle.h"

#define LOCTEXT_NAMESPACE "BulletMoverCVDExtension"

FBulletMoverCVDTab::FBulletMoverCVDTab(const FName& InTabID, TSharedPtr<FTabManager> InTabManager, TWeakPtr<SChaosVDMainTab> InOwningTabWidget)
	: FChaosVDObjectDetailsTab(InTabID, InTabManager, InOwningTabWidget)
{
	// Cache a scene pointer for future reference
	if (TSharedPtr<SChaosVDMainTab> OwningTabWidgetSharedPtr = OwningTabWidget.Pin())
	{
		if (TSharedPtr<FChaosVDScene> Scene = OwningTabWidgetSharedPtr->GetScene())
		{
			SceneWeakPtr = Scene;
		}
	}
}

FBulletMoverCVDTab::~FBulletMoverCVDTab()
{
	if (TSharedPtr<FChaosVDScene> Scene = SceneWeakPtr.Pin())
	{
		Scene->OnSceneUpdated().RemoveAll(this);
	}
}

TSharedRef<SDockTab> FBulletMoverCVDTab::HandleTabSpawnRequest(const FSpawnTabArgs& Args)
{
	// I can't add callbacks in the constructor as SharedFromThis would complain
	if (TSharedPtr<FChaosVDScene> Scene = SceneWeakPtr.Pin())
	{
		Scene->OnSceneUpdated().AddSP(this, &FBulletMoverCVDTab::HandleSceneUpdated);
	}

	TSharedRef<SDockTab> Tab = FChaosVDObjectDetailsTab::HandleTabSpawnRequest(Args);

	// Let's rename the tab so its name is unique
	Tab->SetLabel(Args.GetTabId().ToText());

	Tab->SetTabIcon(FBulletMoverCVDStyle::Get().GetBrush("TabIconBulletMoverInfoPanel"));

	return Tab;
}

void FBulletMoverCVDTab::HandleSolverDataSelectionChange(const TSharedPtr<FChaosVDSolverDataSelectionHandle>& SelectionHandle)
{
	// We override the default behavior, otherwise we will display any struct that gets selected and can be displayed
	// while we only want to display BulletMover info
	return;
}

void FBulletMoverCVDTab::RetrieveAllSolversBulletMoverDataComponents()
{
	SolverToSimDataComponentMap.Empty();

	const TSharedPtr<FChaosVDScene> Scene = SceneWeakPtr.Pin();
	if (Scene)
	{
		const FChaosVDSolverInfoByIDMap& SolverInfoByIDMap = Scene->GetSolverInfoActorsMap();
		for (const auto& It : SolverInfoByIDMap)
		{
			int32 SolverID = It.Key;
			// No need to know about AChaosVDSolverInfoActor as we intend to solely find an actor component,
			// but also at the time of writing, AChaosVDSolverInfoActor was no a public include
			const AActor* SolverInfoActor = reinterpret_cast<const AActor*>(It.Value);
			if (SolverInfoActor)
			{
				if (UBulletMoverCVDSimDataComponent* SolverSimDataComponent = SolverInfoActor->FindComponentByClass<UBulletMoverCVDSimDataComponent>())
				{
					SolverToSimDataComponentMap.Add(SolverID, SolverSimDataComponent);
				}
			}
		}
	}
}

TWeakObjectPtr<UBulletMoverCVDSimDataComponent>* FBulletMoverCVDTab::FindBulletMoverDataComponentForSolver(int32 SolverID)
{
	TWeakObjectPtr<UBulletMoverCVDSimDataComponent>* FoundSimDataComponent = SolverToSimDataComponentMap.Find(SolverID);
	if (!FoundSimDataComponent || FoundSimDataComponent->IsStale())
	{
		// Refresh SolverToSimDataComponentMap
		RetrieveAllSolversBulletMoverDataComponents();

		// Try again
		FoundSimDataComponent = SolverToSimDataComponentMap.Find(SolverID);
	}

	return FoundSimDataComponent;
}

void FBulletMoverCVDTab::DisplaySingleParticleInfo(int32 SelectedSolverID, int32 SelectedParticleID)
{
	// At the moment this is really written to support only one particle selected at a time
	// I we were to have multiple selection we would have to cache the whole list of particle IDs selected
	bool IsParticleBeingSelected = (SelectedParticleID != INDEX_NONE && SelectedSolverID != INDEX_NONE);
	bool WasDataFoundForParticle = false;
	if (IsParticleBeingSelected)
	{
		// Find the sim data component weak pointer for the selected solver ID
		TWeakObjectPtr<UBulletMoverCVDSimDataComponent>* FoundSimDataComponent = FindBulletMoverDataComponentForSolver(SelectedSolverID);
		if (FoundSimDataComponent)
		{
			// Pin a strong pointer to it so it doesn't get away while in use
			TStrongObjectPtr<UBulletMoverCVDSimDataComponent> SimDataComponentSharedPtr = FoundSimDataComponent->Pin();
			UBulletMoverCVDSimDataComponent* SimDataComponent = SimDataComponentSharedPtr.Get();
			if (IsValid(SimDataComponent))
			{
				if (SimDataComponent->FindAndUnwrapSimDataForParticle(SelectedParticleID, BulletMoverSimDataWrapper, BulletMoverSyncState, BulletMoverInputCmd, BulletMoverLocalSimData))
				{
					if (BulletMoverSimDataWrapper && BulletMoverInputCmd && BulletMoverSyncState)
					{
						if (DetailsPanelView)
						{
							MultiViewWrapper.Clear();

							// This displays the particle ID and solver ID
							MultiViewWrapper.AddData(BulletMoverSimDataWrapper.Get());

							// Input Command Context structs
							for (const TSharedPtr<FBulletMoverDataStructBase>& InputDataStructSharedPtr : BulletMoverInputCmd->Collection.GetDataArray())
							{
								if (FBulletMoverDataStructBase* BulletMoverDataStructBase = InputDataStructSharedPtr.Get())
								{
									MultiViewWrapper.AddData(MakeShared<FStructOnScope>(BulletMoverDataStructBase->GetScriptStruct(), reinterpret_cast<uint8*>(BulletMoverDataStructBase)));
								}
							}

							MultiViewWrapper.AddData(BulletMoverSyncState.Get());
							// Sync State Data Collection structs
							for (const TSharedPtr<FBulletMoverDataStructBase>& InputDataStructSharedPtr : BulletMoverSyncState->Collection.GetDataArray())
							{
								if (FBulletMoverDataStructBase* BulletMoverDataStructBase = InputDataStructSharedPtr.Get())
								{
									MultiViewWrapper.AddData(MakeShared<FStructOnScope>(BulletMoverDataStructBase->GetScriptStruct(), reinterpret_cast<uint8*>(BulletMoverDataStructBase)));
								}
							}

							// Local Simulation State structs
							if (BulletMoverLocalSimData)
							{
								for (const TSharedPtr<FBulletMoverDataStructBase>& LocalSimDataStructSharedPtr : BulletMoverLocalSimData->GetDataArray())
								{
									if (FBulletMoverDataStructBase* BulletMoverDataStructBase = LocalSimDataStructSharedPtr.Get())
									{
										MultiViewWrapper.AddData(MakeShared<FStructOnScope>(BulletMoverDataStructBase->GetScriptStruct(), reinterpret_cast<uint8*>(BulletMoverDataStructBase)));
									}
								}
							}

							WasDataFoundForParticle = true;

							SetStructToInspect(&MultiViewWrapper);
						}
					}
					// Cache the particle ID so we can display info for that same particle when we scrub to a different frame that also has that particle
					// WARNING: Apparently IDs can be reused across frames for different particles, so this might not be good enough
					CurrentlyDisplayedSolverID = SelectedSolverID;
					CurrentlyDisplayedParticleID = SelectedParticleID;
				}
			}
		}
	}
	
	if (!WasDataFoundForParticle)
	{
		CurrentlyDisplayedSolverID = INDEX_NONE;
		CurrentlyDisplayedParticleID = INDEX_NONE;
	}
}

void FBulletMoverCVDTab::DisplayBulletMoverInfoForSelectedElements(const TArray<FTypedElementHandle>& SelectedElementHandles)
{
	if (DetailsPanelView)
	{
		DetailsPanelView->SetSelectedStruct(nullptr);

		bool ShouldClearSimDataDetailsPanel = true;
		using namespace Chaos::VD::TypedElementDataUtil;
		for (int32 SelectionIndex = 0; SelectionIndex < SelectedElementHandles.Num(); ++SelectionIndex)
		{
			if (const FChaosVDSceneParticle* Particle = GetStructDataFromTypedElementHandle<FChaosVDSceneParticle>(SelectedElementHandles[SelectionIndex]))
			{
				int32 SelectedParticleID = Particle->GetParticleData()->ParticleIndex; // Replace with ID of selected particle if one is selected
				int32 SelectedSolverID = Particle->GetParticleData()->SolverID;
				ShouldClearSimDataDetailsPanel = false;
				DisplaySingleParticleInfo(SelectedSolverID, SelectedParticleID);

				// Right now, we only handle displaying info for the first particle in the selection, but later we might not break here and display info for all particles
				break;
			}
		}

		// Clear the details panel view if we didn't find particles in the selection
		if (ShouldClearSimDataDetailsPanel)
		{
			DetailsPanelView->SetSelectedStruct(nullptr);
		}
	}
}

void FBulletMoverCVDTab::HandleSceneUpdated()
{
	// This is called when we scrub the timelines for instance
	bool IsParticleSelected = (CurrentlyDisplayedSolverID != INDEX_NONE && CurrentlyDisplayedParticleID != INDEX_NONE);

	if (!IsParticleSelected)
	{
		return;
	}

	const TSharedPtr<FChaosVDScene> Scene = SceneWeakPtr.Pin();
	if (!Scene)
	{
		return;
	}

	bool ShouldClearSimDataDetailsPanel = true;
	using namespace Chaos::VD::TypedElementDataUtil;
	TArray<FTypedElementHandle> SelectedParticlesHandles = Scene->GetSelectedElementHandles();
	DisplayBulletMoverInfoForSelectedElements(SelectedParticlesHandles);
}

void FBulletMoverCVDTab::HandlePostSelectionChange(const UTypedElementSelectionSet* ChangedSelectionSet)
{
	// This is called when the selection changes
	TArray<FTypedElementHandle> SelectedParticlesHandles;
	ChangedSelectionSet->GetSelectedElementHandles(SelectedParticlesHandles, UChaosVDSelectionInterface::StaticClass());	
	DisplayBulletMoverInfoForSelectedElements(SelectedParticlesHandles);
}

#undef LOCTEXT_NAMESPACE

