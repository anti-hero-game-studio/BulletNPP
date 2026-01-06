// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletNetworkPredictionComponent.h"
#include "Engine/World.h"
#include "BulletNetworkPredictionWorldManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletNetworkPredictionComponent)

UBulletNetworkPredictionComponent::UBulletNetworkPredictionComponent()
{
	SetIsReplicatedByDefault(true);
	
}

void UBulletNetworkPredictionComponent::InitializeComponent()
{
	Super::InitializeComponent();

	UWorld* World = GetWorld();	
	UBulletNetworkPredictionWorldManager* NetworkPredictionWorldManager = GetWorld()->GetSubsystem<UBulletNetworkPredictionWorldManager>();
	if (NetworkPredictionWorldManager)
	{
		// Init RepProxies
		ReplicationProxy_ServerRPC.Init(&NetworkPredictionProxy, EBulletReplicationProxyTarget::ServerRPC);
		ReplicationProxy_Autonomous.Init(&NetworkPredictionProxy, EBulletReplicationProxyTarget::AutonomousProxy);
		ReplicationProxy_Simulated.Init(&NetworkPredictionProxy, EBulletReplicationProxyTarget::SimulatedProxy);
		ReplicationProxy_Replay.Init(&NetworkPredictionProxy, EBulletReplicationProxyTarget::Replay);

		InitializeNetworkPredictionProxy();

		CheckOwnerRoleChange();
	}
}

void UBulletNetworkPredictionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	NetworkPredictionProxy.EndPlay();
}

void UBulletNetworkPredictionComponent::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
	
	CheckOwnerRoleChange();

	// We have to update our replication proxies so they can be accurately compared against client shadowstate during property replication. ServerRPC proxy does not need to do this.
	ReplicationProxy_Autonomous.OnPreReplication();
	ReplicationProxy_Simulated.OnPreReplication();
	ReplicationProxy_Replay.OnPreReplication();
}

void UBulletNetworkPredictionComponent::PreNetReceive()
{
	Super::PreNetReceive();
	CheckOwnerRoleChange();
}

void UBulletNetworkPredictionComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME( UBulletNetworkPredictionComponent, NetworkPredictionProxy);
	DOREPLIFETIME_CONDITION( UBulletNetworkPredictionComponent, ReplicationProxy_Autonomous, COND_AutonomousOnly);
	DOREPLIFETIME_CONDITION( UBulletNetworkPredictionComponent, ReplicationProxy_Simulated, COND_SimulatedOnlyNoReplay);
	DOREPLIFETIME_CONDITION( UBulletNetworkPredictionComponent, ReplicationProxy_Replay, COND_ReplayOnly);
}

void UBulletNetworkPredictionComponent::InitializeForNetworkRole(ENetRole Role, const bool bHasNetConnection,UBulletNetworkPredictionPlayerControllerComponent* RPCHandler)
{
	NetworkPredictionProxy.InitForNetworkRole(Role, bHasNetConnection,RPCHandler);
}

bool UBulletNetworkPredictionComponent::CheckOwnerRoleChange()
{
	AActor* OwnerActor = GetOwner();
	const ENetRole CurrentRole = OwnerActor->GetLocalRole();
	const bool bHasNetConnection = OwnerActor->GetNetConnection() != nullptr;
	UBulletNetworkPredictionPlayerControllerComponent* RPCHandler = NetworkPredictionProxy.GetCachedRPCHandler();
	if (CurrentRole != ROLE_SimulatedProxy && bHasNetConnection && !IsValid(NetworkPredictionProxy.GetCachedRPCHandler()))
	{
		if (GetOwner()->GetNetConnection()->OwningActor)
		{
			RPCHandler = GetOwner()->GetNetConnection()->OwningActor->GetComponentByClass<UBulletNetworkPredictionPlayerControllerComponent>();
			if (!RPCHandler)
			{
				// Create and register a new component dynamically
				RPCHandler = NewObject<UBulletNetworkPredictionPlayerControllerComponent>(GetOwner()->GetNetConnection()->OwningActor);

				if (RPCHandler)
				{
					RPCHandler->SetNetAddressable();
					RPCHandler->SetIsReplicated(true);
					RPCHandler->RegisterComponent();
					RPCHandler->InitializeComponent();
					RPCHandler->Activate(true);
				}
			}
		}
	}
	
	if (CurrentRole != NetworkPredictionProxy.GetCachedNetRole() || bHasNetConnection != NetworkPredictionProxy.GetCachedHasNetConnection()
		|| RPCHandler != NetworkPredictionProxy.GetCachedRPCHandler())
	{
		InitializeForNetworkRole(CurrentRole, bHasNetConnection,RPCHandler);
		return true;
	}

	return false;
}

bool UBulletNetworkPredictionComponent::ServerReceiveClientInput_Validate(const FBulletServerReplicationRPCParameter& ProxyParameter)
{
	return true;
}

void UBulletNetworkPredictionComponent::ServerReceiveClientInput_Implementation(const FBulletServerReplicationRPCParameter& ProxyParameter)
{
	// The const_cast is unavoidable here because the replication system only allows by value (forces copy, bad) or by const reference. This use case is unique because we are using the RPC parameter as a temp buffer.
	const_cast<FBulletServerReplicationRPCParameter&>(ProxyParameter).NetSerializeToProxy(ReplicationProxy_ServerRPC);
}

void UBulletNetworkPredictionComponent::CallServerRPC()
{
	// Temp hack to make sure the ServerRPC doesn't get suppressed from bandwidth limiting
	// (system hasn't been optimized and not mature enough yet to handle gaps in input stream)
	FBulletScopedBandwidthLimitBypass BandwidthBypass(GetOwner());

	FBulletServerReplicationRPCParameter ProxyParameter(ReplicationProxy_ServerRPC);
	ServerReceiveClientInput(ProxyParameter);
}

// --------------------------------------------------------------


