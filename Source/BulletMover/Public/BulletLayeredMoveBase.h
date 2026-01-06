// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "MoveLibrary/BulletMovementUtilsTypes.h"
#include "Templates/SubclassOf.h"
#include "BulletLayeredMove.h"
#include "BulletMoverLog.h"

#include "BulletLayeredMoveBase.generated.h"

//////////////////////////////////////////////////////////////////////////
// TODO: Consider relocating this class and functionality, a lot of what's here belongs in the original LayeredMove.h/.cpp as replacements for FBulletLayeredMoveBase and such
//		 but we wanted to explore this implementation of Blueprintable layered moves without disrupting the old system so it lives here for now.
//////////////////////////////////////////////////////////////////////////

#define UE_API BULLETMOVER_API
#define USING_BULLET_PARAMS(ActivationParamsClass) using ActivationParamsType = ActivationParamsClass;
#define USING_BULLET_MOVE_DATA(MoveDataClass) using MoveDataType = MoveDataClass;

class UBulletMovementMixer;
struct FBulletMoverTickStartData;
struct FBulletMoverTimeStep;
class UBulletMoverBlackboard;
class UBulletMoverComponent;
class UBulletLayeredMoveLogic;

/**
 * Packaged params struct for initializing a corresponding FBulletLayeredMoveInstancedData
 * Allows BP to do "templated" move data creation. Optional in C++, where params can be forwarded to the FBulletLayeredMoveInstancedData ctor directly
 * The base class can also be used on any activation to just use default values
 */
USTRUCT(Blueprintable)
struct FBulletLayeredMoveActivationParams
{
	GENERATED_BODY()
	
	/**
 	 * This move will expire after a set amount of time if > 0. If 0, it will be ticked only once, regardless of time step. It will need to be manually ended if < 0.
 	 * Note: If changed after starting to a value beneath the current lifetime of the move, it will immediately finish (so if your move finishes early, setting this to 0 is equivalent to returning true from IsFinished())
 	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	double DurationMs = 0.f;
};

/** Instanced data created and replicated for each activation of a layered move */
USTRUCT(Blueprintable)
struct FBulletLayeredMoveInstancedData
{
	GENERATED_BODY()

	//@todo DanH: If we're already using a macro, might as well put the boilerplate virtuals in here
	USING_BULLET_PARAMS(FBulletLayeredMoveActivationParams);
	
	FBulletLayeredMoveInstancedData() {}
	virtual ~FBulletLayeredMoveInstancedData() {}
	// Checks if 
	UE_API bool operator==(const FBulletLayeredMoveInstancedData& Other) const;
	bool operator!=(const FBulletLayeredMoveInstancedData& Other) const { return !operator==(Other); }

	/** @return A newly allocated copy of this FBulletLayeredMoveInstancedData. Must be overridden by child classes */
	virtual FBulletLayeredMoveInstancedData* Clone() const { return new FBulletLayeredMoveInstancedData(*this); }

	/** @return The UScriptStruct describing this struct. Must be overridden by child classes */
	virtual UScriptStruct* GetScriptStruct() const { return StaticStruct(); }
	
	/** @return True if this move data is identical to OtherData. OtherData is guaranteed to be safe to cast to the implementing type. */
	UE_API virtual bool Equals(const FBulletLayeredMoveInstancedData& OtherData) const;
	
	/** Called when a queued layered move is activated. Provides an opportunity to initialize Layered MoveData */
	UE_API virtual void ActivateFromContext(const FBulletLayeredMoveActivationParams* ActivationParams);
	
	UE_API virtual void NetSerialize(FArchive& Ar);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}

	/** Is this move considered to "have" a given gameplay tag? */
	virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const { return false; }
	
	double GetStartSimTimeMs() const { return StartSimTimeMs; }

	/**
	 * This move will expire after a set amount of time if > 0. If 0, it will be ticked only once, regardless of time step. It will need to be manually ended if < 0.
	 * Note: If changed after starting to a value beneath the current lifetime of the move, it will immediately finish (so if your move finishes early, setting this to 0 is equivalent to returning true from IsFinished())
	 */
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	double DurationMs = -1.f;
	
	/** The simulation time this move first ticked (< 0 means it hasn't started yet) */
    UPROPERTY(BlueprintReadOnly, Category = Mover)
    double StartSimTimeMs = -UE_BIG_NUMBER;

	friend class UBulletLayeredMoveLogic;
};

template<>
struct TStructOpsTypeTraits<FBulletLayeredMoveInstancedData> : public TStructOpsTypeTraitsBase2<FBulletLayeredMoveInstancedData>
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};

/**
 * Base class for all layered move logic that operates in tandem with instanced FBulletLayeredMoveInstancedData.
 * The logic object itself is not meant to ever be replicated, and a maximum of one instance of each logic class
 * need ever exist on a given MoverComponent. Repeated and/or simultaneous activations of the same move on a component
 * are represented, tracked, and replicated through instances of the FBulletLayeredMoveInstancedData struct type
 * that the logic class indicates in the ActiveMoveDataStructType property.
 *
 * The virtual methods on this class are invoked in a special and strict pattern that guarantees AccessExecutionMoveData
 * will return the valid data instance relevant to that function execution.
 *
 * Refer to [Examples when they exist] for implementation examples
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class UBulletLayeredMoveLogic : public UObject
{
	GENERATED_BODY()

public:
	USING_BULLET_MOVE_DATA(FBulletLayeredMoveInstancedData)
	
	UE_API UBulletLayeredMoveLogic();

	UScriptStruct* GetInstancedDataType() const { return InstancedDataStructType; }
	
	const FBulletLayeredMoveFinishVelocitySettings& GetFinishVelocitySettings() const { return FinishVelocitySettings; }
	EBulletMoveMixMode GetMixMode() const { return MixMode; }
	uint8 GetPriority() const { return Priority; }

	// Helper function for validating move data when passing data and logic to/from BP
	static bool ValidateMoveDataGetSet(const UObject* ObjectValidatingData, const UBulletLayeredMoveLogic* MoveLogic, const FStructProperty* MoveDataProperty, const uint8* MoveDataPtr, FFrame& StackFrame);
	
private:
	// These functions all assume validity of CurrentActiveMoveData to perform logic from the perspective of that chunk of properties
	// To that end, the only external entity capable of invoking them is the FScopedMoveLogicExecContext, which guarantees the active move data has been set
	friend class FScopedMoveLogicExecContext;
	
protected:
	/** Called when this move is initially activated  */
	UFUNCTION(BlueprintNativeEvent)
	UE_API void OnStart(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard);

	/** Called when this move has ended  */
	UFUNCTION(BlueprintNativeEvent)
	UE_API void OnEnd(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard);

	/** Generate a movement that will be combined with other sources */
	UFUNCTION(BlueprintNativeEvent)
	UE_API bool GenerateMove(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard, UPARAM(ref) const FBulletMoverTickStartData& StartState, UPARAM(ref) FBulletProposedMove& OutProposedMove);
	
	//@todo: Will need to cache whether to treat the instance data as const (i.e. whether to disregard & complain if someone tries to set the data in BP during that window) 
	UFUNCTION(BlueprintNativeEvent)
	UE_API bool IsFinished(const FBulletMoverTimeStep& TimeStep, const UBulletMoverBlackboard* SimBlackboard);
	
	virtual void OnStart_Implementation(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard) {}
	virtual void OnEnd_Implementation(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard) {}
	virtual bool GenerateMove_Implementation(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard, const FBulletMoverTickStartData& StartState, FBulletProposedMove& OutProposedMove) { return false; }
	UE_API virtual bool IsFinished_Implementation(const FBulletMoverTimeStep& TimeStep, const UBulletMoverBlackboard* SimBlackboard);
	
	/** Accessor to the FBulletLayeredMoveInstancedData established for the execution of a virtual move logic function */
	template <typename MoveDataT = FBulletLayeredMoveInstancedData UE_REQUIRES(std::is_base_of_v<FBulletLayeredMoveInstancedData, MoveDataT>)>
	MoveDataT& AccessExecutionMoveData() const
	{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
		if (!ensure(InstancedDataStructType))
        {
			UE_LOG(LogBulletMover, Error, TEXT("Move Logic needs an active data struct type. If no data is needed consider using the default move data type."))
        	static MoveDataT GarbageData;
        	return GarbageData;
        }
#endif
		
		check(InstancedDataStructType);
		return static_cast<MoveDataT&>(*CurrentInstancedData);
	}
	
	/**
	 * Gets active move data that is tied to this active move. Use SetActiveMoveData to write active move data.
	 * Note: Make sure break node is the correct data type specified by the layered move logic.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "BulletMover | Layered Move", DisplayName = "Get Active Move Data", meta = (BlueprintProtected, BlueprintInternalUseOnly, CustomStructureParam = "OutMoveData", DefaultToSelf = "MoveLogic"))
	static bool K2_GetActiveMoveData(UBulletLayeredMoveLogic* MoveLogic, FBulletLayeredMoveInstancedData& OutMoveData);
	DECLARE_FUNCTION(execK2_GetActiveMoveData);
	
	/**
	 * Sets active move data that is tied to this active move. Use GetActiveMoveData to read current active move data.
	 * Note: Make sure make node is the correct data type specified by the layered move logic.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "BulletMover | Layered Move", DisplayName = "Set Active Move Data", meta = (BlueprintProtected, BlueprintInternalUseOnly, CustomStructureParam = "OutMoveData", DefaultToSelf = "MoveLogic"))
	static void K2_SetActiveMoveData(UBulletLayeredMoveLogic* MoveLogic, const FBulletLayeredMoveInstancedData& OutMoveData);
	DECLARE_FUNCTION(execK2_SetActiveMoveData);
	
	/**
	 * This move will expire after a set amount of time if > 0. If 0, it will be ticked only once, regardless of time step. It will need to be manually ended if < 0.
	 * Note: If changed after starting to a value beneath the current lifetime of the move, it will immediately finish (so if your move finishes early, setting this to 0 is equivalent to returning true from IsFinished())
	 */
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	double DefaultDurationMs = -1.f;
	
	/** Determines how this object's movement contribution should be mixed with others */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mover)
	EBulletMoveMixMode MixMode = EBulletMoveMixMode::AdditiveVelocity;

	/** Determines if this layered move should take priority over other layered moves when different moves have conflicting overrides - higher numbers taking precedent. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mover)
	uint8 Priority = 0;

	//@todo: Does this need to be a separate struct?
	/** Settings related to velocity applied to the actor after the move has finished */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mover)
	FBulletLayeredMoveFinishVelocitySettings FinishVelocitySettings;

	//@todo DanH: Fail validation if this isn't set or isn't an FBulletLayeredMoveInstancedData
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadOnly, Category = Mover, meta=(MetaStruct="/Script/BulletMover.LayeredMoveInstancedData"))
	TObjectPtr<UScriptStruct> InstancedDataStructType;
	
private:
	/**
	 * The FBulletLayeredMoveInstancedData provided to each of the base virtual move functions, valid only for the duration a single virtual function execution.
	 * Direct access is only for internal plumbing - use AccessExecutionMoveData() in virtuals to obtain a typed reference to this.
	 */
	TSharedPtr<FBulletLayeredMoveInstancedData> CurrentInstancedData;
};

/**
 * Wrapper to encapsulate the split implementation of a move between a stateless UBulletLayeredMoveLogic* object and an instance of FBulletLayeredMoveInstancedData
 * Those two pieces, in tandem, represent a "whole" functional Layered Move.
 */
USTRUCT()
struct FBulletLayeredMoveInstance
{
	GENERATED_BODY()
	
	UE_API FBulletLayeredMoveInstance();
	UE_API FBulletLayeredMoveInstance(const FBulletLayeredMoveInstance& OtherLayeredMoveInstance);
	UE_API FBulletLayeredMoveInstance(const TSharedRef<FBulletLayeredMoveInstancedData>& InMoveData, UBulletLayeredMoveLogic* InMoveLogic = nullptr);
	
	/** TArray::FindByKey() enablers */
	bool operator==(const TSubclassOf<UBulletLayeredMoveLogic>& LogicClass) const { return MoveLogic && MoveLogic->IsA(LogicClass); }
	bool operator==(const UScriptStruct* MoveDataType) const { return InstanceMoveData->GetScriptStruct()->IsChildOf(MoveDataType); }

	bool HasLogic() const { return MoveLogic != nullptr; }
	const UClass* GetLogicClass() const { return MoveLogic->GetClass(); }
	
	UE_API void StartMove(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard) const;
	UE_API bool GenerateMove(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutProposedMove) const;
	UE_API void EndMove(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard) const;
	UE_API bool IsFinished(const FBulletMoverTimeStep& TimeStep, const UBulletMoverBlackboard* SimBlackboard) const;

	UE_API const FBulletLayeredMoveFinishVelocitySettings& GetFinishVelocitySettings() const;
	UE_API EBulletMoveMixMode GetMixMode() const;
	uint8 GetPriority() const { return MoveLogic->GetPriority(); }
	double GetStartingTimeMs() const { return InstanceMoveData->GetStartSimTimeMs(); }
	bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const { return InstanceMoveData->HasGameplayTag(TagToFind, bExactMatch); }

	UE_API FBulletLayeredMoveInstance Clone() const;
	UScriptStruct* GetDataStructType() const { return InstanceMoveData->GetScriptStruct(); }
	void AddReferencedObjects(FReferenceCollector& Collector) const { InstanceMoveData->AddReferencedObjects(Collector); } 

	UE_API void NetSerialize(FArchive& Ar);

	UE_API const UClass* GetSerializedMoveLogicClass() const;

	/**
	 * This function populates the MoveLogic reference of ActiveMoves that don't have any logic classes. This is necessary as
	 * active move data received from NetSerialize doesn't necessarily have the Logic class it was activated with.
	 * Note: Currently we are NetSerializing the MoveLogic class so that we can then populate Logic from using the registered move array
	 * on the MoverComponent but eventually we should use a ID mapping or something better to avoid serializing the MoveLogic class.
	 */
	bool PopulateMissingActiveMoveLogic(const TArray<TObjectPtr<UBulletLayeredMoveLogic>>& RegisteredMoves);
	
private:
	TSharedPtr<FBulletLayeredMoveInstancedData> InstanceMoveData;
	
	/**
	 * Is used in PopulateMissingActiveMoveLogic to help populate logic classes in active moves that were NetSerialized since MoveLogic isn't NetSerialized
	 * TODO: Eventually replace this with some ID/Mapping system so we can NetSerialize that instead of a full UClass
	 */
	UPROPERTY(Transient)
	TSubclassOf<UBulletLayeredMoveLogic> MoveLogicClassType;
	
	//@todo DanH: This exists on the PT for the chaos mover - can/should this still be a UProperty TObjectPtr?
	UPROPERTY(Transient)
	TObjectPtr<UBulletLayeredMoveLogic> MoveLogic;
};

template<>
struct TStructOpsTypeTraits<FBulletLayeredMoveInstance> : public TStructOpsTypeTraitsBase2<FBulletLayeredMoveInstance>
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};

#undef UE_API
