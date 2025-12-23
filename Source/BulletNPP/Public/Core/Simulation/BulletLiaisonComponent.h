// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NetworkPredictionComponent.h"
#include "NetworkPredictionSimulation.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTickState.h"
#include "Core/DataTypes/BulletSimulationTypes.h"
#include "BulletLiaisonComponent.generated.h"

using BulletBufferTypes = TNetworkPredictionStateTypes<FBulletInputCmdContext, FBulletSyncState, FBulletAuxStateContext>;


UCLASS(BlueprintType, Blueprintable)
class BULLETNPP_API UBulletLiaisonComponent : public UNetworkPredictionComponent
{
	GENERATED_BODY()

public:
	
	// Begin NP Driver interface
	// Get latest local input prior to simulation step. Called by Network Prediction system on owner's instance (autonomous or authority).
	void ProduceInput(const int32 DeltaTimeMS, FBulletInputCmdContext* Cmd);

	// Restore a previous frame prior to resimulating. Called by Network Prediction system.
	void RestoreFrame(const FBulletSyncState* SyncState, const FBulletAuxStateContext* AuxState);

	// Take output for simulation. Called by Network Prediction system.
	void FinalizeFrame(const FBulletSyncState* SyncState, const FBulletAuxStateContext* AuxState);
	
	// Seed initial values based on component's state. Called by Network Prediction system.
	void InitializeSimulationState(FBulletSyncState* OutSync, FBulletAuxStateContext* OutAux);

	// Primary movement simulation update. Given an starting state and timestep, produce a new state. Called by Network Prediction system.
	void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<BulletBufferTypes>& SimInput, const TNetSimOutput<BulletBufferTypes>& SimOutput);
	// End NP Driver interface

	void FinalizeSmoothingFrame(const FBulletSyncState* Sync, const FBulletAuxStateContext* AuxState);
	
	
	// Sets default values for this component's properties
	UBulletLiaisonComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
};
