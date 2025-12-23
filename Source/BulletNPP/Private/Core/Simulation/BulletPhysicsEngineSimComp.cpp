// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Simulation/BulletPhysicsEngineSimComp.h"


// Sets default values for this component's properties
UBulletPhysicsEngineSimComp::UBulletPhysicsEngineSimComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UBulletPhysicsEngineSimComp::InitializeComponent()
{
	Super::InitializeComponent();
}

void UBulletPhysicsEngineSimComp::UninitializeComponent()
{
	Super::UninitializeComponent();
}

void UBulletPhysicsEngineSimComp::OnRegister()
{
	Super::OnRegister();
}

void UBulletPhysicsEngineSimComp::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);
}

void UBulletPhysicsEngineSimComp::PostLoad()
{
	Super::PostLoad();
}

void UBulletPhysicsEngineSimComp::ProduceInput(const int32 DeltaTimeMS, FBulletInputCmdContext* Cmd)
{
}

void UBulletPhysicsEngineSimComp::RestoreFrame(const FBulletSyncState* SyncState,
	const FBulletAuxStateContext* AuxState, const FBulletTimeStep& NewBaseTimeStep)
{
}

void UBulletPhysicsEngineSimComp::FinalizeFrame(const FBulletSyncState* SyncState,
	const FBulletAuxStateContext* AuxState)
{
}

void UBulletPhysicsEngineSimComp::FinalizeUnchangedFrame()
{
}

void UBulletPhysicsEngineSimComp::FinalizeSmoothingFrame(const FBulletSyncState* SyncState,
	const FBulletAuxStateContext* AuxState)
{
}

void UBulletPhysicsEngineSimComp::TickInterpolatedSimProxy(const FBulletTimeStep& TimeStep,
	const FBulletInputCmdContext& InputCmd, UBulletPhysicsEngineSimComp* BulletComp,
	const FBulletSyncState& CachedSyncState, const FBulletSyncState& SyncState, const FBulletAuxStateContext& AuxState)
{
}

void UBulletPhysicsEngineSimComp::InitializeSimulationState(FBulletSyncState* OutSync, FBulletAuxStateContext* OutAux)
{
}

void UBulletPhysicsEngineSimComp::SimulationTick(const FBulletTimeStep& InTimeStep,
	const FBulletTickStartData& SimInput, FBulletTickEndData& SimOutput)
{
}

USceneComponent* UBulletPhysicsEngineSimComp::GetUpdatedComponent() const
{
	return nullptr;
}


// Called when the game starts
void UBulletPhysicsEngineSimComp::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

