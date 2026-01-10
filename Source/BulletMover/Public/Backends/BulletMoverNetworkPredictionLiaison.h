// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Backends/BulletMoverBackendLiaison.h"
#include "BulletNetworkPredictionComponent.h"
#include "BulletNetworkPredictionSimulation.h"
#include "BulletNetworkPredictionTickState.h"
#include "BulletMovementMode.h"
#include "BulletMoverTypes.h"

#include "BulletMoverNetworkPredictionLiaison.generated.h"

#define UE_API BULLETMOVER_API

class UBulletMoverComponent;


using KinematicMoverStateTypes = TBulletNetworkPredictionStateTypes<FBulletMoverInputCmdContext, FBulletMoverSyncState, FBulletMoverAuxStateContext>;

/**
 * MoverNetworkPredictionLiaisonComponent: this component acts as a middleman between an actor's Mover component and the Network Prediction plugin.
 * This class is set on a Mover component as the "back end".
 */
UCLASS(MinimalAPI)
class UBulletMoverNetworkPredictionLiaisonComponent : public UBulletNetworkPredictionComponent, public IBulletMoverBackendLiaisonInterface
{
	GENERATED_BODY()

public:
	// Begin NP Driver interface
	// Get latest local input prior to simulation step. Called by Network Prediction system on owner's instance (autonomous or authority).
	UE_API void ProduceInput(const int32 DeltaTimeMS, FBulletMoverInputCmdContext* Cmd);

	// Restore a previous frame prior to resimulating. Called by Network Prediction system.
	UE_API void RestoreFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState);
	
	// Restore a previous frame prior to resimulating. Called by Network Prediction system.
	UE_API void RestorePhysicsFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState);

	// Take output for simulation. Called by Network Prediction system.
	UE_API void FinalizeFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState);

	// Take output for smoothing. Called by Network Prediction system.
	UE_API void FinalizeSmoothingFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState);

	// Seed initial values based on component's state. Called by Network Prediction system.
	UE_API void InitializeSimulationState(FBulletMoverSyncState* OutSync, FBulletMoverAuxStateContext* OutAux);

	// Primary movement simulation update. Given an starting state and timestep, produce a new state. Called by Network Prediction system.
	UE_API void SimulationTick(const FBulletNetSimTimeStep& TimeStep, const TBulletNetSimInput<KinematicMoverStateTypes>& SimInput, const TBulletNetSimOutput<KinematicMoverStateTypes>& SimOutput);
	UE_API void PostPhysicsTick(const FBulletNetSimTimeStep& TimeStep, const TBulletNetSimInput<KinematicMoverStateTypes>& SimInput, const TBulletNetSimOutput<KinematicMoverStateTypes>& SimOutput);
	// End NP Driver interface

	// IBulletMoverBackendLiaisonInterface
	UE_API virtual double GetCurrentSimTimeMs() override;
	UE_API virtual int32 GetCurrentSimFrame() override;
	UE_API virtual bool ReadPendingSyncState(OUT FBulletMoverSyncState& OutSyncState) override;
	UE_API virtual bool WritePendingSyncState(const FBulletMoverSyncState& SyncStateToWrite) override;
	UE_API virtual bool ReadPresentationSyncState(OUT FBulletMoverSyncState& OutSyncState) override;
	UE_API virtual bool WritePresentationSyncState(const FBulletMoverSyncState& SyncStateToWrite) override;
	UE_API virtual bool ReadPrevPresentationSyncState(FBulletMoverSyncState& OutSyncState) override;
	UE_API virtual bool WritePrevPresentationSyncState(const FBulletMoverSyncState& SyncStateToWrite) override;
#if WITH_EDITOR
	UE_API virtual EDataValidationResult ValidateData(FDataValidationContext& Context, const UBulletMoverComponent& ValidationMoverComp) const override;
#endif
	// End IBulletMoverBackendLiaisonInterface

	UE_API virtual void BeginPlay() override;

	// UObject interface
	UE_API void InitializeComponent() override;
	UE_API void UninitializeComponent() override;
	UE_API void OnRegister() override;
	UE_API void RegisterComponentTickFunctions(bool bRegister) override;
	// End UObject interface

	// UBulletNetworkPredictionComponent interface
	UE_API virtual void InitializeNetworkPredictionProxy() override;
	// End UBulletNetworkPredictionComponent interface


public:
	UE_API UBulletMoverNetworkPredictionLiaisonComponent();

protected:
	TObjectPtr<UBulletMoverComponent> MoverComp;	// the component that we're in charge of driving
	FBulletMoverSyncState* StartingOutSync;
	FBulletMoverAuxStateContext* StartingOutAux;
};

#undef UE_API
