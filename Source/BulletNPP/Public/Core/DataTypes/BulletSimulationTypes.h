// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletLayeredMove.h"
#include "BulletMovementRecord.h"
#include "BulletMovementUtilTypes.h"
#include "BulletPhysicsTypes.h"
#include "NetworkPredictionReplicationProxy.h"
#include "BulletSimulationTypes.generated.h"

// TODO:@GreggoryAddison::CodeCompletion | Add FMovementModifiers & Potentially add FLayeredMoveInstance for BP support (yuck)

class UBulletBlackboard;

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
	UPROPERTY(BlueprintReadWrite, Category=Bullet)
	float RemainingMs;

	UPROPERTY(BlueprintReadWrite, Category=Bullet)
	FName NextModeName = NAME_None;

	// Affirms that no state changes were made during this simulation tick. Can help optimizations if modes set this during sim tick.
	UPROPERTY(BlueprintReadWrite, Category=Bullet)
	bool bEndedWithNoChanges = false;
};

/**
 * The client generates this representation of "input" to the simulated actor for one simulation frame. This can be direct mapping
 * of controls, or more abstract data. It is composed of a collection of typed structs that can be customized per project.
 */
USTRUCT(BlueprintType)
struct FBulletInputCmdContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Bullet)
	FBulletDataCollection DataCollection;

	UScriptStruct* GetStruct() const
	{
		return StaticStruct();
	}

	void NetSerialize(const FNetSerializeParams& P)
	{
		bool bIgnoredResult(false);
		DataCollection.NetSerialize(P.Ar, P.Map, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		DataCollection.ToString(Out);
	}

	void Interpolate(const FBulletInputCmdContext* From, const FBulletInputCmdContext* To, float Pct)
	{
		DataCollection.Interpolate(From->DataCollection, To->DataCollection, Pct);
	}

	void Reset()
	{
		DataCollection.Empty();
	}
};


/** State we are evolving frame to frame and keeping in sync (frequently changing). It is composed of a collection of typed structs 
 *  that can be customized per project. Bullet actors are required to have FBulletDefaultSyncState as one of these structs.
 */
USTRUCT(BlueprintType)
struct FBulletSyncState
{
	GENERATED_BODY()

public:

	// The mode we ended up in from the prior frame, and which we'll start in during the next frame
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Bullet)
	FName MovementMode;

	// Additional moves influencing our proposed motion
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Bullet)
	FBulletLayeredMoveGroup LayeredMoves;

	// Additional moves influencing our proposed motion
	/*UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Bullet)
	FBulletLayeredMoveInstanceGroup LayeredMoveInstances;*/

	// Additional modifiers influencing our simulation
	/*UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = Bullet)
	FBulletMovementModifierGroup MovementModifiers;*/

	UPROPERTY(BlueprintReadWrite, Category = Bullet)
	FBulletDataCollection DataCollection;

	FBulletSyncState()
	{
		MovementMode = NAME_None;
	}

	bool HasSameContents(const FBulletSyncState& Other) const
	{
		return MovementMode == Other.MovementMode &&
			LayeredMoves.HasSameContents(Other.LayeredMoves) &&
			//LayeredMoveInstances.HasSameContents(Other.LayeredMoveInstances) &&
			//MovementModifiers.HasSameContents(Other.MovementModifiers) &&
			DataCollection.HasSameContents(Other.DataCollection);
	}

	UScriptStruct* GetStruct() const { return StaticStruct(); }


	void NetSerialize(const FNetSerializeParams& P)
	{
		P.Ar << MovementMode;
		LayeredMoves.NetSerialize(P.Ar);
		//LayeredMoveInstances.NetSerialize(P.Ar);
		//MovementModifiers.NetSerialize(P.Ar);

		bool bIgnoredResult(false);
		DataCollection.NetSerialize(P.Ar, P.Map, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("MovementMode: %s\n", TCHAR_TO_ANSI(*MovementMode.ToString()));
		Out.Appendf("Layered Moves: %s\n", TCHAR_TO_ANSI(*LayeredMoves.ToSimpleString()));
		//Out.Appendf("Layered Moves: %s\n", TCHAR_TO_ANSI(*LayeredMoveInstances.ToSimpleString()));
		//Out.Appendf("Movement Modifiers: %s\n", TCHAR_TO_ANSI(*MovementModifiers.ToSimpleString()));
		DataCollection.ToString(Out);
	}

	bool ShouldReconcile(const FBulletSyncState& AuthorityState) const
	{
		return (MovementMode != AuthorityState.MovementMode) || 
			   DataCollection.ShouldReconcile(AuthorityState.DataCollection) /*||
			   MovementModifiers.ShouldReconcile(AuthorityState.MovementModifiers)*/;
	}

	void Interpolate(const FBulletSyncState* From, const FBulletSyncState* To, float Pct)
	{
		MovementMode = To->MovementMode;
		LayeredMoves = To->LayeredMoves;
		//LayeredMoveInstances = To->LayeredMoveInstances;
		//MovementModifiers = To->MovementModifiers;

		DataCollection.Interpolate(From->DataCollection, To->DataCollection, Pct);
	}

	// Resets the sync state to its default configuration and removes any
	// active or queued layered modes and modifiers
	void Reset()
	{
		MovementMode = NAME_None;
		DataCollection.Empty();
		LayeredMoves.Reset();
		//LayeredMoveInstances.Reset();
		//MovementModifiers.Reset();
	}
};

/** 
 *  Double Buffer struct for various Bullet data. 
 */
template<typename T>
struct FBulletDoubleBuffer
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
struct FBulletAuxStateContext
{
	GENERATED_BODY()

public:
	UScriptStruct* GetStruct() const { return StaticStruct(); }

	bool ShouldReconcile(const FBulletAuxStateContext& AuthorityState) const
	{ 
		return DataCollection.ShouldReconcile(AuthorityState.DataCollection); 
	}

	void NetSerialize(const FNetSerializeParams& P)
	{
		bool bIgnoredResult(false);
		DataCollection.NetSerialize(P.Ar, P.Map, bIgnoredResult);
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
		DataCollection.ToString(Out);
	}

	void Interpolate(const FBulletAuxStateContext* From, const FBulletAuxStateContext* To, float Pct)
	{
		DataCollection.Interpolate(From->DataCollection, To->DataCollection, Pct);
	}

	UPROPERTY(BlueprintReadWrite, Category = Bullet)
	FBulletDataCollection DataCollection;
};


/**
 * Contains all state data for the start of a simulation tick
 */
USTRUCT(BlueprintType)
struct FBulletTickStartData
{
	GENERATED_BODY()

	FBulletTickStartData() {}
	FBulletTickStartData(
			const FBulletInputCmdContext& InInputCmd,
			const FBulletSyncState& InSyncState,
			const FBulletAuxStateContext& InAuxState)
		:  InputCmd(InInputCmd), SyncState(InSyncState), AuxState(InAuxState)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category=Bullet)
	FBulletInputCmdContext InputCmd;
	UPROPERTY(BlueprintReadOnly, Category=Bullet)
	FBulletSyncState SyncState;
	UPROPERTY(BlueprintReadOnly, Category=Bullet)
	FBulletAuxStateContext AuxState;
};

/**
 * Contains all state data produced by a simulation tick, including new simulation state
 */
USTRUCT(BlueprintType)
struct FBulletTickEndData
{
	GENERATED_BODY()

	FBulletTickEndData() {}
	FBulletTickEndData(
		const FBulletSyncState* SyncState,
		const FBulletAuxStateContext* AuxState)
	{
		this->SyncState = *SyncState;
		this->AuxState = *AuxState;
	}

	void InitForNewFrame()
	{
		MovementEndState.ResetToDefaults();
		MoveRecord.Reset();
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Bullet)
	FBulletSyncState SyncState;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Bullet)
	FBulletAuxStateContext AuxState;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Bullet)
	FBulletMovementModeTickEndState MovementEndState;

	FBulletMovementRecord MoveRecord;
};

// Input parameters to provide context for SimulationTick functions
USTRUCT(BlueprintType)
struct FSimulationTickParams
{
	GENERATED_BODY()

	// Components involved in movement by the simulation
	// This will be empty when the simulation is ticked asynchronously
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Bullet)
	FBulletMovingComponentSet MovingComps;

	// Blackboard
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Bullet)
	TObjectPtr<UBulletBlackboard> SimBlackboard;

	// Simulation state data at the start of the tick, including Input Cmd
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Bullet)
	FBulletTickStartData StartState;

	// Time and frame information for this tick
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Bullet)
	FBulletTimeStep TimeStep;

	// Proposed movement for this tick
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Bullet)
	FBulletProposedMove ProposedMove;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UBulletInputProducerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * BulletInputProducerInterface: API for any object that can produce input for a Bullet simulation frame
 */
class IBulletInputProducerInterface : public IInterface
{
	GENERATED_BODY()

public:
	/** Contributes additions to the input cmd for this simulation frame. Typically this is translating accumulated user input (or AI state) into parameters that affect movement. */
	UFUNCTION(BlueprintNativeEvent)
	BULLETNPP_API void ProduceInput(int32 SimTimeMs, FBulletInputCmdContext& InputCmdResult);
};


/** 
 * FBulletPredictTrajectoryParams: parameter block for querying future trajectory samples based on a starting state
 * See UBulletComponent::GetPredictedTrajectory
 */
USTRUCT(BlueprintType)
struct FBulletPredictTrajectoryParams
{
	GENERATED_BODY()

	/** How many samples to predict into the future, including the first sample, which is always a snapshot of the
	 *  starting state with 0 accumulated time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet, meta = (ClampMin = 1))
	int32 NumPredictionSamples = 1;

	/* How much time between predicted samples */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet, meta = (ClampMin = 0.00001))
	float SecondsPerSample = 0.333f;

	/** If true, samples are based on the visual component transform, rather than the 'updated' movement root.
	 *  Typically, this is a mesh with its component location at the bottom of the collision primitive.
	 *  If false, samples are from the movement root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet)
	bool bUseVisualComponentRoot = false;

	/** If true, gravity will not taken into account during prediction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet)
	bool bDisableGravity = false;

 	/** Optional starting sync state. If not set, prediction will begin from the current state. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet)
 	TOptional<FBulletSyncState> OptionalStartSyncState;
 
 	/** Optional starting aux state. If not set, prediction will begin from the current state. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet)
 	TOptional<FBulletAuxStateContext> OptionalStartAuxState;

 	/** Optional input cmds to use, one per sample. If none are specified, prediction will begin with last-used inputs. 
 	 *  If too few are specified for the number of samples, the final input in the array will be used repeatedly to cover remaining samples. */
 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet)
 	TArray<FBulletInputCmdContext> OptionalInputCmds;

};