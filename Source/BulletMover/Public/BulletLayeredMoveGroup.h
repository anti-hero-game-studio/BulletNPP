// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "MoveLibrary/BulletMovementUtilsTypes.h"
#include "BulletLayeredMoveGroup.generated.h"

#define UE_API BULLETMOVER_API

struct FBulletLayeredMoveInstance;
struct FBulletLayeredMoveInstancedData;
struct FBulletProposedMove;
class UBulletMovementMixer;
struct FBulletMoverTickStartData;
struct FBulletMoverTimeStep;
class UBulletMoverBlackboard;
class UBulletLayeredMoveLogic;

/**
 * The group of information about currently active and queued moves.
 * This replicates info for FBulletLayeredMoveInstancedData only - it is expected that the corresponding UBulletLayeredMoveLogic is 
 * already registered with the mover component.
 */
USTRUCT(BlueprintType)
struct FBulletLayeredMoveInstanceGroup
{
	GENERATED_BODY()

	UE_API FBulletLayeredMoveInstanceGroup();
	UE_API FBulletLayeredMoveInstanceGroup& operator=(const FBulletLayeredMoveInstanceGroup& Other);
	UE_API bool operator==(const FBulletLayeredMoveInstanceGroup& Other) const;
	bool operator!=(const FBulletLayeredMoveInstanceGroup& Other) const { return !operator==(Other); }

	/** Checks only whether there are matching LayeredMoves, but NOT necessarily identical states of each move */
	UE_API bool HasSameContents(const FBulletLayeredMoveInstanceGroup& Other) const;
	
	UE_API bool GenerateMixedMove(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, UBulletMovementMixer& MovementMixer, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutMixedMove);
	UE_API void ApplyResidualVelocity(FBulletProposedMove& InOutProposedMove);
	UE_API void NetSerialize(FArchive& Ar, uint8 MaxNumMovesToSerialize = MAX_uint8);
	UE_API void AddStructReferencedObjects(FReferenceCollector& Collector) const;
	UE_API void ResetResidualVelocity();
	UE_API void Reset();

	/**
	 * Loops through all Queued and Active moves and populates any missing MoveLogic using FBulletLayeredMoveInstance::PopulateMissingActiveMoveLogic.
	 * See FBulletLayeredMoveInstance::PopulateMissingActiveMoveLogic function for more details.
	 */
	UE_API void PopulateMissingActiveMoveLogic(const TArray<TObjectPtr<UBulletLayeredMoveLogic>>& RegisteredMoves);
	
	/** Adds the active move to the queued array of the move group */
	UE_API void QueueLayeredMove(const TSharedPtr<FBulletLayeredMoveInstance>& Move);
	
	/** @return True if there are any active or queued moves in this group */
	bool HasAnyMoves() const { return (!ActiveMoves.IsEmpty() || !QueuedMoves.IsEmpty()); }
	
	/** @return True if there is at least one layered move that's either active or queued and is associated with the provided logic or data type */
	template <typename MoveElementT UE_REQUIRES(std::is_base_of_v<FBulletLayeredMoveInstancedData, MoveElementT> || std::is_base_of_v<UBulletLayeredMoveLogic, MoveElementT>)>
	bool HasMove() const
	{
		return FindActiveMove<MoveElementT>() || FindQueuedMove<MoveElementT>();
	}
	
	/** Get a simplified string representation of this group. Typically for debugging. */
	UE_API FString ToSimpleString() const;
	
	/** Returns the first active layered move associated with logic of the specified type, if one exists */
	template <typename MoveLogicT = UBulletLayeredMoveLogic UE_REQUIRES(std::is_base_of_v<UBulletLayeredMoveLogic, MoveLogicT>)>
	const FBulletLayeredMoveInstance* FindActiveMove(TSubclassOf<UBulletLayeredMoveLogic> MoveLogicClass = MoveLogicT::StaticClass()) const
	{
		return PrivateFindActiveMove(MoveLogicClass);
	}
	
	/** Returns the first active layered move using data of the specified type, if one exists */
	template <typename MoveDataT = FBulletLayeredMoveInstancedData UE_REQUIRES(std::is_base_of_v<FBulletLayeredMoveInstancedData, MoveDataT>)>
	const FBulletLayeredMoveInstance* FindActiveMove(const UScriptStruct* MoveDataType = MoveDataT::StaticStruct()) const
	{
		return PrivateFindActiveMove(MoveDataType);
	}
	
	/** Returns the first queued layered move associated with logic of the specified type, if one exists */
	template <typename MoveLogicT = UBulletLayeredMoveLogic UE_REQUIRES(std::is_base_of_v<UBulletLayeredMoveLogic, MoveLogicT>)>
	const FBulletLayeredMoveInstance* FindQueuedMove(TSubclassOf<UBulletLayeredMoveLogic> MoveLogicClass = MoveLogicT::StaticClass()) const
	{
		return PrivateFindQueuedMove(MoveLogicClass);
	}

	/** Returns the first queued layered move using data of the specified type, if one exists */
	template <typename MoveDataT = FBulletLayeredMoveInstancedData UE_REQUIRES(std::is_base_of_v<FBulletLayeredMoveInstancedData, MoveDataT>)>
	const FBulletLayeredMoveInstance* FindQueuedMove(const UScriptStruct* MoveDataType = MoveDataT::StaticStruct()) const
	{
		return PrivateFindQueuedMove(MoveDataType);
	}

	/** Cancel any active or queued moves with a matching tag */
	void CancelMovesByTag(FGameplayTag Tag, bool bRequireExactMatch); 

	// Clears out any finished or invalid active moves and adds any queued moves to the active moves
	UE_API void FlushMoveArrays(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard);
	
protected:
	// Helper function for gathering any residual velocity settings from layered moves that just ended
	void ProcessFinishedMove(const FBulletLayeredMoveInstance& Move, bool& bResidualVelocityOverriden, bool& bClampVelocityOverriden);
	
	/** Moves that are currently active in this group */
	TArray<TSharedPtr<FBulletLayeredMoveInstance>> ActiveMoves;

	/** Moves that are queued to become active next sim frame */
	TArray<TSharedPtr<FBulletLayeredMoveInstance>> QueuedMoves;

private:
	const FBulletLayeredMoveInstance* PrivateFindActiveMove(const TSubclassOf<UBulletLayeredMoveLogic>& MoveLogicClass) const;
	const FBulletLayeredMoveInstance* PrivateFindActiveMove(const UScriptStruct* MoveDataType) const;
	const FBulletLayeredMoveInstance* PrivateFindQueuedMove(const TSubclassOf<UBulletLayeredMoveLogic>& MoveLogicClass) const;
	const FBulletLayeredMoveInstance* PrivateFindQueuedMove(const UScriptStruct* MoveDataType) const;
	
	//@todo DanH: Maybe these should be grouped in a struct?
	/**
	 * Clamps an actors velocity to this value when a layered move ends. This expects Value >= 0.
	 * Note: This is set automatically when a layered move has ended based off of end velocity settings - it is not meant to be set by the user see @FBulletLayeredMoveFinishVelocitySettings
	 */
	UPROPERTY(NotReplicated, VisibleAnywhere, BlueprintReadOnly, Category = Mover, meta = (AllowPrivateAccess))
	float ResidualClamping;

	/**
	 * If true ResidualVelocity will be the next velocity used for this actor
	 * Note: This is set automatically when a layered move has ended based off of end velocity settings - it is not meant to be set by the user see @FBulletLayeredMoveFinishVelocitySettings
	 */
	UPROPERTY(NotReplicated, VisibleAnywhere, BlueprintReadOnly, Category = Mover, meta = (AllowPrivateAccess))
	bool bApplyResidualVelocity;

	/**
	 * If bApplyResidualVelocity is true this actors velocity will be set to this.
	 * Note: This is set automatically when a layered move has ended based off of end velocity settings - it is not meant to be set by the user see @FBulletLayeredMoveFinishVelocitySettings
	 */
	UPROPERTY(NotReplicated, VisibleAnywhere, BlueprintReadOnly, Category = Mover, meta = (AllowPrivateAccess))
	FVector ResidualVelocity;

	/** Used during simulation to cancel any moves that match a tag */
	TArray<TPair<FGameplayTag, bool>> TagCancellationRequests;
};

template<>
struct TStructOpsTypeTraits<FBulletLayeredMoveInstanceGroup> : public TStructOpsTypeTraitsBase2<FBulletLayeredMoveInstanceGroup>
{
	enum
	{
		WithCopy = true,
		//WithNetSerializer = true,
		WithIdenticalViaEquality = true,
		WithAddStructReferencedObjects = true,
	};
};

#undef UE_API
