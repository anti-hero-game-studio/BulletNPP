// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "NetworkPredictionComponent.h"
#include "NetworkPredictionSimulation.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionTickState.h"
#include "Core/DataTypes/BulletSimulationTypes.h"
#include "Core/Interfaces/BulletBackendLiaisonInterface.h"
#include "BulletLiaisonComponent.generated.h"

using BulletBufferTypes = TNetworkPredictionStateTypes<FBulletInputCmdContext, FBulletSyncState, FBulletAuxStateContext>;


UCLASS(BlueprintType, Blueprintable)
class BULLETNPP_API UBulletLiaisonComponent : public UNetworkPredictionComponent, public IBulletBackendLiaisonInterface
{
	GENERATED_BODY()

public:
	
	// Sets default values for this component's properties
	UBulletLiaisonComponent();
	
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
	
	
	
	// UObject interface
	virtual void InitializeComponent() override;
	virtual void OnRegister() override;
	virtual void RegisterComponentTickFunctions(bool bRegister) override;
	// End UObject interface

	// UNetworkPredictionComponent interface
	virtual void InitializeNetworkPredictionProxy() override;
	// End UNetworkPredictionComponent interface
	
	
	//virtual float GetCurrentSimTimeMs() const;
	//virtual int32 GetCurrentSimFrame() const;
	//virtual float GetSyncedInterpolationTime() const;
	virtual ENetRole GetCachedSimNetRole() const;



protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	
#pragma region SETTINGS
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Settings)
	uint8 bDoesSimulationProcessLocalInput : 1 = 0;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Settings)
	uint8 bDoSimProxiesForwardPredictThisSimulation : 1 = 1;
	
#pragma endregion SETTINGS
	

	UPROPERTY()
	TObjectPtr<UBulletPhysicsEngineSimComp> SimulationComponent;	// the component that we're in charge of driving
	
	
	virtual ENetworkPredictionLocalInputPolicy GetLocalInputPolicy() const; 
	
	float ElapsedTime = 0.f;
	bool bIsFirstTick = true;
	
	
	
};
