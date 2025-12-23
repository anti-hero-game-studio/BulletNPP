// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DataTypes/BulletPhysicsTypes.h"
#include "Core/DataTypes/BulletSimulationTypes.h"
#include "BulletPhysicsEngineSimComp.generated.h"

namespace BulletComponentConstants
{
	extern const FVector DefaultGravityAccel;		// Fallback gravity if not determined by the component or world (cm/s^2)
	extern const FVector DefaultUpDir;				// Fallback up direction if not determined by the component or world (normalized)
}

// Fired just before a simulation tick, regardless of being a re-simulated frame or not.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBullet_OnPreSimTick, const FBulletTimeStep&, TimeStep, const FBulletInputCmdContext&, InputCmd);

// Fired during a simulation tick, after the input is processed but before the actual move calculation.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBullet_OnPreMovement, const FBulletTimeStep&, TimeStep, const FBulletInputCmdContext&, InputCmd, const FBulletSyncState&, SyncState, const FBulletAuxStateContext&, AuxState);

// Fired during a simulation tick, after movement has occurred but before the state is finalized, allowing changes to the output state.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBullet_OnPostMovement, const FBulletTimeStep&, TimeStep, FBulletSyncState&, SyncState, FBulletAuxStateContext&, AuxState);

// Fired after a simulation tick, regardless of being a re-simulated frame or not.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBullet_OnPostSimTick, const FBulletTimeStep&, TimeStep);

// Fired after a rollback. First param is the time step we've rolled back to. Second param is when we rolled back from, and represents a later frame that is no longer valid.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBullet_OnPostSimRollback, const FBulletTimeStep&, CurrentTimeStep, const FBulletTimeStep&, ExpungedTimeStep);

// Fired after changing movement modes. First param is the name of the previous movement mode. Second is the name of the new movement mode. 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBullet_OnMovementModeChanged, const FName&, PreviousMovementModeName, const FName&, NewMovementModeName);

// Fired when a teleport has succeeded
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBullet_OnTeleportSucceeded, const FVector&, FromLocation, const FQuat&, FromRotation, const FVector&, ToLocation, const FQuat&, ToRotation);

// Fired when a teleport has failed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBullet_OnTeleportFailed, const FVector&, FromLocation, const FQuat&, FromRotation, const FVector&, ToLocation, const FQuat&, ToRotation);

// Fired after a transition has been triggered.
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBullet_OnTransitionTriggered, const UBaseMovementModeTransition*, ModeTransition);

// Fired after a frame has been finalized. This may be a resimulation or not. No changes to state are possible. Guaranteed to be on the game thread.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBullet_OnPostFinalize, const FBulletSyncState&, SyncState, const FBulletAuxStateContext&, AuxState);

// Fired after proposed movement has been generated (i.e. after movement modes and layered moves have generated movement and mixed together).
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FBullet_ProcessGeneratedMovement, const FBulletTickStartData&, StartState, const FBulletTimeStep&, TimeStep, FBulletProposedMove&, OutProposedMove);

// Fired when a new event has been received from the simulation. This is a C++ only dispatch of the generic event. To dispatch the event to BP, prefer exposing the concrete event
// via a dedicated dynamic delegate (like OnMovementModeChanged).
// DECLARE_MULTICAST_DELEGATE_OneParam(FBullet_OnPostSimEventReceived, const FBulletSimulationEventData&); TODO:@GreggoryAddison::BulletActions | This is a similar concept to these events here so it might be worth making a clone of this.


UCLASS(MinimalAPI, Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class UBulletPhysicsEngineSimComp : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UBulletPhysicsEngineSimComp();

	BULLETNPP_API virtual void InitializeComponent() override;
	BULLETNPP_API virtual void UninitializeComponent() override;
	BULLETNPP_API virtual void OnRegister() override;
	BULLETNPP_API virtual void RegisterComponentTickFunctions(bool bRegister) override;
	BULLETNPP_API virtual void PostLoad() override;
	BULLETNPP_API virtual void BeginPlay() override;
	
	
	// Broadcast before each simulation tick.
	// Note - Guaranteed to run on the game thread (even in async simulation).
	UPROPERTY(BlueprintAssignable, Category = Bullet)
	FBullet_OnPreSimTick OnPreSimulationTick;

	// Broadcast at the end of a simulation tick after movement has occurred, but allowing additions/modifications to the state. Not assignable as a BP event due to it having mutable parameters.
	UPROPERTY()
	FBullet_OnPostMovement OnPostMovement;

	// Broadcast after each simulation tick and the state is finalized
	UPROPERTY(BlueprintAssignable, Category = Bullet)
	FBullet_OnPostSimTick OnPostSimulationTick;

	// Broadcast when a rollback has occurred, just before the next simulation tick occurs
	UPROPERTY(BlueprintAssignable, Category = Bullet)
	FBullet_OnPostSimRollback OnPostSimulationRollback;

	// Broadcast when a MovementMode has changed. Happens during a simulation tick if the mode changed that tick or when SetModeImmediately is used to change modes.
	UPROPERTY(BlueprintAssignable, Category = Bullet)
	FBullet_OnMovementModeChanged OnMovementModeChanged;

	// Broadcast when a teleport has succeeded
	UPROPERTY(BlueprintAssignable, Category = Bullet)
	FBullet_OnTeleportSucceeded OnTeleportSucceeded;

	// Broadcast when a teleport has failed
	UPROPERTY(BlueprintAssignable, Category = Bullet)
	FBullet_OnTeleportFailed OnTeleportFailed;

	// Broadcast when a Transition has been triggered.
	/*UPROPERTY(BlueprintAssignable, Category = Bullet)
	FBullet_OnTransitionTriggered OnMovementTransitionTriggered;*/

	// Broadcast after each finalized simulation frame, after the state is finalized. (Game thread only)
	UPROPERTY(BlueprintAssignable, Category = Bullet)
	FBullet_OnPostFinalize OnPostFinalize;

	// Fired when a new event has been received from the simulation
	// This happens after the event had been processed at the Bullet component level, which may
	// have caused a dedicated delegate, e.g. OnMovementModeChanged, to fire prior to this broadcast.
	// FBullet_OnPostSimEventReceived OnPostSimEventReceived;

	/**
	 * Broadcast after proposed movement has been generated. After movement modes and layered moves have generated movement and mixed together.
	 * This allows for final modifications to proposed movement before it's executed.
	 */
	FBullet_ProcessGeneratedMovement ProcessGeneratedMovement;
	
	
	// --------------------------------------------------------------------------------
	// NP Driver
	// --------------------------------------------------------------------------------

	// Get latest local input prior to simulation step. Called by backend system on owner's instance (autonomous or authority).
	BULLETNPP_API virtual void ProduceInput(const int32 DeltaTimeMS, FBulletInputCmdContext* Cmd);

	// Restore a previous frame prior to resimulating. Called by backend system. NewBaseTimeStep represents the current time and frame we'll simulate next.
	BULLETNPP_API void RestoreFrame(const FBulletSyncState* SyncState, const FBulletAuxStateContext* AuxState, const FBulletTimeStep& NewBaseTimeStep);

	// Take output for simulation. Called by backend system.
	BULLETNPP_API void FinalizeFrame(const FBulletSyncState* SyncState, const FBulletAuxStateContext* AuxState);

	// Take output for simulation when no simulation state changes have occurred. Called by backend system.
	BULLETNPP_API void FinalizeUnchangedFrame();

	// Take smoothed simulation state. Called by backend system, if supported.
	BULLETNPP_API void FinalizeSmoothingFrame(const FBulletSyncState* SyncState, const FBulletAuxStateContext* AuxState);

	// This is an opportunity to run code on the code on the simproxy in interpolated mode - currently used to help activate and deactivate modifiers on the simproxy in interpolated mode
	BULLETNPP_API void TickInterpolatedSimProxy(const FBulletTimeStep& TimeStep, const FBulletInputCmdContext& InputCmd, UBulletPhysicsEngineSimComp* BulletComp, const FBulletSyncState& CachedSyncState, const FBulletSyncState& SyncState, const FBulletAuxStateContext& AuxState);
	
	// Seed initial values based on component's state. Called by backend system.
	BULLETNPP_API void InitializeSimulationState(FBulletSyncState* OutSync, FBulletAuxStateContext* OutAux);

	// Primary movement simulation update. Given an starting state and timestep, produce a new state. Called by backend system.
	BULLETNPP_API void SimulationTick(const FBulletTimeStep& InTimeStep, const FBulletTickStartData& SimInput, OUT FBulletTickEndData& SimOutput);
	
	
	UFUNCTION(BlueprintCallable, Category = Bullet)
	BULLETNPP_API USceneComponent* GetUpdatedComponent() const;
};
