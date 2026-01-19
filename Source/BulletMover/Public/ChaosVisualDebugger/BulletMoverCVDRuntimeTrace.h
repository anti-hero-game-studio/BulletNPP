// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

#include "ChaosVisualDebugger/ChaosVDOptionalDataChannel.h"

struct FBulletMoverCVDSimDataWrapper;
struct FBulletMoverInputCmdContext;
struct FBulletMoverSyncState;
struct FBulletMoverDataCollection;
class UBulletMoverComponent;


namespace UE::BulletMoverUtils
{

typedef TArray<TPair<FName, const FBulletMoverDataCollection*>> NamedDataCollections;

#if WITH_CHAOS_VISUAL_DEBUGGER

/** Utility functions used to trace BulletMover data into the Chaos Visual Debugger */
class FBulletMoverCVDRuntimeTrace
{
public:
	BULLETMOVER_API static void TraceBulletMoverData(UBulletMoverComponent* BulletMoverComponent, const FBulletMoverInputCmdContext* InputCmd, const FBulletMoverSyncState* SyncState, const NamedDataCollections* LocalSimDataCollections = nullptr);
	BULLETMOVER_API static void TraceBulletMoverData(uint32 SolverID, uint32 ParticleID, const FBulletMoverInputCmdContext* InputCmd, const FBulletMoverSyncState* SyncState, const NamedDataCollections* LocalSimDataCollections = nullptr);

	BULLETMOVER_API static void UnwrapSimData(const FBulletMoverCVDSimDataWrapper& InSimDataWrapper, TSharedPtr<FBulletMoverInputCmdContext>& OutInputCmd, TSharedPtr<FBulletMoverSyncState>& OutSyncState, TSharedPtr<FBulletMoverDataCollection>& OutLocalSimState);
	BULLETMOVER_API static void WrapSimData(uint32 SolverID, uint32 ParticleID, const FBulletMoverInputCmdContext& InInputCmd, const FBulletMoverSyncState& InSyncState, const FBulletMoverDataCollection* LocalSimState, FBulletMoverCVDSimDataWrapper& OutSimDataWrapper);

private:
	static void TraceBulletMoverDataPrivate(uint32 SolverID, uint32 ParticleID, const FBulletMoverInputCmdContext* InputCmd, const FBulletMoverSyncState* SyncState, const FBulletMoverDataCollection* LocalSimState = nullptr);
};

// This is all bulletMover data that is networked, either input command (client to server) or sync state (server to client)
CVD_DECLARE_OPTIONAL_DATA_CHANNEL_EXTERN(BulletMoverNetworkedData, BULLETMOVER_API)
// This is additional bulletMover data, local to each end point's simulation
CVD_DECLARE_OPTIONAL_DATA_CHANNEL_EXTERN(BulletMoverLocalSimData, BULLETMOVER_API)

#else

// Noop implementation in case this is compiled without Chaos Visual Debugger support (e.g. shipping)
class FBulletMoverCVDRuntimeTrace
{
public:
	static void TraceBulletMoverData(UBulletMoverComponent* BulletMoverComponent, const FBulletMoverInputCmdContext* InputCmd, const FBulletMoverSyncState* SyncState, const NamedDataCollections* FBulletMoverLocalSimState = nullptr) {}
	static void TraceBulletMoverData(uint32 SolverID, uint32 ParticleID, const FBulletMoverInputCmdContext* InputCmd, const FBulletMoverSyncState* SyncState, const NamedDataCollections* FBulletMoverLocalSimState = nullptr) {}
	static void UnwrapSimData(const FBulletMoverCVDSimDataWrapper& InSimDataWrapper, TSharedPtr<FBulletMoverInputCmdContext>& OutInputCmd, TSharedPtr<FBulletMoverSyncState>& OutSyncState, TSharedPtr<FBulletMoverDataCollection>& FBulletMoverLocalSimState) {}
	static void WrapSimData(uint32 SolverID, uint32 ParticleID, const FBulletMoverInputCmdContext& InInputCmd, const FBulletMoverSyncState& InSyncState, const FBulletMoverDataCollection* FBulletMoverLocalSimState, FBulletMoverCVDSimDataWrapper& OutSimDataWrapper) {}
};

#endif // WITH_CHAOS_VISUAL_DEBUGGER

} // namespace UE::BulletMoverUtils