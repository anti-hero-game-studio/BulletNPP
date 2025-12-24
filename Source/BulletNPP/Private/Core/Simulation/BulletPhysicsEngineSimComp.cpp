// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Simulation/BulletPhysicsEngineSimComp.h"

#include "BulletLogChannels.h"
#include "Core/Simulation/BulletLiaisonComponent.h"


// Sets default values for this component's properties
UBulletPhysicsEngineSimComp::UBulletPhysicsEngineSimComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	
	BackendClass = UBulletLiaisonComponent::StaticClass();

	// ...
}

void UBulletPhysicsEngineSimComp::InitializeComponent()
{
	Super::InitializeComponent();
	
	
	// Instantiate our sister backend component that will actually talk to the system driving the simulation
	if (BackendClass)
	{
		UActorComponent* NewLiaisonComp = NewObject<UActorComponent>(this, BackendClass, TEXT("BackendLiaisonComponent"));
		BackendLiaisonComp.SetObject(NewLiaisonComp);
		BackendLiaisonComp.SetInterface(CastChecked<IBulletBackendLiaisonInterface>(NewLiaisonComp));
		if (BackendLiaisonComp)
		{
			NewLiaisonComp->RegisterComponent();
			NewLiaisonComp->InitializeComponent();
			NewLiaisonComp->SetNetAddressable();
		}
	}
	else
	{
		UE_LOG(LogBullet, Error, TEXT("No backend class set on %s. Bullet actor will not function."), *GetNameSafe(GetOwner()));
	}
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

void UBulletPhysicsEngineSimComp::SimulationTick(const FBulletTimeStep& InTimeStep, const FBulletTickStartData& SimInput, FBulletTickEndData& SimOutput)
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

void UBulletPhysicsEngineSimComp::InitializeSimulation()
{
	// TODO:@GreggoryAddison::Init | Function's role is to create the state machine that manages the movement modes. This is how it's done in ActionScript don't know if its needed here.
}

