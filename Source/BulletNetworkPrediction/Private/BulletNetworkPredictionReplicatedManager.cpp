// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletNetworkPredictionReplicatedManager.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Engine/World.h"
#include "BulletNetworkPredictionWorldManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletNetworkPredictionReplicatedManager)


ABulletNetworkPredictionReplicatedManager::FOnAuthoritySpawn ABulletNetworkPredictionReplicatedManager::OnAuthoritySpawnDelegate;
TWeakObjectPtr<ABulletNetworkPredictionReplicatedManager> ABulletNetworkPredictionReplicatedManager::AuthorityInstance;

ABulletNetworkPredictionReplicatedManager::ABulletNetworkPredictionReplicatedManager()
{
	bReplicates = true;
	NetPriority = 1000.f; // We want this to be super high priority when it replicates
	// Mute very low update frequency ensure. Was 0.001f .
	SetNetUpdateFrequency(0.125f); // Low frequency: we will use ForceNetUpdate when important data changes
	bAlwaysRelevant = true;
}

void ABulletNetworkPredictionReplicatedManager::BeginPlay()
{
	Super::BeginPlay();
	if (GetLocalRole() == ROLE_Authority)
	{
		OnAuthoritySpawnDelegate.Broadcast(this);
	}
	else
	{
		UBulletNetworkPredictionWorldManager* NetworkPredictionWorldManager = GetWorld()->GetSubsystem<UBulletNetworkPredictionWorldManager>();
		jnpCheckSlow(NetworkPredictionWorldManager);
		NetworkPredictionWorldManager->ReplicatedManager = this;
	}
}

void ABulletNetworkPredictionReplicatedManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ABulletNetworkPredictionReplicatedManager, SharedPackageMap, SharedParams);
}

FDelegateHandle ABulletNetworkPredictionReplicatedManager::OnAuthoritySpawn(const TFunction<void(ABulletNetworkPredictionReplicatedManager*)>& Func)
{
	if (AuthorityInstance.IsValid())
	{
		Func(AuthorityInstance.Get());
	}

	// I don't think there is a way to move a TUniqueFunction onto a delegate, so TFunction will have to do
	return OnAuthoritySpawnDelegate.AddLambda(Func);
}

uint8 ABulletNetworkPredictionReplicatedManager::GetIDForObject(UObject* Obj) const
{
	// Naive lookup
	for (auto It = SharedPackageMap.Items.CreateConstIterator(); It; ++It)
	{
		const FBulletSharedPackageMapItem& Item = *It;
		if (Item.SoftPtr.Get() == Obj)
		{
			jnpCheckSlow(It.GetIndex() < TNumericLimits<uint8>::Max());
			return (uint8)It.GetIndex();
		}
	}

	jnpEnsureMsgf(false, TEXT("Could not find Object %s in SharedPackageMap."), *GetNameSafe(Obj));
	return 0;
}

TSoftObjectPtr<UObject> ABulletNetworkPredictionReplicatedManager::GetObjectForID(uint8 ID) const
{
	if (SharedPackageMap.Items.IsValidIndex(ID))
	{
		return SharedPackageMap.Items[ID].SoftPtr;
	}

	return TSoftObjectPtr<UObject>();
}

uint8 ABulletNetworkPredictionReplicatedManager::AddObjectToSharedPackageMap(TSoftObjectPtr<UObject> SoftPtr)
{
	if (SharedPackageMap.Items.Num()+1 >= TNumericLimits<uint8>::Max())
	{
		UE_LOG(LogTemp, Warning, TEXT("Mock SharedPackageMap has overflowed!"));
		for (FBulletSharedPackageMapItem& Item : SharedPackageMap.Items)
		{
			UE_LOG(LogTemp, Warning, TEXT("   %s"), *Item.SoftPtr.ToString());
		}
		ensureMsgf(false, TEXT("SharedPackageMap overflowed"));
		return 0;
	}

	SharedPackageMap.Items.Add(FBulletSharedPackageMapItem{SoftPtr});
	MARK_PROPERTY_DIRTY_FROM_NAME(ABulletNetworkPredictionReplicatedManager, SharedPackageMap, this);

	return SharedPackageMap.Items.Num()-1;
}
