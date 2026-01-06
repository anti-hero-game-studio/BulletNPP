// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/TransactionallySafeRWLock.h"
#include "Templates/SubclassOf.h"
#include "BulletMovementMode.h"
#include "BulletInstantMovementEffect.h"
#include "BulletMovementModeStateMachine.generated.h"

#define UE_API BULLETMOVER_API

struct FBulletProposedMove;
class UBulletImmediateMovementModeTransition;
class UBulletMovementModeTransition;

/**
 * - Any movement modes registered are co-owned by the state machine
 * - There is always an active mode, falling back to a do-nothing 'null' mode
 * - Queuing a mode that is already active will cause it to exit and re-enter
 * - Modes only switch during simulation tick
 */
 UCLASS(MinimalAPI)
class UBulletMovementModeStateMachine : public UObject
{
	 GENERATED_UCLASS_BODY()

public:
	UE_API void RegisterMovementMode(FName ModeName, TObjectPtr<UBulletBaseMovementMode> Mode, bool bIsDefaultMode=false);
	UE_API void RegisterMovementMode(FName ModeName, TSubclassOf<UBulletBaseMovementMode> ModeType, bool bIsDefaultMode=false);

	UE_API void UnregisterMovementMode(FName ModeName);
	UE_API void ClearAllMovementModes();

	UE_API void RegisterGlobalTransition(TObjectPtr<UBulletBaseMovementModeTransition> Transition);
	UE_API void UnregisterGlobalTransition(TObjectPtr<UBulletBaseMovementModeTransition> Transition);
	UE_API void ClearAllGlobalTransitions();

	UE_API void SetDefaultMode(FName NewDefaultModeName);

	UE_API void QueueNextMode(FName DesiredNextModeName, bool bShouldReenter=false);
	UE_API void SetModeImmediately(FName DesiredModeName, bool bShouldReenter=false);
	UE_API void ClearQueuedMode();

	UE_API void OnSimulationTick(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UBulletMoverBlackboard* SimBlackboard, const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletMoverTickEndData& OutputState);
 	UE_API void OnSimulationPreRollback(const FBulletMoverSyncState* InvalidSyncState, const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* InvalidAuxState, const FBulletMoverAuxStateContext* AuxState, const FBulletMoverTimeStep& NewBaseTimeStep);
	UE_API void OnSimulationRollback(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState, const FBulletMoverTimeStep& NewBaseTimeStep);

	FName GetCurrentModeName() const { return CurrentModeName; }

	UE_API const UBulletBaseMovementMode* GetCurrentMode() const;

	UE_API const UBulletBaseMovementMode* FindMovementMode(FName ModeName) const;

	UE_API void QueueLayeredMove(TSharedPtr<FBulletLayeredMoveBase> Move);
	
	UE_API void QueueActiveLayeredMove(const TSharedPtr<FBulletLayeredMoveInstance>& LayeredMove);

 	UE_API FBulletMovementModifierHandle QueueMovementModifier(TSharedPtr<FBulletMovementModifierBase> Modifier);

 	UE_API void CancelModifierFromHandle(FBulletMovementModifierHandle ModifierHandle);

 	UE_API const FBulletMovementModifierBase* FindQueuedModifier(FBulletMovementModifierHandle ModifierHandle) const;
 	UE_API const FBulletMovementModifierBase* FindQueuedModifierByType(const UScriptStruct* ModifierType) const;

	UE_API void CancelFeaturesWithTag(FGameplayTag TagToCancel, bool bRequireExactMatch=false);

	// This function is meant to be used only in async mode on the physics thread, not on the game thread
	void QueueInstantMovementEffect_Internal(const FBulletScheduledInstantMovementEffect& ScheduledInstantMovementEffect);
protected:
	UE_API void QueueInstantMovementEffect(const FBulletScheduledInstantMovementEffect& ScheduledInstantMovementEffect);
	UE_API void QueueInstantMovementEffects(const TArray<FBulletScheduledInstantMovementEffect>& ScheduledInstantMovementEffects);

	void ProcessEvents(const TArray<TSharedPtr<FBulletMoverSimulationEventData>>& InEvents);
	UE_API virtual void ProcessSimulationEvent(const FBulletMoverSimulationEventData& EventData);

	UE_API virtual void PostInitProperties() override;

	UPROPERTY()
	TMap<FName, TObjectPtr<UBulletBaseMovementMode>> Modes;
	TArray<TObjectPtr<UBulletBaseMovementModeTransition>> GlobalTransitions;

	UPROPERTY(Transient)
	TObjectPtr<UBulletImmediateMovementModeTransition> QueuedModeTransition;

	FName DefaultModeName = NAME_None;
	FName CurrentModeName = NAME_None;

	// Represents the current sim time that's passed, and the next frame number that's next to be simulated.
	FBulletMoverTimeStep CurrentBaseTimeStep;

	/** Moves that are queued to be added to the simulation at the start of the next sim subtick. Access covered by lock. */
	TArray<TSharedPtr<FBulletLayeredMoveBase>> QueuedLayeredMoves;

	/** Moves that are queued to be added to the simulation at the start of the next sim subtick. Access covered by lock. */
 	TArray<TSharedPtr<FBulletLayeredMoveInstance>> QueuedLayeredMoveInstances;
 	
 	/** Effects that are queued to be applied to the simulation at the start of the next sim subtick or at the end of this tick.  Access covered by lock. */
 	TArray<FBulletScheduledInstantMovementEffect> QueuedInstantEffects;

 	/** Modifiers that are queued to be added to the simulation at the start of the next sim subtick. Access covered by lock. */
 	TArray<TSharedPtr<FBulletMovementModifierBase>> QueuedMovementModifiers;

 	/** Modifiers that are to be canceled at the start of the next sim subtick.  Access covered by lock. */
 	TArray<FBulletMovementModifierHandle> ModifiersToCancel;
 	
	/** Tags that are used to cancel any matching movement features (modifiers, layered moves, etc). Access covered by lock. */
	TArray<TPair<FGameplayTag, bool>> TagCancellationRequests;

	// Internal-use-only tick data structs, for efficiency since they typically have the same contents from frame to frame
	FBulletMoverTickStartData WorkingSubstepStartData;
	FBulletSimulationTickParams WorkingSimTickParams;

private:
	// Locks for thread safety on queueing mechanisms
	mutable FTransactionallySafeRWLock LayeredMoveQueueLock;
	mutable FTransactionallySafeRWLock InstantEffectsQueueLock;
	mutable FTransactionallySafeRWLock ModifiersQueueLock;
	mutable FTransactionallySafeRWLock ModifierCancelQueueLock;
	mutable FTransactionallySafeRWLock TagCancellationRequestsLock;

	UE_API void ConstructDefaultModes();
	UE_API void AdvanceToNextMode();
	UE_API void FlushQueuedMovesToGroup(FBulletLayeredMoveGroup& Group);
	// Flushes queued ActiveLayeredMoves to FBulletLayeredMoveInstanceGroup for this frame
 	UE_API void ActivateQueuedMoves(FBulletLayeredMoveInstanceGroup& Group);
 	UE_API void FlushQueuedModifiersToGroup(FBulletMovementModifierGroup& ModifierGroup);
 	UE_API void FlushModifierCancellationsToGroup(FBulletMovementModifierGroup& ActiveModifierGroup);
	UE_API void FlushTagCancellationsToSyncState(FBulletMoverSyncState& SyncState);
 	UE_API void RollbackModifiers(const FBulletMoverSyncState* InvalidSyncState, const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* InvalidAuxState, const FBulletMoverAuxStateContext* AuxState);
	UE_API bool HasAnyInstantEffectsQueued() const;
 	UE_API bool ApplyInstantEffects(FBulletApplyMovementEffectParams& ApplyEffectParams, FBulletMoverSyncState& OutputState);
	UE_API AActor* GetOwnerActor() const;
};

#undef UE_API
