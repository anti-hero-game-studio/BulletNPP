// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BulletNetworkPredictionCheck.h"

#include "BulletNetworkPredictionReplicationProxy.generated.h"

struct FBulletNetworkPredictionProxy;
class UPackageMap;

// Target of replication
enum class EBulletReplicationProxyTarget: uint8
{
	ServerRPC,			// Client -> Server
	AutonomousProxy,	// Owning/Controlling client
	SimulatedProxy,		// Non owning client
	Replay,				// Replay net driver
};

inline FString LexToString(EBulletReplicationProxyTarget A)
{
	return *UEnum::GetValueAsString(TEXT("BulletNetworkPrediction.EBulletReplicationProxyTarget"), A);
}

// The parameters for NetSerialize that are passed around the system. Everything should use this, expecting to have to add more.
struct FBulletNetSerializeParams
{
	FBulletNetSerializeParams(FArchive& InAr) : Ar(InAr),Map(nullptr) { }
	FBulletNetSerializeParams(FArchive& InAr,UPackageMap* InMap) : Ar(InAr),Map(InMap) { }
	FBulletNetSerializeParams(FArchive& InAr,UPackageMap* InMap, const EBulletReplicationProxyTarget& InReplicationTarget) : Ar(InAr),Map(InMap),ReplicationTarget(InReplicationTarget) { }
	FArchive& Ar;
	UPackageMap* Map;
	EBulletReplicationProxyTarget ReplicationTarget = EBulletReplicationProxyTarget::ServerRPC;
	template<typename T>
	const T* GetBaseDeltaState() const
	{
		return static_cast<const T*>(BaseDeltaStatePtr);
	}
	
	const void* BaseDeltaStatePtr = nullptr;
};

// Redirects NetSerialize to a dynamically set NetSerializeFunc.
// This is how we hook into the replication systems role-based serialization
USTRUCT()
struct BULLETNETWORKPREDICTION_API FBulletReplicationProxy
{
	GENERATED_BODY()

	void Init(FBulletNetworkPredictionProxy* InBulletNetSimProxy, EBulletReplicationProxyTarget InReplicationTarget);
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
	void OnPreReplication();	
	bool Identical(const FBulletReplicationProxy* Other, uint32 PortFlags) const;

	TFunction<void(const FBulletNetSerializeParams& P)> NetSerializeFunc;
	FBulletNetworkPredictionProxy* BulletNetSimProxy = nullptr;

private:

	EBulletReplicationProxyTarget ReplicationTarget;
	int32 CachedPendingFrame = INDEX_NONE;
};

template<>
struct TStructOpsTypeTraits<FBulletReplicationProxy> : public TStructOpsTypeTraitsBase2<FBulletReplicationProxy>
{
	enum
	{
		WithNetSerializer = true,
		WithIdentical = true,
	};
};

// Collection of each replication proxy
struct FBulletReplicationProxySet
{
	FBulletReplicationProxy* ServerRPC = nullptr;
	FBulletReplicationProxy* AutonomousProxy = nullptr;
	FBulletReplicationProxy* SimulatedProxy = nullptr;
	FBulletReplicationProxy* Replay = nullptr;

	void UnbindAll() const
	{
		jnpCheckSlow(ServerRPC && AutonomousProxy && SimulatedProxy && Replay);
		ServerRPC->NetSerializeFunc = nullptr;
		AutonomousProxy->NetSerializeFunc = nullptr;
		SimulatedProxy->NetSerializeFunc = nullptr;
		Replay->NetSerializeFunc = nullptr;
	}
};

// -------------------------------------------------------------------------------------------------------------------------------
//	FServerRPCProxyParameter
//	Used for the client->Server RPC. Since this is instantiated on the stack by the replication system prior to net serializing,
//	we have no opportunity to point the RPC parameter to the member variables we want. So we serialize into a generic temp byte buffer
//	and move them into the real buffers on the component in the RPC body (via ::NetSerialzeToProxy).
// -------------------------------------------------------------------------------------------------------------------------------
USTRUCT()
struct BULLETNETWORKPREDICTION_API FBulletServerReplicationRPCParameter
{
	GENERATED_BODY()

	// Receive flow: ctor() -> NetSerializetoProxy
	FBulletServerReplicationRPCParameter() : Proxy(nullptr)	{ }
	void NetSerializeToProxy(FBulletReplicationProxy& InProxy);

	// Send flow: ctor(Proxy) -> NetSerialize
	FBulletServerReplicationRPCParameter(FBulletReplicationProxy& InProxy) : Proxy(&InProxy) { }
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

private:

	static TArray<uint8> TempStorage;

	FBulletReplicationProxy* Proxy;
	int64 CachedNumBits = -1;
	class UPackageMap* CachedPackageMap = nullptr;
};

template<>
struct TStructOpsTypeTraits<FBulletServerReplicationRPCParameter> : public TStructOpsTypeTraitsBase2<FBulletServerReplicationRPCParameter>
{
	enum
	{
		WithNetSerializer = true
	};
};

// Helper struct to bypass the bandwidth limit imposed by the engine's NetDriver (QueuedBits, NetSpeed, etc).
// This is really a temp measure to make the system easier to drop in/try in a project without messing with your engine settings.
// (bandwidth optimizations have not been done yet and the system in general hasn't been stressed with packetloss / gaps in command streams)
// So, you are free to use this in your own code but it may be removed one day. Hopefully in general bandwidth limiting will also become more robust.
struct BULLETNETWORKPREDICTION_API FBulletScopedBandwidthLimitBypass
{
	FBulletScopedBandwidthLimitBypass(AActor* OwnerActor);
	~FBulletScopedBandwidthLimitBypass();
private:

	int64 RestoreBits = 0;
	class UNetConnection* CachedNetConnection = nullptr;
};


USTRUCT()
struct FBulletSimulationReplicatedInput
{
	GENERATED_BODY()

	FBulletSimulationReplicatedInput(){}
	FBulletSimulationReplicatedInput(const int32& InID,const uint32& InDataSize, const TArray<uint8>& InData)
	{
		ID = InID;
		DataSize = InDataSize;
		InputData = InData;
	};
	
	UPROPERTY()
	uint32 ID = 0;

	UPROPERTY()
	uint32 DataSize = 0;

	UPROPERTY()
	TArray<uint8> InputData;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& OutSuccess)
	{
		Ar.SerializeIntPacked(ID);
		Ar.SerializeIntPacked(DataSize);
		
		uint32 Num = InputData.Num();
		Ar.SerializeIntPacked(Num);
		if (Ar.IsLoading())
		{
			InputData.SetNum(Num);
		}
		for (uint32 i = 0; i < Num; i++)
		{
			Ar << InputData[i];
		}
		
		OutSuccess = true;
		return true;
	}
};
template<>
struct TStructOpsTypeTraits<FBulletSimulationReplicatedInput> : public TStructOpsTypeTraitsBase2<FBulletSimulationReplicatedInput>
{
	enum
	{
		WithNetSerializer = true,
	};
};

USTRUCT()
struct FBulletSimTimeDilation
{
	GENERATED_BODY()

	FBulletSimTimeDilation(){}
	FBulletSimTimeDilation(const float& InTimeDilation)
	{
		UpdateTimeDilation(InTimeDilation);
	};
	
	float GetTimeDilation() const {return TimeDilation / 10000.f;}
	void UpdateTimeDilation(const float& InTimeDilation)
	{
		TimeDilation = FMath::Clamp(FMath::RoundToInt32(InTimeDilation * 10000.f), 0, UINT16_MAX);
	}

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& OutSuccess)
	{
		Ar << TimeDilation;
		OutSuccess = true;
		return true;
	}

private:
	UPROPERTY()
	uint16 TimeDilation = 10000;
};
template<>
struct TStructOpsTypeTraits<FBulletSimTimeDilation> : public TStructOpsTypeTraitsBase2<FBulletSimTimeDilation>
{
	enum
	{
		WithNetSerializer = true,
	};
};

