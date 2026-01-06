// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "MoveLibrary/BulletMovementUtilsTypes.h"
#include "BulletMovementModifier.generated.h"

#define UE_API BULLETMOVER_API

class UBulletMoverComponent;
class UBulletMoverSimulation;
struct FBulletMoverAuxStateContext;
struct FBulletMoverTimeStep;
struct FBulletMoverSyncState;
struct FBulletMoverInputCmdContext;

#define MODIFIER_HANDLE_TYPE uint16
#define MODIFIER_HANDLE_MAX UINT16_MAX
#define MODIFIER_INVALID_HANDLE 0

/**
 * Handle to identify movement modifiers.
 */
USTRUCT(BlueprintType)
struct FBulletMovementModifierHandle
{
	GENERATED_BODY()

	FBulletMovementModifierHandle()
		: Handle(0)
	{
	}

	FBulletMovementModifierHandle(MODIFIER_HANDLE_TYPE InHandle)
		: Handle(InHandle)
	{
	}

	/** Creates a new handle */
	UE_API void GenerateHandle();
	
	bool operator==(const FBulletMovementModifierHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FBulletMovementModifierHandle& Other) const
	{
		return Handle != Other.Handle;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d"), Handle);
	}

	bool IsValid() const
	{
		return Handle != MODIFIER_INVALID_HANDLE;
	}

	void Invalidate()
	{
		Handle = MODIFIER_INVALID_HANDLE;
	}
	
private:
	MODIFIER_HANDLE_TYPE Handle;
};

struct FBulletMovementModifierParams_Async
{
	FBulletMovementModifierParams_Async(UBulletMoverSimulation* InSimulation, const FBulletMoverSyncState* InSyncState, const FBulletMoverTimeStep* InTimeStep)
		: Simulation(InSimulation)
		, SyncState(InSyncState)
		, TimeStep(InTimeStep)
	{
		ensureMsgf(IsValid(), TEXT("Invalid initialization data for FBulletMovementModifierParams_Async"));
	}
	FBulletMovementModifierParams_Async() = delete;

	bool IsValid() const
	{
		return Simulation && SyncState && TimeStep;
	}

	UBulletMoverSimulation* Simulation = nullptr;
	const FBulletMoverSyncState* SyncState = nullptr;
	const FBulletMoverTimeStep* TimeStep = nullptr;
};

/**
 * Movement Modifier: Used to apply changes that indirectly influence the movement simulation,
 * without proposing or executing any movement, but still in sync with the sim.
 * Example usages: changing groups of settings, movement mode re-mappings, etc.
 * 
 * Note: Currently mover expects to only have one type of modifier active at a time.
 *		 This can be fixed by extending the Matches function to check more than just type,
 *		 but make sure anything used to compare is synced through the NetSerialize function.
 */
USTRUCT(BlueprintInternalUseOnly)
struct FBulletMovementModifierBase
{
	GENERATED_BODY()

	UE_API FBulletMovementModifierBase();
	virtual ~FBulletMovementModifierBase() {}
	
	// This modifier will expire after a set amount of time if > 0. If 0, it will be ticked only once, regardless of time step. It will need to be manually ended if < 0. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float DurationMs;

	// The simulation time this move first ticked (< 0 means it hasn't started yet)
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = Mover)
	double StartSimTimeMs;
	
	/** Fired when this modifier is activated. */
	virtual void OnStart(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState) {}
	
	/** Fired when this modifier is deactivated. */
	virtual void OnEnd(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState) {}
	
	/**
	 * Fired just before a movement Substep
	 */
	virtual void OnPreMovement(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep) {}

	/** Fired after a movement Substep */
	virtual void OnPostMovement(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState) {}

	/** Kicks off this modifier, allowing any initialization to occur. */
	UE_API void StartModifier(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState);

	/** Ends this move, allowing any cleanup to occur. */
	UE_API void EndModifier(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState);

	/** Runtime query whether this modifier is finished and can be destroyed. The default implementation is based on DurationMs. */
	UE_API virtual bool IsFinished(double CurrentSimTimeMs) const;
	

	// Begin Async API
	// The async API is used by the async friendly mover simulation class
	// which is currently implemented in ChaosMover for async physics support.
	// The standard Mover update does not currently require these async functions
	// to be implemented
	virtual void OnStart_Async(const FBulletMovementModifierParams_Async& Params) {}
	virtual void OnEnd_Async(const FBulletMovementModifierParams_Async& Params) {}
	virtual void OnPreMovement_Async(const FBulletMovementModifierParams_Async& Params) {}
	virtual void OnPostMovement_Async(const FBulletMovementModifierParams_Async& Params) {}

	UE_API void StartModifier_Async(const FBulletMovementModifierParams_Async& Params);
	UE_API void EndModifier_Async(const FBulletMovementModifierParams_Async& Params);
	// End Async API



	// @return newly allocated copy of this FBulletMovementModifier. Must be overridden by child classes
	UE_API virtual FBulletMovementModifierBase* Clone() const;

	UE_API virtual void NetSerialize(FArchive& Ar);

	UE_API virtual UScriptStruct* GetScriptStruct() const;

	UE_API virtual FString ToSimpleString() const;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) {}

	/** 
	 * Used to compare modifiers and check if they are the same instance of modifiers.
	 * Doesn't need to be overridden but more specific data to match is best
	 * Note: Current default implementation only checks type and nothing else.
	 */ 
	UE_API virtual bool Matches(const FBulletMovementModifierBase* Other) const;

	UE_API FBulletMovementModifierHandle GetHandle() const;

	UE_API void GenerateHandle();

	/**
	 * Used to write to a valid handle to a invalid handle
	 * Currently used to write a local handle to modifiers that were added from a rollback so they don't have a handle yet
	 * This is done to avoid queueing a modifier again as a local client applies potential input
	 */
	UE_API void OverwriteHandleIfInvalid(const FBulletMovementModifierHandle& ValidModifierHandle);

	/**
  	 * Check modifier for a gameplay tag.
  	 *
  	 * @param TagToFind			Tag to check on the Mover systems
  	 * @param bExactMatch		If true, the tag has to be exactly present, if false then TagToFind will include it's parent tags while matching
  	 * 
  	 * @return True if the TagToFind was found
  	 */
	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
	{
		return false;
	}

protected:
	/**
	 * Modifier handle local to this client or server instance. Used to cancel or query for a active/queued modifier.
	 */
	UPROPERTY()
	FBulletMovementModifierHandle LocalModifierHandle;
};

template<>
struct TStructOpsTypeTraits< FBulletMovementModifierBase > : public TStructOpsTypeTraitsBase2< FBulletMovementModifierBase >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};


// A collection of movement modifiers affecting a movable actor
USTRUCT(BlueprintType)
struct FBulletMovementModifierGroup
{
	GENERATED_BODY()

	FBulletMovementModifierGroup() {}
	
	bool HasAnyMoves() const { return (!ActiveModifiers.IsEmpty() || !QueuedModifiers.IsEmpty()); }

	/** Serialize all modifiers and their states for this group */
	UE_API void NetSerialize(FArchive& Ar, uint8 MaxNumModifiersToSerialize = MAX_uint8);
	
	UE_API void QueueMovementModifier(TSharedPtr<FBulletMovementModifierBase> Modifier);

	UE_API void CancelModifierFromHandle(const FBulletMovementModifierHandle& HandleToCancel);
	UE_API void CancelModifiersByTag(FGameplayTag Tag, bool bRequiresExactMatch=false);
	
	// Generates active modifier list (by calling FlushModifierArrays) and returns the an array of all currently active modifiers
	UE_API TArray<TSharedPtr<FBulletMovementModifierBase>> GenerateActiveModifiers(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState);
	
	UE_API TArray<TSharedPtr<FBulletMovementModifierBase>> GenerateActiveModifiers_Async(const FBulletMovementModifierParams_Async& Params);

	/** Copy operator - deep copy so it can be used for archiving/saving off moves */
	UE_API FBulletMovementModifierGroup& operator=(const FBulletMovementModifierGroup& Other);

	/** Comparison operator - needs matching modifier along with identical states in those structs */
	UE_API bool operator==(const FBulletMovementModifierGroup& Other) const;

	/** Comparison operator */
	UE_API bool operator!=(const FBulletMovementModifierGroup& Other) const;

	/** Checks only whether there are matching modifiers, but NOT necessarily identical states of each one */
	UE_API bool HasSameContents(const FBulletMovementModifierGroup& Other) const;

	/** Exposes references to GC system */
	UE_API void AddStructReferencedObjects(FReferenceCollector& Collector) const;

	/** Get a simplified string representation of this group. Typically for debugging. */
	UE_API FString ToSimpleString() const;

	/** Const access to active moves */
	UE_API TArray<TSharedPtr<FBulletMovementModifierBase>>::TConstIterator GetActiveModifiersIterator() const;
	
	/** Const access to queued moves */
	UE_API TArray<TSharedPtr<FBulletMovementModifierBase>>::TConstIterator GetQueuedModifiersIterator() const;

	// Clears active and queued modifiers
	UE_API void Reset();

	// Whether this modifier should reconcile with the authority state
	UE_API bool ShouldReconcile(const FBulletMovementModifierGroup& Other) const;
	
protected:
	/** Movement modifiers that are currently active in this group */
	TArray< TSharedPtr<FBulletMovementModifierBase> > ActiveModifiers;

	/** Movement modifiers that are queued to become active next sim frame */
	TArray< TSharedPtr<FBulletMovementModifierBase> > QueuedModifiers;
	
	// Clears out any finished or invalid modifiers and adds any queued modifiers to the active modifiers
	UE_API void FlushModifierArrays(UBulletMoverComponent* MoverComp, const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState);
	
	UE_API void FlushModifierArrays_Async(const FBulletMovementModifierParams_Async& Params);

	/** Helper function for serializing array of movement modifiers */
	static UE_API void NetSerializeMovementModifierArray(FArchive& Ar, TArray< TSharedPtr<FBulletMovementModifierBase> >& ModifierArray, uint8 MaxNumModifiersToSerialize = MAX_uint8);
	

};


template<>
struct TStructOpsTypeTraits<FBulletMovementModifierGroup> : public TStructOpsTypeTraitsBase2<FBulletMovementModifierGroup>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FBulletMovementModifierGroup> Data is copied around
		//WithNetSerializer = true,
		WithIdenticalViaEquality = true,
		WithAddStructReferencedObjects = true,
	};
};

#undef UE_API
