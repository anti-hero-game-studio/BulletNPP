// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <functional>

#include "Misc/StringBuilder.h"
#include "BulletNetworkPredictionReplicationProxy.h"
#include "Engine/NetSerialization.h"
#include "BulletMoverTypes.h"
#include "MoveLibrary/BulletMovementRecord.h"
#include "BulletLayeredMove.h"
#include "BulletLayeredMoveGroup.h"
#include "BulletMovementModifier.h"
#include "BulletMoverDataModelTypes.h"
#include "BulletInstantMovementEffect.h"
#include "UObject/Interface.h"

#include "BulletMoverSimulationTypes.generated.h"

// Names for our default modes
namespace DefaultModeNames
{
	const FName Walking = TEXT("Walking");
	const FName Falling = TEXT("Falling");
	const FName Flying  = TEXT("Flying");
	const FName Swimming  = TEXT("Swimming");
}

// Commonly-used blackboard object keys
namespace CommonBlackboard
{
	const FName LastFloorResult = TEXT("LastFloor");
	const FName LastWaterResult = TEXT("LastWater");
	const FName LastFoundDynamicMovementBase = TEXT("LastFoundDynamicMovementBase");
	const FName LastAppliedDynamicMovementBase = TEXT("LastAppliedDynamicMovementBase");
	const FName TimeSinceSupported = TEXT("TimeSinceSupported");

	const FName LastModeChangeRecord = TEXT("LastModeChangeRecord");
}


/**
 * Filled out by a MovementMode during simulation tick to indicate its ending state, allowing for a residual time step and switching modes mid-tick
 */
USTRUCT(BlueprintType)
struct FBulletMovementModeTickEndState
{
	GENERATED_BODY()
	
	FBulletMovementModeTickEndState() 
	{ 
		ResetToDefaults(); 
	}

	void ResetToDefaults()
	{
		RemainingMs = 0.f;
		NextModeName = NAME_None;
		bEndedWithNoChanges = false;
	}

	// Any unused tick time
	UPROPERTY(BlueprintReadWrite, Category=Mover)
	float RemainingMs;

	UPROPERTY(BlueprintReadWrite, Category=Mover)
	FName NextModeName = NAME_None;

	// Affirms that no state changes were made during this simulation tick. Can help optimizations if modes set this during sim tick.
	UPROPERTY(BlueprintReadWrite, Category=Mover)
	bool bEndedWithNoChanges = false;

};

USTRUCT()
struct FBulletScheduledInstantMovementEffect
{
	GENERATED_BODY()

	/** Turns a FBulletInstantMovementEffect into a scheduled one (FBulletScheduledInstantMovementEffect)
	*	The effect can be scheduled to apply immediately, or scheduled to apply with a delay
	*   This function should not be called on the game thread
	*   @param World The world, used to retrieve the current server frame in async mode, or the sim time otherwise
	*   @param TimeStep the time step of the current or upcoming tick
	*   @param InstantMovementEffect the effect to schedule
	*   @param SchedulingDelaySeconds Scheduling delay to ensure it applies on all end points on the same frame (this is only perfectly accurate when simulation dt is fixed)
	*/
	static FBulletScheduledInstantMovementEffect ScheduleEffect(UWorld* World, const FBulletMoverTimeStep& TimeStep, TSharedPtr<FBulletInstantMovementEffect> InstantMovementEffect, float SchedulingDelaySeconds = 0.0f);

	bool ShouldExecuteAtFrame(int32 CurrentServerFrame) const
	{
		ensureMsgf(bIsFixedDt, TEXT("In variable delta time mode, use the version of ShouldExecute that takes a floating point time"));
		return (CurrentServerFrame >= ExecutionServerFrame);
	}

	bool ShouldExecuteAtTime(double CurrentServerTime) const
	{
		ensureMsgf(!bIsFixedDt, TEXT("In fixed delta time mode, use the version of ShouldExecute that takes a frame number"));
		return (CurrentServerTime >= ExecutionServerTimeSeconds);
	}

	void NetSerialize(const FBulletNetSerializeParams& P)
	{
		P.Ar.SerializeBits(&bIsFixedDt, 1);
		if (bIsFixedDt)
		{
			P.Ar << ExecutionServerFrame;
		}
		else
		{
			P.Ar << ExecutionServerTimeSeconds;
		}
		
		Effect->NetSerialize(P.Ar);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		if (bIsFixedDt)
		{
			Out.Appendf("ExecutionServerFrame: %d", ExecutionServerFrame);
		}
		else
		{
			Out.Appendf("ExecutionDateSeconds: %f", ExecutionServerTimeSeconds);
		}

		Out.Appendf(" | Effect = % s", *(Effect.IsValid() ? Effect->ToSimpleString() : "Invalid"));
	}

	// Server frame at which this instant movement effect should be applied
	// Only valid if bIsFixedDt is true, i.e. in fixed time step mode
	UPROPERTY(VisibleAnywhere, Category = "BulletMover")
	int32 ExecutionServerFrame = INDEX_NONE;

	// Server Time (in seconds) after which this instant movement effect should be applied
	// Only valid if bIsFixedDt is false, i.e. in variable time step mode
	UPROPERTY(VisibleAnywhere, Category = "BulletMover")
	double ExecutionServerTimeSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "BulletMover")
	bool bIsFixedDt = true;

	TSharedPtr<FBulletInstantMovementEffect> Effect;
};

/**
 * The client generates this representation of "input" to the simulated actor for one simulation frame. This can be direct mapping
 * of controls, or more abstract data. It is composed of a collection of typed structs that can be customized per project.
 */
USTRUCT(BlueprintType)
struct FBulletMoverInputCmdContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FBulletMoverDataCollection Collection;

	UScriptStruct* GetStruct() const
	{
		return StaticStruct();
	}

	void NetSerialize(const FBulletNetSerializeParams& P)
	{
		bool bIgnoredResult(false);
		Collection.NetSerialize(P.Ar, P.Map, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Collection.ToString(Out);
	}

	void Interpolate(const FBulletMoverInputCmdContext* From, const FBulletMoverInputCmdContext* To, float Pct)
	{
		Collection.Interpolate(From->Collection, To->Collection, Pct);
	}

	void Reset()
	{
		Collection.Empty();
	}
};


/** State we are evolving frame to frame and keeping in sync (frequently changing). It is composed of a collection of typed structs 
 *  that can be customized per project. Mover actors are required to have FBulletUpdatedMotionState as one of these structs.
 */
USTRUCT(BlueprintType)
struct FBulletMoverSyncState
{
	GENERATED_BODY()

public:

	// The mode we ended up in from the prior frame, and which we'll start in during the next frame
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Mover)
	FName MovementMode;

	// Additional moves influencing our proposed motion
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Mover)
	FBulletLayeredMoveGroup LayeredMoves;

	// Additional moves influencing our proposed motion
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Mover)
	FBulletLayeredMoveInstanceGroup LayeredMoveInstances;

	// Additional modifiers influencing our simulation
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Mover)
	FBulletMovementModifierGroup MovementModifiers;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FBulletMoverDataCollection Collection;

	FBulletMoverSyncState()
	{
		MovementMode = NAME_None;
	}

	bool HasSameContents(const FBulletMoverSyncState& Other) const
	{
		return MovementMode == Other.MovementMode &&
			LayeredMoves.HasSameContents(Other.LayeredMoves) &&
			LayeredMoveInstances.HasSameContents(Other.LayeredMoveInstances) &&
			MovementModifiers.HasSameContents(Other.MovementModifiers) &&
			Collection.HasSameContents(Other.Collection);
	}

	UScriptStruct* GetStruct() const { return StaticStruct(); }


	void NetSerialize(const FBulletNetSerializeParams& P)
	{
		P.Ar << MovementMode;
		LayeredMoves.NetSerialize(P.Ar);
		LayeredMoveInstances.NetSerialize(P.Ar);
		MovementModifiers.NetSerialize(P.Ar);

		bool bIgnoredResult(false);
		Collection.NetSerialize(P.Ar, P.Map, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("BulletMovementMode: %s\n", TCHAR_TO_ANSI(*MovementMode.ToString()));
		Out.Appendf("Layered Moves: %s\n", TCHAR_TO_ANSI(*LayeredMoves.ToSimpleString()));
		Out.Appendf("Layered Moves: %s\n", TCHAR_TO_ANSI(*LayeredMoveInstances.ToSimpleString()));
		Out.Appendf("Movement Modifiers: %s\n", TCHAR_TO_ANSI(*MovementModifiers.ToSimpleString()));
		Collection.ToString(Out);
	}

	bool ShouldReconcile(const FBulletMoverSyncState& AuthorityState) const
	{
		return (MovementMode != AuthorityState.MovementMode) || 
			   Collection.ShouldReconcile(AuthorityState.Collection) ||
			   MovementModifiers.ShouldReconcile(AuthorityState.MovementModifiers);
	}

	void Interpolate(const FBulletMoverSyncState* From, const FBulletMoverSyncState* To, float Pct)
	{
		MovementMode = To->MovementMode;
		LayeredMoves = To->LayeredMoves;
		LayeredMoveInstances = To->LayeredMoveInstances;
		MovementModifiers = To->MovementModifiers;

		Collection.Interpolate(From->Collection, To->Collection, Pct);
	}

	// Resets the sync state to its default configuration and removes any
	// active or queued layered modes and modifiers
	void Reset()
	{
		MovementMode = NAME_None;
		Collection.Empty();
		LayeredMoves.Reset();
		LayeredMoveInstances.Reset();
		MovementModifiers.Reset();
	}
};

/** 
 *  Double Buffer struct for various Mover data. 
 */
template<typename T>
struct FBulletMoverDoubleBuffer
{
	// Sets all buffered data - usually used for initializing data
	void SetBufferedData(const T& InDataToCopy)
	{
		Buffer[0] = InDataToCopy;
		Buffer[1] = InDataToCopy;
	}
	
	// Gets data that is safe to read and is not being written to
	const T& GetReadable() const
	{
		return Buffer[ReadIndex];
	}

	// Gets data that is being written to and is expected to change
	T& GetWritable()
	{
		return Buffer[(ReadIndex + 1) % 2];
	}

	// Flips which data in the buffer we return for reading and writing
	void Flip()
	{
		ReadIndex = (ReadIndex + 1) % 2;
	}
	
private:
	uint32 ReadIndex = 0;
	T Buffer[2];
};

// Auxiliary state that is input into the simulation (changes rarely)
USTRUCT(BlueprintType)
struct FBulletMoverAuxStateContext
{
	GENERATED_BODY()

public:
	UScriptStruct* GetStruct() const { return StaticStruct(); }

	bool ShouldReconcile(const FBulletMoverAuxStateContext& AuthorityState) const
	{ 
		return Collection.ShouldReconcile(AuthorityState.Collection); 
	}

	void NetSerialize(const FBulletNetSerializeParams& P)
	{
		bool bIgnoredResult(false);
		Collection.NetSerialize(P.Ar, P.Map, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Collection.ToString(Out);
	}

	void Interpolate(const FBulletMoverAuxStateContext* From, const FBulletMoverAuxStateContext* To, float Pct)
	{
		Collection.Interpolate(From->Collection, To->Collection, Pct);
	}

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FBulletMoverDataCollection Collection;
};


/**
 * Contains all state data for the start of a simulation tick
 */
USTRUCT(BlueprintType)
struct FBulletMoverTickStartData
{
	GENERATED_BODY()

	FBulletMoverTickStartData() {}
	FBulletMoverTickStartData(
			const FBulletMoverInputCmdContext& InInputCmd,
			const FBulletMoverSyncState& InSyncState,
			const FBulletMoverAuxStateContext& InAuxState)
		:  InputCmd(InInputCmd), SyncState(InSyncState), AuxState(InAuxState)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FBulletMoverInputCmdContext InputCmd;
	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FBulletMoverSyncState SyncState;
	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FBulletMoverAuxStateContext AuxState;
};

/**
 * Contains all state data produced by a simulation tick, including new simulation state
 */
USTRUCT(BlueprintType)
struct FBulletMoverTickEndData
{
	GENERATED_BODY()

	FBulletMoverTickEndData() {}
	FBulletMoverTickEndData(
		const FBulletMoverSyncState* SyncState,
		const FBulletMoverAuxStateContext* AuxState)
	{
		this->SyncState = *SyncState;
		this->AuxState = *AuxState;
	}

	void InitForNewFrame()
	{
		MovementEndState.ResetToDefaults();
		MoveRecord.Reset();
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FBulletMoverSyncState SyncState;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FBulletMoverAuxStateContext AuxState;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FBulletMovementModeTickEndState MovementEndState;

	FBulletMovementRecord MoveRecord;
};

// Input parameters to provide context for SimulationTick functions
USTRUCT(BlueprintType)
struct FBulletSimulationTickParams
{
	GENERATED_BODY()

	// Components involved in movement by the simulation
	// This will be empty when the simulation is ticked asynchronously
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FBulletMovingComponentSet MovingComps;

	// Blackboard
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	TObjectPtr<UBulletMoverBlackboard> SimBlackboard;

	// Simulation state data at the start of the tick, including Input Cmd
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FBulletMoverTickStartData StartState;

	// Time and frame information for this tick
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FBulletMoverTimeStep TimeStep;

	// Proposed movement for this tick
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Mover)
	FBulletProposedMove ProposedMove;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UBulletMoverInputProducerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * MoverInputProducerInterface: API for any object that can produce input for a Mover simulation frame
 */
class IBulletMoverInputProducerInterface : public IInterface
{
	GENERATED_BODY()

public:
	/** Contributes additions to the input cmd for this simulation frame. Typically this is translating accumulated user input (or AI state) into parameters that affect movement. */
	UFUNCTION(BlueprintNativeEvent)
	BULLETMOVER_API void ProduceInput(int32 SimTimeMs, FBulletMoverInputCmdContext& InputCmdResult);
};


/** 
 * FBulletMoverPredictTrajectoryParams: parameter block for querying future trajectory samples based on a starting state
 * See UBulletMoverComponent::GetPredictedTrajectory
 */
USTRUCT(BlueprintType)
struct FBulletMoverPredictTrajectoryParams
{
	GENERATED_BODY()

	/** How many samples to predict into the future, including the first sample, which is always a snapshot of the
	 *  starting state with 0 accumulated time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = 1))
	int32 NumPredictionSamples = 1;

	/* How much time between predicted samples */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover, meta = (ClampMin = 0.00001))
	float SecondsPerSample = 0.333f;

	/** If true, samples are based on the visual component transform, rather than the 'updated' movement root.
	 *  Typically, this is a mesh with its component location at the bottom of the collision primitive.
	 *  If false, samples are from the movement root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bUseVisualComponentRoot = false;

	/** If true, gravity will not taken into account during prediction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bDisableGravity = false;

 	/** Optional starting sync state. If not set, prediction will begin from the current state. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
 	TOptional<FBulletMoverSyncState> OptionalStartSyncState;
 
 	/** Optional starting aux state. If not set, prediction will begin from the current state. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
 	TOptional<FBulletMoverAuxStateContext> OptionalStartAuxState;

 	/** Optional input cmds to use, one per sample. If none are specified, prediction will begin with last-used inputs. 
 	 *  If too few are specified for the number of samples, the final input in the array will be used repeatedly to cover remaining samples. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
 	TArray<FBulletMoverInputCmdContext> OptionalInputCmds;

};

USTRUCT()
struct FBulletMoverSimEventGameThreadContext
{
	GENERATED_BODY()

public:
	UBulletMoverComponent* MoverComp = nullptr;
};

USTRUCT()
struct FBulletMoverSimulationEventData
{
	GENERATED_BODY()

	using FBulletEventProcessedCallbackPtr = std::function<void(const FBulletMoverSimulationEventData& Data, const FBulletMoverSimEventGameThreadContext& GameThreadContext)>;

	FBulletMoverSimulationEventData(double InEventTimeMs, FBulletEventProcessedCallbackPtr InEventProcessedCallback = nullptr)
		: EventProcessedCallback(InEventProcessedCallback)
		, EventTimeMs(InEventTimeMs)
	{
	}
	FBulletMoverSimulationEventData() {}
	virtual ~FBulletMoverSimulationEventData() {}

	// User must override
	BULLETMOVER_API virtual UScriptStruct* GetScriptStruct() const;

	template<typename T>
	T* CastTo_Mutable()
	{
		return T::StaticStruct() == GetScriptStruct() ? static_cast<T*>(this) : nullptr;
	}

	template<typename T>
	const T* CastTo() const
	{
		return const_cast<const T*>(const_cast<FBulletMoverSimulationEventData*>(this)->CastTo_Mutable<T>());
	}

	void OnEventProcessed(const FBulletMoverSimEventGameThreadContext& GameThreadContext) const
	{
		if (EventProcessedCallback)
		{
			EventProcessedCallback(*this, GameThreadContext);
		}
	}

	void SetEventProcessedCallback(FBulletEventProcessedCallbackPtr Callback)
	{
		EventProcessedCallback = Callback;
	}

private:
	// This callback is fired when the event is processed on the game thread
	// This is called before and in addition to any type based handling
	FBulletEventProcessedCallbackPtr EventProcessedCallback = nullptr;

public:
	double EventTimeMs = 0.0;
};

USTRUCT()
struct FBulletMovementModeChangedEventData : public FBulletMoverSimulationEventData
{
	GENERATED_BODY()

	FBulletMovementModeChangedEventData(float InEventTimeMs, const FName InPreviousModeName, const FName InNewModeName, FBulletEventProcessedCallbackPtr InEventProcessedCallback = nullptr)
		: FBulletMoverSimulationEventData(InEventTimeMs, InEventProcessedCallback)
		, PreviousModeName(InPreviousModeName)
		, NewModeName(InNewModeName)
	{
	}
	FBulletMovementModeChangedEventData() {}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FBulletMovementModeChangedEventData::StaticStruct();
	}

	FName PreviousModeName = NAME_None;
	FName NewModeName = NAME_None;
};

USTRUCT()
struct FBulletTeleportSucceededEventData : public FBulletMoverSimulationEventData
{
	GENERATED_BODY()

	FBulletTeleportSucceededEventData(float InEventTimeMs, const FVector& InFromLocation, const FQuat& InFromRotation, const FVector& InToLocation, const FQuat& InToRotation)
		: FBulletMoverSimulationEventData(InEventTimeMs)
		, FromLocation(InFromLocation)
		, FromRotation(InFromRotation)
		, ToLocation(InToLocation)
		, ToRotation(InToRotation)
	{
	}
	FBulletTeleportSucceededEventData() {}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FBulletTeleportSucceededEventData::StaticStruct();
	}

	FVector FromLocation;
	FQuat FromRotation;
	FVector ToLocation;
	FQuat ToRotation;
};

UENUM(BlueprintType)
enum class ETeleportFailureReason : uint8
{
	Reason_NotAvailable UMETA(DisplayName = "Reason Not Available", Tooltip = "A reason for the teleport failure was not indicated"),
};

USTRUCT()
struct FBulletTeleportFailedEventData : public FBulletMoverSimulationEventData
{
	GENERATED_BODY()

	FBulletTeleportFailedEventData(float InEventTimeMs, const FVector& InFromLocation, const FQuat& InFromRotation, const FVector& InToLocation, const FQuat& InToRotation, ETeleportFailureReason InTeleportFailureReason)
		: FBulletMoverSimulationEventData(InEventTimeMs)
		, FromLocation(InFromLocation)
		, FromRotation(InFromRotation)
		, ToLocation(InToLocation)
		, ToRotation(InToRotation)
		, TeleportFailureReason(InTeleportFailureReason)
	{
	}
	FBulletTeleportFailedEventData() {}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FBulletTeleportFailedEventData::StaticStruct();
	}

	FVector FromLocation;
	FQuat FromRotation;
	FVector ToLocation;
	FQuat ToRotation;
	ETeleportFailureReason TeleportFailureReason;
};

namespace UE::BulletMover
{
	struct FBulletSimulationOutputData
	{
		BULLETMOVER_API void Reset();
		BULLETMOVER_API void Interpolate(const FBulletSimulationOutputData& From, const FBulletSimulationOutputData& To, float Alpha, double SimTimeMs);

		FBulletMoverSyncState SyncState;
		FBulletMoverInputCmdContext LastUsedInputCmd;
		FBulletMoverDataCollection AdditionalOutputData;
		TArray<TSharedPtr<FBulletMoverSimulationEventData>> Events;
	};

	class FBulletSimulationOutputRecord
	{
	public:
		struct FData
		{
			BULLETMOVER_API void Reset();

			FBulletMoverTimeStep TimeStep;
			FBulletSimulationOutputData SimOutputData;
		};

		BULLETMOVER_API void Add(const FBulletMoverTimeStep& InTimeStep, const FBulletSimulationOutputData& InData);

		BULLETMOVER_API const FBulletSimulationOutputData& GetLatest() const;

		/** This will create an interpolated output and extract events from the stored data with time stamps up until the input time */
		BULLETMOVER_API void CreateInterpolatedResult(double AtBaseTimeMs, FBulletMoverTimeStep& OutTimeStep, FBulletSimulationOutputData& OutData);

		BULLETMOVER_API void Clear();

	private:
		FData Data[2];
		TArray<TSharedPtr<FBulletMoverSimulationEventData>> Events;
		uint8 CurrentIndex = 1;
	};

} // namespace UE::BulletMover
