// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Components/ActorComponent.h"
#include "MotionWarpingAdapter.h"
#include "BulletMovementMode.h"
#include "BulletMoverTypes.h"
#include "BulletLayeredMove.h"
#include "BulletLayeredMoveBase.h"
#include "MoveLibrary/BulletBasedMovementUtils.h"
#include "MoveLibrary/BulletConstrainedMoveUtils.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Engine/HitResult.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "BulletMovementModifier.h"
#include "Backends/BulletMoverBackendLiaison.h"
#include "UObject/WeakInterfacePtr.h"
#include "Templates/SharedPointer.h"
#include "BulletMain.h"
#include "BulletMoverComponent.generated.h"

struct FBulletMoverTimeStep;
struct FBulletInstantMovementEffect;
struct FBulletMoverSimulationEventData;
class UBulletMovementModeStateMachine;
class UBulletMovementMixer;
class UBulletRollbackBlackboard;
class UBulletRollbackBlackboard_InternalWrapper;

namespace BulletMoverComponentConstants
{
	extern const FVector DefaultGravityAccel;		// Fallback gravity if not determined by the component or world (cm/s^2)
	extern const FVector DefaultUpDir;				// Fallback up direction if not determined by the component or world (normalized)
}

// Fired just before a simulation tick, regardless of being a re-simulated frame or not.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletMover_OnPreSimTick, const FBulletMoverTimeStep&, TimeStep, const FBulletMoverInputCmdContext&, InputCmd);

// Fired during a simulation tick, after the input is processed but before the actual move calculation.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBulletMover_OnPreMovement, const FBulletMoverTimeStep&, TimeStep, const FBulletMoverInputCmdContext&, InputCmd, const FBulletMoverSyncState&, SyncState, const FBulletMoverAuxStateContext&, AuxState);

// Fired during a simulation tick, after movement has occurred but before the state is finalized, allowing changes to the output state.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBulletMover_OnPostMovement, const FBulletMoverTimeStep&, TimeStep, FBulletMoverSyncState&, SyncState, FBulletMoverAuxStateContext&, AuxState);

// Fired after a simulation tick, regardless of being a re-simulated frame or not.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBulletMover_OnPostSimTick, const FBulletMoverTimeStep&, TimeStep);

// Fired after a rollback. First param is the time step we've rolled back to. Second param is when we rolled back from, and represents a later frame that is no longer valid.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletMover_OnPostSimRollback, const FBulletMoverTimeStep&, CurrentTimeStep, const FBulletMoverTimeStep&, ExpungedTimeStep);

// Fired after changing movement modes. First param is the name of the previous movement mode. Second is the name of the new movement mode. 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletMover_OnMovementModeChanged, const FName&, PreviousMovementModeName, const FName&, NewMovementModeName);

// Fired when a teleport has succeeded
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FBulletMover_OnTeleportSucceeded, const FVector&, FromLocation, const FQuat&, FromRotation, const FVector&, ToLocation, const FQuat&, ToRotation);

// Fired when a teleport has failed
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FBulletMover_OnTeleportFailed, const FVector&, FromLocation, const FQuat&, FromRotation, const FVector&, ToLocation, const FQuat&, ToRotation, ETeleportFailureReason, TeleportFailureReason);

// Fired after a transition has been triggered.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBulletMover_OnTransitionTriggered, const UBulletBaseMovementModeTransition*, ModeTransition);

// Fired after a frame has been finalized. This may be a resimulation or not. No changes to state are possible. Guaranteed to be on the game thread.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBulletMover_OnPostFinalize, const FBulletMoverSyncState&, SyncState, const FBulletMoverAuxStateContext&, AuxState);

// Fired after proposed movement has been generated (i.e. after movement modes and layered moves have generated movement and mixed together).
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FBulletMover_ProcessGeneratedMovement, const FBulletMoverTickStartData&, StartState, const FBulletMoverTimeStep&, TimeStep, FBulletProposedMove&, OutProposedMove);

// Fired when a new event has been received from the simulation. This is a C++ only dispatch of the generic event. To dispatch the event to BP, prefer exposing the concrete event
// via a dedicated dynamic delegate (like OnMovementModeChanged).
DECLARE_MULTICAST_DELEGATE_OneParam(FBulletMover_OnPostSimEventReceived, const FBulletMoverSimulationEventData&);

/**
 * 
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, meta = (BlueprintSpawnableComponent))
class UBulletMoverComponent : public UActorComponent
{
	GENERATED_BODY()


public:	
	BULLETMOVER_API UBulletMoverComponent();

	BULLETMOVER_API virtual void InitializeComponent() override;
	BULLETMOVER_API virtual void UninitializeComponent() override;
	BULLETMOVER_API virtual void OnRegister() override;
	BULLETMOVER_API virtual void RegisterComponentTickFunctions(bool bRegister) override;
	BULLETMOVER_API virtual void PostLoad() override;
	BULLETMOVER_API virtual void OnModifyContacts();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	// Broadcast before each simulation tick.
	// Note - Guaranteed to run on the game thread (even in async simulation).
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnPreSimTick OnPreSimulationTick;

	// Broadcast at the end of a simulation tick after movement has occurred, but allowing additions/modifications to the state. Not assignable as a BP event due to it having mutable parameters.
	UPROPERTY()
	FBulletMover_OnPostMovement OnPostMovement;

	// Broadcast after each simulation tick and the state is finalized
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnPostSimTick OnPostSimulationTick;

	// Broadcast when a rollback has occurred, just before the next simulation tick occurs
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnPostSimRollback OnPostSimulationRollback;

	// Broadcast when a MovementMode has changed. Happens during a simulation tick if the mode changed that tick or when SetModeImmediately is used to change modes.
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnMovementModeChanged OnMovementModeChanged;

	// Broadcast when a teleport has succeeded
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnTeleportSucceeded OnTeleportSucceeded;

	// Broadcast when a teleport has failed
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnTeleportFailed OnTeleportFailed;

	// Broadcast when a Transition has been triggered.
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnTransitionTriggered OnMovementTransitionTriggered;

	// Broadcast after each finalized simulation frame, after the state is finalized. (Game thread only)
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnPostFinalize OnPostFinalize;

	// Fired when a new event has been received from the simulation
	// This happens after the event had been processed at the mover component level, which may
	// have caused a dedicated delegate, e.g. OnMovementModeChanged, to fire prior to this broadcast.
	FBulletMover_OnPostSimEventReceived OnPostSimEventReceived;

	/**
	 * Broadcast after proposed movement has been generated. After movement modes and layered moves have generated movement and mixed together.
	 * This allows for final modifications to proposed movement before it's executed.
	 */
	FBulletMover_ProcessGeneratedMovement ProcessGeneratedMovement;
	
	uint8 bIsClientUsingSmoothing : 1 = 0;

	// Binds event for processing movement after it has been generated. Allows for final modifications to proposed movement before it's executed.
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void BindProcessGeneratedMovement(FBulletMover_ProcessGeneratedMovement ProcessGeneratedMovementEvent);
	// Clears current bound event for processing movement after it has been generated.
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void UnbindProcessGeneratedMovement();
	
	// Callbacks
	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) { }

	// --------------------------------------------------------------------------------
	// NP Driver
	// --------------------------------------------------------------------------------

	// Get latest local input prior to simulation step. Called by backend system on owner's instance (autonomous or authority).
	BULLETMOVER_API virtual void ProduceInput(const int32 DeltaTimeMS, FBulletMoverInputCmdContext* Cmd);

	// Restore a previous frame prior to resimulating. Called by backend system. NewBaseTimeStep represents the current time and frame we'll simulate next.
	BULLETMOVER_API void RestoreFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState, const FBulletMoverTimeStep& NewBaseTimeStep);

	// Take output for simulation. Called by backend system.
	BULLETMOVER_API void FinalizeFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState);

	// Take output for simulation when no simulation state changes have occurred. Called by backend system.
	BULLETMOVER_API void FinalizeUnchangedFrame();

	// Take smoothed simulation state. Called by backend system, if supported.
	BULLETMOVER_API void FinalizeSmoothingFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState);

	// This is an opportunity to run code on the code on the simproxy in interpolated mode - currently used to help activate and deactivate modifiers on the simproxy in interpolated mode
	BULLETMOVER_API void TickInterpolatedSimProxy(const FBulletMoverTimeStep& TimeStep, const FBulletMoverInputCmdContext& InputCmd, UBulletMoverComponent* MoverComp, const FBulletMoverSyncState& CachedSyncState, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState);
	
	// Seed initial values based on component's state. Called by backend system.
	BULLETMOVER_API void InitializeSimulationState(FBulletMoverSyncState* OutSync, FBulletMoverAuxStateContext* OutAux);

	// Primary movement simulation update. Given an starting state and timestep, produce a new state. Called by backend system.
	BULLETMOVER_API void SimulationTick(const FBulletMoverTimeStep& InTimeStep, const FBulletMoverTickStartData& SimInput, OUT FBulletMoverTickEndData& SimOutput);
	
	// Primary movement simulation update. Given an starting state and timestep, produce a new state. Called by backend system.
	BULLETMOVER_API void PostPhysicsTick(OUT FBulletMoverTickEndData& SimOutput);

	// Specifies which supporting back end class should drive this Mover actor
	UPROPERTY(EditDefaultsOnly, Category = Mover, meta = (MustImplement = "/Script/BulletMover.MoverBackendLiaisonInterface"))
	TSubclassOf<UActorComponent> BackendClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = Mover, meta=(FullyExpand=true))
	TMap<FName, TObjectPtr<UBulletBaseMovementMode>> MovementModes;

	// Name of the first mode to start in when simulation begins. Must have a mapping in MovementModes. Only used during initialization.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Mover, meta=(GetOptions=GetStartingMovementModeNames))
	FName StartingMovementMode = NAME_None;

	// Transition checks that are always evaluated regardless of mode. Evaluated in order, stopping at the first successful transition check. Mode-owned transitions take precedence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category=Mover)
	TArray<TObjectPtr<UBulletBaseMovementModeTransition>> Transitions;

	/** List of types that should always be present in this actor's sync state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	TArray<FBulletMoverDataPersistence> PersistentSyncStateDataTypes;

	/** Optional object for producing input cmds. Typically set at BeginPlay time. If not specified, defaulted input will be used.
	*   Note that any other actor component implementing MoverInputProducerInterface on this component's owner will also be able
	*   to produce input commands if bGatherInputFromAllInputProducerComponents is true. @see bGatherInputFromAllInputProducerComponents
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover, meta = (ObjectMustImplement = "/Script/BulletMover.MoverInputProducerInterface"))
	TObjectPtr<UObject> InputProducer;

	/** If true, any actor component implementing MoverInputProducerInterface on this component's owner will be able to produce input commands */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Mover)
	bool bGatherInputFromAllInputProducerComponents = true;
	
	/** If true, any input commands will be ignored */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Mover)
	bool bIgnoreAnyInputProducer = false;

	/* All MoverInputProducerInterface objects producing input for this mover component. If bGatherInputFromAllInputProducerComponents
	*  is true, all components implementing MoverInputProducerInterface on this component's owner will be added to 
	*  this array at BeginPlay time, and IBulletMoverInputProducerInterface::ProduceInput will be called on each within UBulletMoverComponent::ProduceInput.
	*  The order shouldn't matter, as this is for input commands independent of each other, driving different movement modes.
	*  If order is important, set bGatherInputFromAllInputProducerComponents to false and implement a dedicated input component instead,
	*  gathering input from different sources in a custom order and set it as the InputProducer.
	*/
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> InputProducers;

	/** Optional object for mixing proposed moves.Typically set at BeginPlay time. If not specified, UDefaultMovementMixer will be used. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover)
	TObjectPtr<UBulletMovementMixer> MovementMixer;

	const TArray<TObjectPtr<UBulletLayeredMoveLogic>>* GetRegisteredMoves() const;
	
	/** Registers layered move logic */
	template <typename MoveT UE_REQUIRES(std::is_base_of_v<UBulletLayeredMoveLogic, MoveT>)>
	void RegisterMove(TSubclassOf<MoveT> MoveClass = MoveT::StaticClass())
	{
		K2_RegisterMove(MoveClass);
	}

	/** Registers layered move logic */
	UFUNCTION(BlueprintCallable, Category = Mover, DisplayName = "Register Move")
	void K2_RegisterMove(TSubclassOf<UBulletLayeredMoveLogic> MoveClass);

	/** Registers an array of layered move logic classes */
	UFUNCTION(BlueprintCallable, Category = Mover, DisplayName = "Register Moves")
	void K2_RegisterMoves(TArray<TSubclassOf<UBulletLayeredMoveLogic>> MoveClasses);
	
	/** Unregisters layered move logic */
	template <typename MoveT UE_REQUIRES(std::is_base_of_v<UBulletLayeredMoveLogic, MoveT>)>
	void UnregisterMove(TSubclassOf<MoveT> MoveClass = MoveT::StaticClass())
	{
		K2_UnregisterMove(MoveClass);
	}

	/** Unregisters layered move logic */
	UFUNCTION(BlueprintCallable, Category = Mover, DisplayName = "Unregister Move")
	void K2_UnregisterMove(TSubclassOf<UBulletLayeredMoveLogic> MoveClass);
	
	template <typename MoveT, typename ActivationParamsT UE_REQUIRES(std::is_base_of_v<UBulletLayeredMoveLogic, MoveT> && std::is_base_of_v<typename MoveT::MoveDataType::ActivationParamsType, ActivationParamsT>)>
	bool QueueLayeredMoveActivationWithContext(const ActivationParamsT& ActivationParams, TSubclassOf<MoveT> MoveClass = MoveT::StaticClass())
	{
		return MakeAndQueueLayeredMove(MoveClass, &ActivationParams);
	}
	
	/**
	 * Queues a layered move for activation.
	 * Takes a Activation Context which provides context to set Layered Move Data.
	 * Make sure Activation Context type matches layered Move Data
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "MoveAsRawData", AllowAbstract = "false"), DisplayName = "Queue Layered Move Activation With Context")
	bool K2_QueueLayeredMoveActivationWithContext(TSubclassOf<UBulletLayeredMoveLogic> MoveLogicClass, UPARAM(DisplayName="Layered Move Activation Context") const int32& MoveAsRawData);
	DECLARE_FUNCTION(execK2_QueueLayeredMoveActivationWithContext);

	/**
 	 * Queues a layered move for activation.
 	 * Takes NO Activation Context meaning the layered move will be activated using default Move Data.
 	 * Note: Changing Move Data is still possible in the layered move logic itself
 	 * See QueueLayeredMoveActivationWithContext for activating a layered move with context
 	 */
	UFUNCTION(BlueprintCallable, Category = Mover, meta = (AllowAbstract = "false"))
	bool QueueLayeredMoveActivation(TSubclassOf<UBulletLayeredMoveLogic> MoveLogicClass);
	
	/**
	 * Queue a layered move to start during the next simulation frame. This will clone whatever move you pass in, so you'll need to fully set it up before queuing.
	 * @param LayeredMove			The move to queue, which must be a LayeredMoveBase sub-type. 
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "MoveAsRawData", AllowAbstract = "false", DisplayName = "Queue Layered Move"))
	BULLETMOVER_API void K2_QueueLayeredMove(UPARAM(DisplayName="Layered Move") const int32& MoveAsRawData);
	DECLARE_FUNCTION(execK2_QueueLayeredMove);

	// Queue a layered move to start during the next simulation frame
	BULLETMOVER_API void QueueLayeredMove(TSharedPtr<FBulletLayeredMoveBase> Move);
	
	/**
 	 * Queue a Movement Modifier to start during the next simulation frame. This will clone whatever move you pass in, so you'll need to fully set it up before queuing.
 	 * @param MovementModifier The modifier to queue, which must be a LayeredMoveBase sub-type.
 	 * @return Returns a Modifier handle that can be used to query or cancel the movement modifier
 	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "MoveAsRawData", AllowAbstract = "false", DisplayName = "Queue Movement Modifier"))
	BULLETMOVER_API FBulletMovementModifierHandle K2_QueueMovementModifier(UPARAM(DisplayName="Movement Modifier") const int32& MoveAsRawData);
	DECLARE_FUNCTION(execK2_QueueMovementModifier);

	// Queue a Movement Modifier to start during the next simulation frame.
	BULLETMOVER_API FBulletMovementModifierHandle QueueMovementModifier(TSharedPtr<FBulletMovementModifierBase> Modifier);
	
	/**
	 * Cancel any active or queued Modifiers with the handle passed in.
	 */
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void CancelModifierFromHandle(FBulletMovementModifierHandle ModifierHandle);

	/**
	 * Cancel any active or queued movement features (layered moves, modifiers, etc.) that have a matching gameplay tag. Does not affect the active movement mode.
	 */
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void CancelFeaturesWithTag(FGameplayTag TagToCancel, bool bRequireExactMatch=false);

	/**
	 * Queue an Instant Movement Effect to start at the end of this frame or start of the next subtick - whichever happens first. This will clone whatever move you pass in, so you'll need to fully set it up before queuing.
	 * @param InstantMovementEffect			The effect to queue, which must be a FBulletInstantMovementEffect sub-type. 
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "EffectAsRawData", AllowAbstract = "false", DisplayName = "Queue Instant Movement Effect"))
	BULLETMOVER_API void K2_QueueInstantMovementEffect(UPARAM(DisplayName="Instant Movement Effect") const int32& EffectAsRawData);
	DECLARE_FUNCTION(execK2_QueueInstantMovementEffect);

	/**
	 * Schedule an Instant Movement Effect to be applied as early as possible while ensuring it gets executed on the same frame on all networked end points.
	 * This adds a delay to the application of the effect, tunable in the NetworkPhysicsSettingsComponent. @see UNetworkPhysicsSettingsComponent, @see FNetworkPhysicsSettings, @see EventSchedulingMinDelaySeconds
	 * This will clone whatever move you pass in, so you'll need to fully set it up before queuing.
	 * @param InstantMovementEffect			The effect to queue, which must be a FBulletInstantMovementEffect sub-type. 
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "EffectAsRawData", AllowAbstract = "false", DisplayName = "Schedule Instant Movement Effect"))
	BULLETMOVER_API void K2_ScheduleInstantMovementEffect(UPARAM(DisplayName="Instant Movement Effect") const int32& EffectAsRawData);
	DECLARE_FUNCTION(execK2_ScheduleInstantMovementEffect);

	/**
	 *  Queue a Instant Movement Effect to take place at the end of this frame or start of the next subtick - whichever happens first
	 *  @param InstantMovementEffect			The effect to queue, which must be a FBulletInstantMovementEffect sub - type.
	 */ 
	BULLETMOVER_API void QueueInstantMovementEffect(TSharedPtr<FBulletInstantMovementEffect> InstantMovementEffect);
	/** 
	 * Queue a scheduled Instant Movement Effect to take place after delay (tunable in the NetworkPhysicsSettingsComponent)
	 * ensuring it gets executed on the same frame on all networked end points. @see UNetworkPhysicsSettingsComponent, @see FNetworkPhysicsSettings, @see EventSchedulingMinDelaySeconds
	 * @param InstantMovementEffect			The effect to queue, which must be a FBulletInstantMovementEffect sub - type.
	 */
	BULLETMOVER_API void ScheduleInstantMovementEffect(TSharedPtr<FBulletInstantMovementEffect> InstantMovementEffect);

	// Get the queued instant movement effects. This is mostly for internal use, general users should abstain from calling it.
	BULLETMOVER_API const TArray<FBulletScheduledInstantMovementEffect>& GetQueuedInstantMovementEffects() const;
	// Clears the queued instant movement effects. This is mostly for internal use, general users should abstain from calling it.
	BULLETMOVER_API void ClearQueuedInstantMovementEffects();
	// Queue an instant movement effect in async mode. Do not use on the game thread.
	void QueueInstantMovementEffect_Internal(const FBulletMoverTimeStep& TimeStep, TSharedPtr<FBulletInstantMovementEffect> InstantMovementEffect);

protected:
	// Queue a scheduled instant movement effect. Thread safe, can be used outside the game thread.
	void QueueInstantMovementEffect(const FBulletScheduledInstantMovementEffect& ScheduledInstantMovementEffect);

public:	
	// Queue a movement mode change to occur during the next simulation frame. If bShouldReenter is true, then a mode change will occur even if already in that mode.
	UFUNCTION(BlueprintCallable, Category = Mover, DisplayName="Queue Next Movement Mode")
	BULLETMOVER_API void QueueNextMode(FName DesiredModeName, bool bShouldReenter=false);

	// Add a movement mode to available movement modes. Returns true if the movement mode was added successfully. Returns the mode that was made.
	UFUNCTION(BlueprintCallable, Category = Mover, meta=(DeterminesOutputType="BulletMovementMode"))
	BULLETMOVER_API UBulletBaseMovementMode* AddMovementModeFromClass(FName ModeName, UPARAM(meta = (AllowAbstract = "false"))TSubclassOf<UBulletBaseMovementMode> MovementMode);

	// Add a movement mode to available movement modes. Returns true if the movement mode was added successfully
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API bool AddMovementModeFromObject(FName ModeName, UBulletBaseMovementMode* MovementMode);
	
	// Removes a movement mode from available movement modes. Returns number of modes removed from the available movement modes.
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API bool RemoveMovementMode(FName ModeName);
	
public:
	// Set gravity override, as a directional acceleration in worldspace.  Gravity on Earth would be {x=0,y=0,z=-980}
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void SetGravityOverride(bool bOverrideGravity, FVector GravityAcceleration=FVector::ZeroVector);
	
	// Get the current acceleration due to gravity (cm/s^2) in worldspace
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Mover)
	BULLETMOVER_API FVector GetGravityAcceleration() const;

	/** Returns a quaternion transforming from world to gravity space. */
	FQuat GetWorldToGravityTransform() const { return WorldToGravityTransform; }

	/** Returns a quaternion transforming from gravity to world space. */
	FQuat GetGravityToWorldTransform() const { return GravityToWorldTransform; }

	// Set UpDirection override. This is a fixed direction that overrides the gravity-derived up direction.
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void SetUpDirectionOverride(bool bOverrideUpDirection, FVector UpDirection=FVector::UpVector);

	// Get the normalized direction considered "up" in worldspace. Typically aligned with gravity, and typically determines the plane an actor tries to move along.
	UFUNCTION(BlueprintPure = false, Category = Mover)
	BULLETMOVER_API FVector GetUpDirection() const;

	// Access the planar constraint that may be limiting movement direction
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Mover)
	BULLETMOVER_API const FBulletPlanarConstraint& GetPlanarConstraint() const;

	// Sets planar constraint that can limit movement direction
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void SetPlanarConstraint(const FBulletPlanarConstraint& InConstraint);

	// Sets BaseVisualComponentTransform used for cases where we want to move the visual component away from the root component. See @BaseVisualComponentTransform
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void SetBaseVisualComponentTransform (const FTransform& ComponentTransform);

	// Gets BaseVisualComponentTransform used for cases where we want to move the visual component away from the root component. See @BaseVisualComponentTransform
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API FTransform GetBaseVisualComponentTransform() const;

	/** Sets whether this mover component can use grouped movement updates, which improve performance but can cause attachments to update later than expected */
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void SetUseDeferredGroupMovement(bool bEnable);

	/** Returns true if this component is actually using grouped movement updates, which checks the flag and any global settings */
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API bool IsUsingDeferredGroupMovement() const;
	
public:

	/**
	 *  Converts a local root motion transform to worldspace. 
	 * @param AlternateActorToWorld   allows specification of a different actor root transform, for cases when root motion isn't directly being applied to this actor (async simulations)
	 * @param OptionalWarpingContext   allows specification of a warping context, for use with root motion that is asynchronous from the actor (async simulations)
	 */
	BULLETMOVER_API virtual FTransform ConvertLocalRootMotionToWorld(const FTransform& LocalRootMotionTransform, float DeltaSeconds, const FTransform* AlternateActorToWorld=nullptr, const FMotionWarpingUpdateContext* OptionalWarpingContext=nullptr) const;

	/** delegates used when converting local root motion to worldspace, allowing external systems to influence it (such as motion warping) */
	FOnWarpLocalspaceRootMotionWithContext ProcessLocalRootMotionDelegate;
	FOnWarpWorldspaceRootMotionWithContext ProcessWorldRootMotionDelegate;

public:	// Queries

	// Get the transform of the root component that our Mover simulation is moving
	BULLETMOVER_API FTransform GetUpdatedComponentTransform() const;

	// Sets which component we're using as the root of our movement
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
	
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void SetBulletPhysicsComponent(UPrimitiveComponent* NewPhysicsComponent);

	// Access the root component of the actor that our Mover simulation is moving
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API USceneComponent* GetUpdatedComponent() const;

	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API UPrimitiveComponent* GetUpdatedPrimitive() const;

	// Typed accessor to root moving component
	template<class T>
	T* GetUpdatedComponent() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, const USceneComponent>::Value, "'T' template parameter to GetUpdatedComponent must be derived from USceneComponent");
		return Cast<T>(GetUpdatedComponent());
	}
	
	// Access the root component of the actor that our Mover simulation is moving
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API UPrimitiveComponent* GetBulletPhysicsBodyComponent() const;

	// Access the primary visual component of the actor
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API USceneComponent* GetPrimaryVisualComponent() const;

	// Typed accessor to primary visual component
	template<class T>
	T* GetPrimaryVisualComponent() const
	{
		return Cast<T>(GetPrimaryVisualComponent());
	}

	// Sets this Mover actor's primary visual component. Must be a descendant of the updated component that acts as our movement root. 
	UFUNCTION(BlueprintCallable, Category=Mover)
	BULLETMOVER_API void SetPrimaryVisualComponent(USceneComponent* SceneComponent);

	// Get the current velocity (units per second, worldspace)
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API FVector GetVelocity() const;

	// Get the intended movement direction in worldspace with magnitude (range 0-1)
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API FVector GetMovementIntent() const;

	// Get the orientation that the actor is moving towards
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API FRotator GetTargetOrientation() const;

	/** Get a sampling of where the actor is projected to be in the future, based on a current state. Note that this is projecting ideal movement without doing full simulation and collision. */
	UE_DEPRECATED(5.5, "Use GetPredictedTrajectory instead.")
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Mover)
	BULLETMOVER_API TArray<FBulletTrajectorySampleInfo> GetFutureTrajectory(float FutureSeconds, float SamplesPerSecond);

	/** Get a sampling of where the actor is projected to be in the future, based on a current state. Note that this is projecting ideal movement without doing full simulation and collision.
	 * The first sample info of the returned array corresponds to the current state of the mover. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Mover)
	BULLETMOVER_API TArray<FBulletTrajectorySampleInfo> GetPredictedTrajectory(FBulletMoverPredictTrajectoryParams PredictionParams);	

	// Get the current movement mode name
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API FName GetMovementModeName() const;
	
	// Get the current movement mode 
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API const UBulletBaseMovementMode* GetMovementMode() const;

	// Get the current movement base. Null if there isn't one.
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API UPrimitiveComponent* GetMovementBase() const;

	// Get the current movement base bone, NAME_None if there isn't one.
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API FName GetMovementBaseBoneName() const;

	// Signals whether we have a sync state saved yet. If not, most queries will not be meaningful.
	UE_DEPRECATED(5.6, "HasValidCachedState has been deprecated, and is not needed since we no longer wait until movement simulation begins before providing a valid sync state.")
	UFUNCTION(BlueprintPure, Category = Mover, meta=(DeprecatedFunction, DeprecationMessage="HasValidCachedState has been deprecated, and is not needed since we no longer wait until movement simulation begins before providing a valid sync state."))
	BULLETMOVER_API bool HasValidCachedState() const;

	// Access the most recent captured sync state.
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API const FBulletMoverSyncState& GetSyncState() const;

	// Signals whether we have input data saved yet. If not, input queries will not be meaningful.
	UE_DEPRECATED(5.6, "HasValidCachedInputCmd has been deprecated, and is not needed since we no longer wait until movement simulation begins before providing a valid input cmd.")
	UFUNCTION(BlueprintPure, Category = Mover, meta = (DeprecatedFunction, DeprecationMessage = "HasValidCachedInputCmd has been deprecated, and is not needed since we no longer wait until movement simulation begins before providing a valid input cmd."))
	BULLETMOVER_API bool HasValidCachedInputCmd() const;

	// Access the most recently-used inputs.
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API const FBulletMoverInputCmdContext& GetLastInputCmd() const;

	// Get the most recent TimeStep
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API const FBulletMoverTimeStep& GetLastTimeStep() const;

	// Access the most recent floor check hit result.
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API virtual bool TryGetFloorCheckHitResult(FHitResult& OutHitResult) const;

	// Access the read-only version of the Mover's Blackboard
	UFUNCTION(BlueprintPure, Category=Mover)
	BULLETMOVER_API const UBulletMoverBlackboard* GetSimBlackboard() const;

	BULLETMOVER_API UBulletMoverBlackboard* GetSimBlackboard_Mutable() const;


	UBulletRollbackBlackboard* GetRollbackBlackboard() const { return RollbackBlackboard.Get(); }
	UBulletRollbackBlackboard_InternalWrapper* GetRollbackBlackboard_Internal() const { return RollbackBlackboard_InternalWrapper.Get(); }

	/** Find settings object by type. Returns null if there is none of that type */
	const IBulletMovementSettingsInterface* FindSharedSettings(const UClass* ByType) const { return FindSharedSettings_Mutable(ByType); }
	template<typename SettingsT = IBulletMovementSettingsInterface UE_REQUIRES(std::is_base_of_v<IBulletMovementSettingsInterface, SettingsT>)>
	const SettingsT* FindSharedSettings() const { return Cast<const SettingsT>(FindSharedSettings(SettingsT::StaticClass())); }

	/** Find mutable settings object by type. Returns null if there is none of that type */
	BULLETMOVER_API IBulletMovementSettingsInterface* FindSharedSettings_Mutable(const UClass* ByType) const;
	template<typename SettingsT = IBulletMovementSettingsInterface UE_REQUIRES(std::is_base_of_v<IBulletMovementSettingsInterface, SettingsT>)>
	SettingsT* FindSharedSettings_Mutable() const { return Cast<SettingsT>(FindSharedSettings_Mutable(SettingsT::StaticClass())); }

	/** Find mutable settings object by type. Returns null if there is none of that type */
	UFUNCTION(BlueprintPure, Category = Mover,  meta=(DeterminesOutputType="SharedSetting", DisplayName="Find Shared Settings Mutable"))
	BULLETMOVER_API UObject* FindSharedSettings_Mutable_BP(UPARAM(meta = (MustImplement = "MovementSettingsInterface")) TSubclassOf<UObject> SharedSetting) const;

	/** Find settings object by type. Returns null if there is none of that type */
	UFUNCTION(BlueprintPure, Category = Mover,  meta=(DeterminesOutputType="SharedSetting", DisplayName="Find Shared Settings"))
	BULLETMOVER_API const UObject* FindSharedSettings_BP(UPARAM(meta = (MustImplement = "MovementSettingsInterface")) TSubclassOf<UObject> SharedSetting) const;

	/** Gets the currently active movement mode, provided it is of the given type. Returns nullptr if there is no active mode yet, or if it's of a different type. */
	template<typename ModeT = UBulletBaseMovementMode UE_REQUIRES(std::is_base_of_v<UBulletBaseMovementMode, ModeT>)>
	const ModeT* GetActiveMode(bool bRequireExactClass = false) const { return Cast<ModeT>(GetActiveModeInternal(ModeT::StaticClass(), bRequireExactClass)); }

	/** Gets the currently active movement mode, provided it is of the given type. Returns nullptr if there is no active mode yet, or if it's of a different type. */
	template<typename ModeT = UBulletBaseMovementMode UE_REQUIRES(std::is_base_of_v<UBulletBaseMovementMode, ModeT>)>
	ModeT* GetActiveMode_Mutable(bool bRequireExactClass = false) const { return Cast<ModeT>(GetActiveModeInternal(ModeT::StaticClass(), bRequireExactClass)); }

	/** Find the first movement mode on this component with the given type, optionally of the given type exactly. Returns null if there is none of that type */
	template<typename ModeT = UBulletBaseMovementMode UE_REQUIRES(std::is_base_of_v<UBulletBaseMovementMode, ModeT>)>
	ModeT* FindMode_Mutable(bool bRequireExactClass = false) const { return Cast<ModeT>(FindMode_Mutable(ModeT::StaticClass(), bRequireExactClass)); }
	BULLETMOVER_API UBulletBaseMovementMode* FindMode_Mutable(TSubclassOf<UBulletBaseMovementMode> ModeType, bool bRequireExactClass = false) const;

	/** Find the movement mode on this component the given name and type, optionally of the given type exactly. Returns null if there is no mode by that name, or if it's of a different type. */
	template<typename ModeT = UBulletBaseMovementMode UE_REQUIRES(std::is_base_of_v<UBulletBaseMovementMode, ModeT>)>
	ModeT* FindMode_Mutable(FName MovementModeName, bool bRequireExactClass = false) const { return Cast<ModeT>(FindMode_Mutable(ModeT::StaticClass(), MovementModeName, bRequireExactClass)); }
	BULLETMOVER_API UBulletBaseMovementMode* FindMode_Mutable(TSubclassOf<UBulletBaseMovementMode> ModeType, FName ModeName, bool bRequireExactClass = false) const;
	
	UFUNCTION(BlueprintPure, Category = Mover,  meta=(DeterminesOutputType="BulletMovementMode"))
	BULLETMOVER_API UBulletBaseMovementMode* FindMovementMode(TSubclassOf<UBulletBaseMovementMode> MovementMode) const;

	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API UBulletBaseMovementMode* FindMovementModeByName(FName MovementModeName) const;
	
	/**
	 * Retrieves an active layered move, by writing to a target instance if it is the matching type. Note: Writing to the struct returned will not modify the active struct.
	 * @param DidSucceed			Flag indicating whether data was actually written to target struct instance
	 * @param TargetAsRawBytes		The data struct instance to write to, which must be a FBulletLayeredMoveBase sub-type
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "TargetAsRawBytes", AllowAbstract = "false", DisplayName = "Find Active Layered Move"))
	BULLETMOVER_API void K2_FindActiveLayeredMove(bool& DidSucceed, UPARAM(DisplayName = "Out Layered Move") int32& TargetAsRawBytes) const;
	DECLARE_FUNCTION(execK2_FindActiveLayeredMove);

	// Find an active layered move by type. Returns null if one wasn't found 
	BULLETMOVER_API const FBulletLayeredMoveBase* FindActiveLayeredMoveByType(const UScriptStruct* DataStructType) const;

	/** Find a layered move of a specific type in this components active layered moves. If not found, null will be returned. */
	template <typename MoveT = FBulletLayeredMoveBase UE_REQUIRES(std::is_base_of_v<FBulletLayeredMoveBase, MoveT>)>
	const MoveT* FindActiveLayeredMoveByType() const { return static_cast<const MoveT*>(FindActiveLayeredMoveByType(MoveT::StaticStruct())); }
	
	/**
	 * Retrieves Movement modifier by writing to a target instance if it is the matching type. Note: Writing to the struct returned will not modify the active struct.
	 * @param ModifierHandle		Handle of the modifier we're trying to cancel
	 * @param bFoundModifier		Flag indicating whether modifier was found and data was actually written to target struct instance
	 * @param TargetAsRawBytes		The data struct instance to write to, which must be a FBulletMovementModifierBase sub-type
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "TargetAsRawBytes", AllowAbstract = "false", DisplayName = "Find Movement Modifier"))
	BULLETMOVER_API void K2_FindMovementModifier(FBulletMovementModifierHandle ModifierHandle, bool& bFoundModifier, UPARAM(DisplayName = "Out Movement Modifier") int32& TargetAsRawBytes) const;
	DECLARE_FUNCTION(execK2_FindMovementModifier);

	// Checks if the modifier handle passed in is active or queued on this mover component
	UFUNCTION(BlueprintPure, Category = Mover)
	BULLETMOVER_API bool IsModifierActiveOrQueued(const FBulletMovementModifierHandle& ModifierHandle) const;
	
	// Find movement modifier by it's handle. Returns nullptr if the modifier couldn't be found
	BULLETMOVER_API const FBulletMovementModifierBase* FindMovementModifier(const FBulletMovementModifierHandle& ModifierHandle) const;

	// Find movement modifier by type (returns the first modifier it finds). Returns nullptr if the modifier couldn't be found
	BULLETMOVER_API const FBulletMovementModifierBase* FindMovementModifierByType(const UScriptStruct* DataStructType) const;
	
	/** Find a movement modifier of a specific type in this components movement modifiers. If not found, null will be returned. */
	template <typename ModifierT = FBulletMovementModifierBase UE_REQUIRES(std::is_base_of_v<FBulletMovementModifierBase, ModifierT>)>
	const ModifierT* FindMovementModifierByType() const { return static_cast<const ModifierT*>(FindMovementModifierByType(ModifierT::StaticStruct())); }
	
	/**
 	 * Check Mover systems for a gameplay tag.
 	 *
 	 * @param TagToFind			Tag to check on the Mover systems
 	 * @param bExactMatch		If true, the tag has to be exactly present, if false then TagToFind will include it's parent tags while matching
 	 * 
 	 * @return True if the TagToFind was found
 	 */
	UFUNCTION(BlueprintPure, Category = Mover, meta = (Keywords = "HasTag"))
	BULLETMOVER_API bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const;

	/**
	 * Check Mover systems for a gameplay tag. Use the given state, as well as any loose tags on the MoverComponent.
	 *
	 * @param TagToFind			Tag to check on the MoverComponent or state
	 * @param bExactMatch		If true, the tag has to be exactly present, if false then TagToFind will include it's parent tags while matching
	 *
	 * @return True if the TagToFind was found
	 */
	UFUNCTION(BlueprintPure, Category = Mover, meta = (Keywords = "HasTag"))
	BULLETMOVER_API bool HasGameplayTagInState(const FBulletMoverSyncState& SyncState, FGameplayTag TagToFind, bool bExactMatch) const;

	/**
  	 * Adds a gameplay tag to this Mover Component.
  	 * Note: Duplicate tags will not be added
  	 * @param TagToAdd			Tag to add to the Mover Component
  	 */
	UFUNCTION(BlueprintCallable, Category = Mover, meta = (Keywords = "Add Tag"))
	BULLETMOVER_API void AddGameplayTag(FGameplayTag TagToAdd);

	/**
   	 * Adds a series of gameplay tags to this Mover Component
   	 * Note: Duplicate tags will not be added
   	 * @param TagsToAdd			Tags to add/append to the Mover Component
   	 */
	UFUNCTION(BlueprintCallable, Category = Mover, meta = (Keywords = "Add Tag"))
	BULLETMOVER_API void AddGameplayTags(const FGameplayTagContainer& TagsToAdd);
	
	/**
   	 * Removes a gameplay tag from this Mover Component
   	 * @param TagToRemove			Tag to remove from the Mover Component
   	 */
	UFUNCTION(BlueprintCallable, Category = Mover, meta = (Keywords = "Remove Tag"))
	BULLETMOVER_API void RemoveGameplayTag(FGameplayTag TagToRemove);

	/**
	 * Removes gameplay tags from this Mover Component
	 * @param TagsToRemove			Tags to remove from the Mover Component
	 */
	UFUNCTION(BlueprintCallable, Category = Mover, meta = (Keywords = "Remove Tag"))
	BULLETMOVER_API void RemoveGameplayTags(const FGameplayTagContainer& TagsToRemove);
	
protected:

	// Called before each simulation tick. Broadcasts OnPreSimulationTick delegate.
	void PreSimulationTick(const FBulletMoverTimeStep& TimeStep, const FBulletMoverInputCmdContext& InputCmd);
	
	/** Makes this component and owner actor reflect the state of a particular frame snapshot. This occurs after simulation ticking, as well as during a rollback before we resimulate forward.
	  @param bRebaseBasedState	If true and the state was using based movement, it will use the current game world base pos/rot instead of the captured one. This is necessary during rollbacks.
	*/
	BULLETMOVER_API void SetFrameStateFromContext(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState, bool bRebaseBasedState);
	BULLETMOVER_API void SetFrameStateFromContextFromNestedChild(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState, bool bRebaseBasedState);

	/** Update cached frame state if it has changed */
	BULLETMOVER_API void UpdateCachedFrameState(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState);

public:
	BULLETMOVER_API virtual void CreateDefaultInputAndState(FBulletMoverInputCmdContext& OutInputCmd, FBulletMoverSyncState& OutSyncState, FBulletMoverAuxStateContext& OutAuxState) const;

	/** Handle a blocking impact.*/
	UFUNCTION(BlueprintCallable, Category = Mover)
	BULLETMOVER_API void HandleImpact(FBulletMoverOnImpactParams& ImpactParams);

protected:
	BULLETMOVER_API void FindDefaultComponents();
	BULLETMOVER_API void FindDefaultUpdatedComponent();
	BULLETMOVER_API void UpdateTickRegistration();

	BULLETMOVER_API virtual void DoQueueNextMode(FName DesiredModeName, bool bShouldReenter=false);

	// Broadcast during the simulation tick after inputs have been processed, but before the actual move is performed.
	// Note - When async simulating, the delegate would be called on the async thread, and might be broadcast multiple times.
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnPreMovement OnPreMovement;

	/** Called when a rollback occurs, before the simulation state has been restored. NewBaseTimeStep represents the current time and frame we're about to simulate. */
	BULLETMOVER_API void OnSimulationPreRollback(const FBulletMoverSyncState* InvalidSyncState, const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* InvalidAuxState, const FBulletMoverAuxStateContext* AuxState, const FBulletMoverTimeStep& NewBaseTimeStep);
	
	/** Called when a rollback occurs, after the simulation state has been restored. NewBaseTimeStep represents the current time and frame we're about to simulate. */
	BULLETMOVER_API void OnSimulationRollback(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState, const FBulletMoverTimeStep& NewBaseTimeStep);

	BULLETMOVER_API void ProcessFirstSimTickAfterRollback(const FBulletMoverTimeStep& TimeStep);

	#if WITH_EDITOR
	BULLETMOVER_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	BULLETMOVER_API virtual void PostCDOCompiled(const FPostCDOCompiledContext& Context) override;
	BULLETMOVER_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	BULLETMOVER_API virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	BULLETMOVER_API bool ValidateSetup(class FDataValidationContext& ValidationErrors) const;
	BULLETMOVER_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	UFUNCTION()
	BULLETMOVER_API TArray<FString> GetStartingMovementModeNames();
	#endif // WITH_EDITOR

	UFUNCTION()
	BULLETMOVER_API virtual void PhysicsVolumeChanged(class APhysicsVolume* NewVolume);

	BULLETMOVER_API virtual void OnHandleImpact(const FBulletMoverOnImpactParams& ImpactParams);

	/** internal function to perform post-sim scheduling to optionally support simple based movement */
	BULLETMOVER_API void UpdateBasedMovementScheduling(const FBulletMoverTickEndData& SimOutput);

	BULLETMOVER_API UBulletBaseMovementMode* GetActiveModeInternal(TSubclassOf<UBulletBaseMovementMode> ModeType, bool bRequireExactClass = false) const;

	TObjectPtr<UPrimitiveComponent> MovementBaseDependency;	// used internally for based movement scheduling management
	
	/** internal function to ensure SharedSettings array matches what's needed by the list of Movement Modes */
	BULLETMOVER_API void RefreshSharedSettings();

	/** This is the component that's actually being moved. Typically it is the Actor's root component and often a collidable primitive. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> UpdatedComponent = nullptr;

	/** UpdatedComponent, cast as a UPrimitiveComponent. May be invalid if UpdatedComponent was null or not a UPrimitiveComponent. */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> UpdatedCompAsPrimitive = nullptr;
	
	/** BulletPhysicsComponent, must be a UPrimitiveComponent. Should not be invalid. 
	 * It is fetched automatically on BeginPlay where the first bullet primitive type in the hierarchy is used*/
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> BulletPhysicsComponent = nullptr;

	/** The main visual component associated with this Mover actor, typically a mesh and typically parented to the UpdatedComponent. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PrimaryVisualComponent;


	/** Cached original offset from the visual component, used for cases where we want to move the visual component away from the root component (for smoothing, corrections, etc.) */
	FTransform BaseVisualComponentTransform = FTransform::Identity;

	// TODO: Look at possibility of replacing this with a FGameplayTagCountContainer that could possibly represent both internal and external tags
	/** A list of gameplay tags associated with this Mover Component added from sources outside of Mover */
	FGameplayTagContainer ExternalGameplayTags;
	
	FBulletMoverInputCmdContext CachedLastProducedInputCmd;
	
	FBulletMoverInputCmdContext CachedLastUsedInputCmd;
	
	FBulletMoverDoubleBuffer<FBulletMoverSyncState> MoverSyncStateDoubleBuffer;
	
	const FBulletUpdatedMotionState* LastMoverDefaultSyncState = nullptr;

	FBulletMoverTimeStep CachedLastSimTickTimeStep;	// Saved timestep info from our last simulation tick, used during rollback handling. This will rewind during corrections.
	FBulletMoverTimeStep CachedNewestSimTickTimeStep;	// Saved timestep info from the newest (farthest-advanced) simulation tick. This will not rewind during corrections.

	UPROPERTY(Transient)
	TScriptInterface<IBulletMoverBackendLiaisonInterface> BackendLiaisonComp;

	/** Tick function that may be called anytime after this actor's movement step, useful as a way to support based movement on objects that are not */
	FBulletMoverDynamicBasedMovementTickFunction BasedMovementTickFunction;

	UPROPERTY(Transient)
	TObjectPtr<UBulletMovementModeStateMachine> ModeFSM;	// JAH TODO: Also consider allowing a type property on the component to allow an alternative machine implementation to be allocated/used

	/** Used to store cached data & computations between decoupled systems, that can be referenced by name */
	UPROPERTY(Transient)
	TObjectPtr<UBulletMoverBlackboard> SimBlackboard = nullptr;

	/** Used to store cached data & computations between decoupled systems, that can be referenced by name. Rollback-aware. */
	UPROPERTY(Transient)
	TObjectPtr<UBulletRollbackBlackboard> RollbackBlackboard = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBulletRollbackBlackboard_InternalWrapper> RollbackBlackboard_InternalWrapper = nullptr;	// This is a thin layer for use only by in-simulation users

	/**
	 * Layered moves registered on this component that can be activated regardless of the current mode
	 * Changes to this array or its contents occur ONLY during PreSimulationTick to ensure threadsafe access during async simulations.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBulletLayeredMoveLogic>> RegisteredMoves;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UBulletLayeredMoveLogic>> MovesPendingRegistration;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UBulletLayeredMoveLogic>> MovesPendingUnregistration;
	
	/** Helper function for making Queueing and making Layered Moves */
	bool MakeAndQueueLayeredMove(const TSubclassOf<UBulletLayeredMoveLogic>& MoveLogicClass, const FBulletLayeredMoveActivationParams* ActivationParams);
	

	
private:
	/** Collection of settings objects that are shared between movement modes. This list is automatically managed based on the @MovementModes contents. */
	UPROPERTY(EditDefaultsOnly, EditFixedSize, Instanced, Category = Mover, meta = (NoResetToDefault, ObjectMustImplement = "/Script/BulletMover.MovementSettingsInterface"))
	TArray<TObjectPtr<UObject>> SharedSettings;
	
	/** cm/s^2, only meaningful if @bHasGravityOverride is enabled.Set @SetGravityOverride */
	UPROPERTY(EditDefaultsOnly, Category="Bullet Mover|Gravity", meta=(ForceUnits = "cm/s^2"))
	FVector GravityAccelOverride;

	/** Settings that can lock movement to a particular plane */
	UPROPERTY(EditDefaultsOnly, Category = "Bullet Mover|Constraints")
	FBulletPlanarConstraint PlanarConstraint;

	/** Effects queued to be applied to the simulation at a given frame. If the frame happens to be in the past, the effect will be applied at the earliest occasion */
	TArray<FBulletScheduledInstantMovementEffect> QueuedInstantMovementEffects;

public:

	// If enabled, the movement of the primary visual component will be smoothed via an offset from the root moving component. This is useful in fixed-tick simulations with variable rendering rates.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BulletMover")
	EBulletMoverSmoothingMode SmoothingMode = EBulletMoverSmoothingMode::VisualComponentOffset;

	// Whether to warn when we detect that an external system has moved our object, outside of movement simulation control
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BulletMover", AdvancedDisplay)
	uint8 bWarnOnExternalMovement : 1 = 1;

	// If enabled, we'll accept any movements from an external system in the next simulation state update
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BulletMover", AdvancedDisplay)
	uint8 bAcceptExternalMovement : 1 = 0;

	// If enabled, we'll send inputs along with to sim proxy via the sync state, and they'll be available via GetLastInputCmd. This may be useful for cases where input is used to hint at object state, such as an anim graph. This option is intended to be temporary until all networking backends allow this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BulletMover", AdvancedDisplay, Experimental)
	uint8 bSyncInputsForSimProxy : 1 = 0;

	BULLETMOVER_API void SetSimulationOutput(const FBulletMoverTimeStep& TimeStep, const UE::BulletMover::FBulletSimulationOutputData& OutputData);

	// Dispatch a simulation event. It will be processed immediately.
	BULLETMOVER_API void DispatchSimulationEvent(const FBulletMoverSimulationEventData& EventData);

protected:
	BULLETMOVER_API virtual void ProcessSimulationEvent(const FBulletMoverSimulationEventData& EventData);
	BULLETMOVER_API virtual void SetAdditionalSimulationOutput(const FBulletMoverDataCollection& Data);
	BULLETMOVER_API virtual void CheckForExternalMovement(const FBulletMoverTickStartData& SimStartingData);

private:
	// Whether to override the up direction with a fixed value instead of using gravity to deduce it
	UPROPERTY(EditDefaultsOnly, Category="Bullet Mover|UpDirection")
	bool bHasUpDirectionOverride = false;

	// A fixed up direction to use if bHasUpDirectionOverride is true
	UPROPERTY(EditDefaultsOnly, Category="Bullet Mover|UpDirection", meta = (EditCondition = "bHasUpDirectionOverride"))
	FVector UpDirectionOverride = FVector::UpVector;

	/** Whether or not gravity is overridden on this actor. Otherwise, fall back on world settings. See @SetGravityOverride */
	UPROPERTY(EditDefaultsOnly, Category="Bullet Mover|Gravity")
	bool bHasGravityOverride = false;
	

	/**
     * If true, then the transform updates applied in UBulletMoverComponent::SetFrameStateFromContext will use a "deferred group move"
     * to improve performance.
     *
     * It is not recommended that you enable this when you need exact, high fidelity characters such as your player character.
     * This is mainly a benefit for scenarios with large amounts of NPCs or lower fidelity characters where it is acceptable
     * to not have immediately applied transforms.
     *
     * This only does something if the "s.GroupedComponentMovement.Enable" CVar is set to true.
     */
    UPROPERTY(EditDefaultsOnly, Category="BulletMover", meta = (EditCondition = "Engine.SceneComponent.IsGroupedComponentMovementEnabled"))
    bool bUseDeferredGroupMovement = false;

	/** Transient flag indicating whether we are executing OnRegister(). */
	bool bInOnRegister = false;

	/** Transient flag indicating whether we are executing InitializeComponent(). */
	bool bInInitializeComponent = false;

	// Transient flag indicating we've had a rollback and haven't started simulating forward again yet
	bool bHasRolledBack = false;

	/**
	 * A cached quaternion representing the rotation from world space to gravity relative space defined by GravityAccelOverride.
	 */
	UPROPERTY(Category="Bullet Mover|Gravity", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	FQuat WorldToGravityTransform;
	
	/**
	 * A cached quaternion representing the inverse rotation from world space to gravity relative space defined by GravityAccelOverride.
	 */
	UPROPERTY(Category="Bullet Mover|Gravity", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
	FQuat GravityToWorldTransform;
	
protected:

	/** If enabled, this actor will be moved to follow a base actor that it's standing on. Typically disabled for physics-based movement, which handles based movement internally. */
	UPROPERTY(EditDefaultsOnly, Category = "BulletMover")
	bool bSupportsKinematicBasedMovement = false;

	// Delay added to scheduled instant movement effects
	// This value is cached from the settings found in the network settings component
	float EventSchedulingMinDelaySeconds = 0.3f;

	FBulletMoverAuxStateContext CachedLastAuxState;

	friend class UBulletBaseMovementMode;
	friend class UBulletMoverDebugComponent;
	friend class UBulletBasedMovementUtils;
	
	
	
	
	
#pragma region BULLET PHYSICS
protected:
	
	

	
	
	virtual void InitializeWithBullet() {};
	
	virtual void BulletPreSimulationTick(const FBulletMoverTimeStep& InTimeStep, const FBulletMoverTickStartData& SimInput, FBulletMoverTickEndData& SimOutput) {};
	virtual void FinalizeStateFromBulletSimulation(FBulletMoverTickEndData& SimOutput) {};
	
public:
	virtual void SendFinalVelocityToBullet(const FBulletMoverTimeStep& InTimeStep, const FVector& LinearVelocity, const FVector& AngularVelocity) {}

#pragma endregion
};
