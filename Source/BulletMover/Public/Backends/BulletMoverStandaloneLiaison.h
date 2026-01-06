// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "Backends/BulletMoverBackendLiaison.h"
#include "BulletMoverStandaloneLiaison.generated.h"

#define UE_API BULLETMOVER_API

class UBulletMoverComponent;
class UBulletMoverStandaloneLiaisonComponent;
class AController;





// Tick task for producing input before the next movement simulation step
USTRUCT()
struct FBulletMoverStandaloneProduceInputTickFunction : public FTickFunction
{
	GENERATED_BODY()

	/** Standalone liaison that is the target of this tick **/
	TWeakObjectPtr<UBulletMoverStandaloneLiaisonComponent> Target;

	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;
};

template<>
struct TStructOpsTypeTraits<FBulletMoverStandaloneProduceInputTickFunction> : public TStructOpsTypeTraitsBase2<FBulletMoverStandaloneProduceInputTickFunction>
{
	enum
	{
		WithCopy = false
	};
};


// Tick task for advancing the movement simulation step, after input has been produced
USTRUCT()
struct FBulletMoverStandaloneSimulateMovementTickFunction : public FTickFunction
{
	GENERATED_BODY()

	/** Standalone liaison that is the target of this tick **/
	TWeakObjectPtr<UBulletMoverStandaloneLiaisonComponent> Target;

	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;
};

template<>
struct TStructOpsTypeTraits<FBulletMoverStandaloneSimulateMovementTickFunction> : public TStructOpsTypeTraitsBase2<FBulletMoverStandaloneSimulateMovementTickFunction>
{
	enum
	{
		WithCopy = false
	};
};


// Tick task for applying the new simulation state to the actor/components, after movement has been simulated
USTRUCT()
struct FBulletMoverStandaloneApplyStateTickFunction : public FTickFunction
{
	GENERATED_BODY()

	/** Standalone liaison that is the target of this tick **/
	TWeakObjectPtr<UBulletMoverStandaloneLiaisonComponent> Target;

	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;
};

template<>
struct TStructOpsTypeTraits<FBulletMoverStandaloneApplyStateTickFunction> : public TStructOpsTypeTraitsBase2<FBulletMoverStandaloneApplyStateTickFunction>
{
	enum
	{
		WithCopy = false
	};
};




/**
 * MoverStandaloneLiaison: this component acts as a backend driver for an actor's Mover component, for use in Standalone (non-networked) games.
 * This class is set on a Mover component as the "back end".
 */
UCLASS(MinimalAPI)
class UBulletMoverStandaloneLiaisonComponent : public UActorComponent, public IBulletMoverBackendLiaisonInterface
{
	GENERATED_BODY()

public:
	UE_API UBulletMoverStandaloneLiaisonComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// IBulletMoverBackendLiaisonInterface
	UE_API virtual double GetCurrentSimTimeMs() override;
	UE_API virtual int32 GetCurrentSimFrame() override;
	UE_API virtual bool ReadPendingSyncState(OUT FBulletMoverSyncState& OutSyncState) override;
	UE_API virtual bool WritePendingSyncState(const FBulletMoverSyncState& SyncStateToWrite) override;
	// End IBulletMoverBackendLiaisonInterface

	// Begin UActorComponent interface
	UE_API virtual void RegisterComponentTickFunctions(bool bRegister) override;
	UE_API virtual void BeginPlay() override;
	// End UActorComponent interface

	UE_API FTickFunction* FindTickFunction(EBulletMoverTickPhase MoverTickPhase);

	/**
	 * Adds a tick dependency between another component and one of mover's tick functions.
	 * @param OtherComponent		The component to add a dependency with.
	 * @param TickOrder				What order OtherComponent's should have relative to TickPhase i.e. OtherComponent Before SimulateMovement.
	 * @param TickPhase				The mover tick phase we want to a dependency with.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ticking", meta = (Keywords = "Tick Dependency"))
	UE_API void AddTickDependency(UActorComponent* OtherComponent, EBulletMoverTickDependencyOrder TickOrder, EBulletMoverTickPhase TickPhase);
	
	/** Sets whether this instance's produce input can run on worker threads or not. See @bUseAsyncProduceInput and @SetEnableProduceInput */
	UFUNCTION(BlueprintCallable, Category = "Ticking")
	UE_API void SetUseAsyncProduceInput(bool bUseAsyncInputProduction);

	UFUNCTION(BlueprintCallable, Category = "Ticking")
	UE_API bool GetUseAsyncProduceInput() const;

	/** Sets whether this instance's produce-input tick will run at all. It may be useful to disable on actors that don't rely on Mover input to move.  */
	UFUNCTION(BlueprintCallable, Category = "Ticking")
	UE_API void SetEnableProduceInput(bool bEnableInputProduction);
	
	/** Whether this instance will have its produce-input tick called. */
	UFUNCTION(BlueprintCallable, Category = "Ticking")
	UE_API bool GetEnableProduceInput() const;
	
	/** Sets whether this instance's movement simulation tick can run on worker threads or not. See @bUseAsyncMovementSimulationTick */
	UFUNCTION(BlueprintCallable, Category = "Ticking")
	UE_API void SetUseAsyncMovementSimulationTick(bool bUseAsyncMovementSim);

	UFUNCTION(BlueprintCallable, Category = "Ticking")
	UE_API bool GetUseAsyncMovementSimulationTick() const;

protected:
	UE_API void TickInputProduction(float DeltaSeconds);
	UE_API void TickMovementSimulation(float DeltaSeconds);
	UE_API void TickApplySimulationState(float DeltaSeconds);

	UE_API void UpdateSimulationTime();

	// Called when controller changes, used to manage ticking dependencies
	UFUNCTION()
	UE_API virtual void OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

protected:
	/**
	 * Sets whether produce input can run on worker threads or not, also gated by global option.
	 * Changes at runtime will take affect next frame. Has no effect on simulation ticking or applying results.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Async Movement")
	bool bUseAsyncProduceInput = false;
	
	/**
	 * Sets whether the movement simulation tick can run on worker threads or not, also gated by global option.
	 * Changes at runtime will take affect next frame. Has no effect on input production or applying results.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Async Movement")
	bool bUseAsyncMovementSimulationTick = false;

	TObjectPtr<UBulletMoverComponent> MoverComp;	// the component that we're in charge of driving

	double CurrentSimTimeMs;
	int32 CurrentSimFrame;

	FBulletMoverInputCmdContext LastProducedInputCmd;

	FBulletMoverSyncState CachedLastSyncState;
	FBulletMoverAuxStateContext CachedLastAuxState;
	bool bIsCachedStateDirty = false;	// If true, we need to propagate the state to our MoverComponent during ApplyState

	FRWLock StateDataLock;	// used when reading/writing to our cached state (sync/aux) data

	bool bIsInApplySimulationState = false;	// transient flag indicating ApplySimulationState is active

	FBulletMoverStandaloneProduceInputTickFunction ProduceInputTickFunction;
	FBulletMoverStandaloneSimulateMovementTickFunction SimulateMovementTickFunction;
	FBulletMoverStandaloneApplyStateTickFunction ApplyStateTickFunction;

	friend struct FBulletMoverStandaloneProduceInputTickFunction;
private:
	// Internal-use-only tick data structs, for efficiency since they typically have the same contents from frame to frame
	FBulletMoverTickStartData WorkingStartData;
	FBulletMoverTickEndData WorkingEndData;

	friend struct FBulletMoverStandaloneSimulateMovementTickFunction;
	friend struct FBulletMoverStandaloneApplyStateTickFunction;
};

#undef UE_API
