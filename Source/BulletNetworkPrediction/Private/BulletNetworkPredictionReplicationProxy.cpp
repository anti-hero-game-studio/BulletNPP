// Copyright Epic Games, Inc. All Rights Reserved

#include "BulletNetworkPredictionReplicationProxy.h"
#include "BulletNetworkPredictionProxy.h"
#include "Engine/NetConnection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletNetworkPredictionReplicationProxy)

// -------------------------------------------------------------------------------------------------------------------------------
//	FBulletReplicationProxy
// -------------------------------------------------------------------------------------------------------------------------------

void FBulletReplicationProxy::Init(FBulletNetworkPredictionProxy* InBulletNetSimProxy, EBulletReplicationProxyTarget InReplicationTarget)
{
	BulletNetSimProxy = InBulletNetSimProxy;
	ReplicationTarget = InReplicationTarget;
}

bool FBulletReplicationProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	if (jnpEnsureMsgf(NetSerializeFunc, TEXT("NetSerializeFunc not set for FBulletReplicationProxy %d"), ReplicationTarget))
	{
		NetSerializeFunc(FBulletNetSerializeParams(Ar,Map,ReplicationTarget));
		return true;
	}
	return true;
}

void FBulletReplicationProxy::OnPreReplication()
{
	if (BulletNetSimProxy)
	{
		CachedPendingFrame = BulletNetSimProxy->GetPendingFrame();
	}
}

bool FBulletReplicationProxy::Identical(const FBulletReplicationProxy* Other, uint32 PortFlags) const
{
	return (CachedPendingFrame == Other->CachedPendingFrame);
}

// -------------------------------------------------------------------------------------------------------------------------------
//	FServerRPCProxyParameter
// -------------------------------------------------------------------------------------------------------------------------------

TArray<uint8> FBulletServerReplicationRPCParameter::TempStorage;

bool FBulletServerReplicationRPCParameter::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	if (Ar.IsLoading())
	{
		// Loading: serialize to temp storage. We'll do the real deserialize in a manual call to ::NetSerializeToProxy
		FNetBitReader& BitReader = (FNetBitReader&)Ar;
		CachedNumBits = BitReader.GetBitsLeft();
		CachedPackageMap = Map;

		const int64 BytesLeft = BitReader.GetBytesLeft();
		check(BytesLeft > 0); // Should not possibly be able to get here with an empty archive
		TempStorage.Reset(BytesLeft);
		TempStorage.SetNumUninitialized(BytesLeft);
		TempStorage.Last() = 0;

		BitReader.SerializeBits(TempStorage.GetData(), CachedNumBits);
	}
	else
	{
		// Saving: directly call into the proxy's NetSerialize. No need for temp storage.
		check(Proxy); // Must have been set before, via ctor.
		return Proxy->NetSerialize(Ar, Map, bOutSuccess);
	}

	return true;
}

void FBulletServerReplicationRPCParameter::NetSerializeToProxy(FBulletReplicationProxy& InProxy)
{
	check(CachedPackageMap != nullptr);
	check(CachedNumBits != -1);

	FNetBitReader BitReader(CachedPackageMap, TempStorage.GetData(), CachedNumBits);

	bool bOutSuccess = true;
	InProxy.NetSerialize(BitReader, CachedPackageMap, bOutSuccess);

	CachedNumBits = -1;
	CachedPackageMap = nullptr;
}

// -------------------------------------------------------------------------------------------------------------------------------
//	FBulletScopedBandwidthLimitBypass
// -------------------------------------------------------------------------------------------------------------------------------

FBulletScopedBandwidthLimitBypass::FBulletScopedBandwidthLimitBypass(AActor* OwnerActor)
{
	if (OwnerActor)
	{
		CachedNetConnection = OwnerActor->GetNetConnection();
		if (CachedNetConnection)
		{
			RestoreBits = CachedNetConnection->QueuedBits + CachedNetConnection->SendBuffer.GetNumBits();
		}
	}
}

FBulletScopedBandwidthLimitBypass::~FBulletScopedBandwidthLimitBypass()
{
	if (CachedNetConnection)
	{
		CachedNetConnection->QueuedBits = RestoreBits - CachedNetConnection->SendBuffer.GetNumBits();
	}
}

