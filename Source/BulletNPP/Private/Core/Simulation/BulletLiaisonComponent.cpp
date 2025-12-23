// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Simulation/BulletLiaisonComponent.h"

#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionModelDefRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletLiaisonComponent)


#define LOCTEXT_NAMESPACE "Bullet"

// ----------------------------------------------------------------------------------------------------------
//	FBulletActorModelDef: the piece that ties everything together that we use to register with the NP system.
// ----------------------------------------------------------------------------------------------------------

class FBulletActorModelDef : public FNetworkPredictionModelDef
{
public:

	NP_MODEL_BODY();

	using Simulation = UBulletLiaisonComponent;
	using StateTypes = BulletBufferTypes;
	using Driver = UBulletLiaisonComponent;

	static const TCHAR* GetName() { return TEXT("Bullet Actor"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::Physics; }
};

NP_MODEL_REGISTER(FBulletActorModelDef);


void UBulletLiaisonComponent::ProduceInput(const int32 DeltaTimeMS, FBulletInputCmdContext* Cmd)
{
}

void UBulletLiaisonComponent::RestoreFrame(const FBulletSyncState* SyncState, const FBulletAuxStateContext* AuxState)
{
}

void UBulletLiaisonComponent::FinalizeFrame(const FBulletSyncState* SyncState, const FBulletAuxStateContext* AuxState)
{
}

void UBulletLiaisonComponent::InitializeSimulationState(FBulletSyncState* OutSync, FBulletAuxStateContext* OutAux)
{
}

void UBulletLiaisonComponent::SimulationTick(const FNetSimTimeStep& TimeStep,
	const TNetSimInput<BulletBufferTypes>& SimInput, const TNetSimOutput<BulletBufferTypes>& SimOutput)
{
}

void UBulletLiaisonComponent::FinalizeSmoothingFrame(const FBulletSyncState* Sync,
	const FBulletAuxStateContext* AuxState)
{
}

// Sets default values for this component's properties
UBulletLiaisonComponent::UBulletLiaisonComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UBulletLiaisonComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


