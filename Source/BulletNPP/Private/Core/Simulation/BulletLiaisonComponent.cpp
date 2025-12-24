// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Simulation/BulletLiaisonComponent.h"
#include "NetworkPredictionProxyWrite.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxy.h"
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionWorldManager.h"
#include "Core/Simulation/BulletPhysicsEngineSimComp.h"
#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"

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


// Sets default values for this component's properties
UBulletLiaisonComponent::UBulletLiaisonComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.TickGroup = TG_DuringPhysics; //TODO:@GreggoryAddison::Optimization | This might need to tick before UE physics but hoping that it can run along with the physics tick & potentially save some cpu cycles.
	PrimaryComponentTick.bCanEverTick = true;

	bWantsInitializeComponent = true;
	bAutoActivate = true;
	SetIsReplicatedByDefault(true);

	// ...
}


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
	//TODO:@GreggoryAddison::Init | Register my dynamic rigid body with the Bullet Physics World

	if (!SimulationComponent) return;
	if (UBulletPhysicsWorldSubsystem* B = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>())
	{
		int32 DummyId;
		B->RegisterDynamicRigidBody(SimulationComponent->GetOwner(), 0.5, 0, 10.f, false, DummyId);
	}
}

void UBulletLiaisonComponent::SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<BulletBufferTypes>& SimInput, const TNetSimOutput<BulletBufferTypes>& SimOutput)
{
	if (!SimulationComponent) return;
	
	const float TriggerTime = 30.f;
	int32 Seed = 200;
	if (!bIsFirstTick)
	{
		if (ElapsedTime >= 7.f)
		{
			ElapsedTime = 0;
			Seed = TimeStep.Frame;
		}
		else
		{
			ElapsedTime += (TimeStep.StepMS * 0.001f);
		}
	}
	
	bIsFirstTick = false;
	
	FRandomStream RandomStream(Seed);
	// Get a location in a box around us. 
	const FVector Center = SimulationComponent.Get()->GetOwner()->GetActorLocation();
	const FVector HalfSize = FVector(3000.f, 3000.f, 100.f);
	
	const FVector BoxMin = Center - HalfSize;
	const FVector BoxMax = Center + HalfSize;
	const FVector TargetLocation = RandomStream.RandPointInBox(FBox(BoxMin, BoxMax));

	if (FMath::IsNearlyZero(ElapsedTime) || bIsFirstTick)
	{
		DrawDebugBox(GetWorld(), Center, HalfSize, FColor::Yellow, false, 7.f);
		DrawDebugSphere(GetWorld(), TargetLocation, 24.f, 8, FColor::Green, false, 7.f, 1, 1);
		DrawDebugDirectionalArrow(GetWorld(), Center, TargetLocation, 5.f, FColor::Cyan, false, 7.f, 1, 4);
	}
	
	const FVector Direction = (TargetLocation - Center).GetSafeNormal();
	
	// Add linear force in the "Direction" 
	if (UBulletPhysicsWorldSubsystem* B = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>())
	{
		B->AddForce(SimulationComponent->GetOwner(), Direction * 500.f, Center);
		
	// call StepPhysics in our bullet world
		B->StepPhysics(TimeStep.StepMS * 0.001f);
	}
	
	//TODO:@GreggoryAddison::TEST | Add simple forces to the owner's dynamic rb based on the input or even simpler a deterministic randomized vector force just to see what happens
}

void UBulletLiaisonComponent::FinalizeSmoothingFrame(const FBulletSyncState* Sync, const FBulletAuxStateContext* AuxState)
{
}

void UBulletLiaisonComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UBulletLiaisonComponent::OnRegister()
{
	Super::OnRegister();
}

void UBulletLiaisonComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);
}

void UBulletLiaisonComponent::InitializeNetworkPredictionProxy()
{
	SimulationComponent = GetOwner()->FindComponentByClass<UBulletPhysicsEngineSimComp>();
	
	if (ensureAlwaysMsgf(SimulationComponent, TEXT("UActionScriptNetworkHandlerComponent on actor %s failed to find associated ASC component. This actor will not be simulated. Verify its setup."), *GetNameSafe(GetOwner())))
	{
		SimulationComponent->InitializeSimulation();

		NetworkPredictionProxy.Init<FBulletActorModelDef>(GetWorld(), GetReplicationProxies(), this, this);
		FNetworkPredictionInstanceConfig Config;
		Config.InputPolicy = GetLocalInputPolicy();
		Config.NetworkLOD = ENetworkLOD::ForwardPredict;
		NetworkPredictionProxy.Configure(Config);
	}
}

ENetRole UBulletLiaisonComponent::GetCachedSimNetRole() const
{
	switch (NetworkPredictionProxy.GetCachedNetRole())
	{
	case ROLE_None:
		break;
	case ROLE_SimulatedProxy:
		if (UNetworkPredictionWorldManager* M = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>())
		{
			return M->GetSettings().SimulatedProxyNetworkLOD == ENetworkLOD::Interpolated  && bDoSimProxiesForwardPredictThisSimulation ? ROLE_AutonomousProxy : ROLE_SimulatedProxy; 
		}
		return ROLE_SimulatedProxy;
	case ROLE_AutonomousProxy:
		return ROLE_AutonomousProxy;
	case ROLE_Authority:
		return ROLE_Authority;
	case ROLE_MAX:
		break;
	}

	return ROLE_None;
}




// Called when the game starts
void UBulletLiaisonComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

ENetworkPredictionLocalInputPolicy UBulletLiaisonComponent::GetLocalInputPolicy() const
{
	switch (GetOwner()->GetLocalRole())
	{
		case ROLE_Authority:
			return NetworkPredictionProxy.GetCachedHasNetConnection() ? ENetworkPredictionLocalInputPolicy::Passive : ENetworkPredictionLocalInputPolicy::PollPerSimFrame;
		case ROLE_AutonomousProxy:
			return ENetworkPredictionLocalInputPolicy::PollPerSimFrame;
		case ROLE_SimulatedProxy:
			return ENetworkPredictionLocalInputPolicy::Passive;
	}
	
	return ENetworkPredictionLocalInputPolicy::Passive;
}


