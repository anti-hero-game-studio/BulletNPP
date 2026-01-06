// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletLayeredMoveGroup.h"
#include "BulletLayeredMoveBase.h"
#include "BulletMoverLog.h"
#include "MoveLibrary/BulletMovementMixer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletLayeredMoveGroup)

FBulletLayeredMoveInstanceGroup::FBulletLayeredMoveInstanceGroup()
	: ResidualClamping(-1.f)
	, bApplyResidualVelocity(false)
	, ResidualVelocity(FVector::Zero())
{
}

FBulletLayeredMoveInstanceGroup& FBulletLayeredMoveInstanceGroup::operator=(const FBulletLayeredMoveInstanceGroup& Other)
{
	if (this != &Other)
	{
		const auto DeepCopyMovesFunc = [](const TArray<TSharedPtr<FBulletLayeredMoveInstance>>& From, TArray<TSharedPtr<FBulletLayeredMoveInstance>>& To)
		{
			To.Empty(From.Num());
			for (const TSharedPtr<FBulletLayeredMoveInstance>& Move : From)
			{
				To.Add(Move);
			}
		};
		DeepCopyMovesFunc(Other.ActiveMoves, ActiveMoves);
		DeepCopyMovesFunc(Other.QueuedMoves, QueuedMoves);

		TagCancellationRequests = Other.TagCancellationRequests;
	}

	return *this;
}

bool FBulletLayeredMoveInstanceGroup::operator==(const FBulletLayeredMoveInstanceGroup& Other) const
{
	// Todo: Deep move-by-move comparison
	if (ActiveMoves.Num() != Other.ActiveMoves.Num())
	{
		return false;
	}
	if (QueuedMoves.Num() != Other.QueuedMoves.Num())
	{
		return false;
	}
	
	return true;
}

void FBulletLayeredMoveInstanceGroup::QueueLayeredMove(const TSharedPtr<FBulletLayeredMoveInstance>& Move)
{
	if (ensure(Move.IsValid() && Move->HasLogic()))
	{
		QueuedMoves.Add(Move);
		// TODO NS: Add simple string support and re add this log
		//UE_LOG(LogBulletMover, VeryVerbose, TEXT("BulletLayeredMove queued move (%s)"), *Move->ToSimpleString());
	}
}
bool FBulletLayeredMoveInstanceGroup::HasSameContents(const FBulletLayeredMoveInstanceGroup& Other) const
{
	// Only compare the types of moves contained, not the state
	if (ActiveMoves.Num() != Other.ActiveMoves.Num() ||
		QueuedMoves.Num() != Other.QueuedMoves.Num())
	{
		return false;
	}
	for (int32 i = 0; i < ActiveMoves.Num(); ++i)
	{
		if (ActiveMoves[i]->GetDataStructType() != Other.ActiveMoves[i]->GetDataStructType())
		{
			return false;
		}
	}
	for (int32 i = 0; i < QueuedMoves.Num(); ++i)
	{
		if (QueuedMoves[i]->GetDataStructType() != Other.QueuedMoves[i]->GetDataStructType())
		{
			return false;
		}
	}
	return true;
}
void FBulletLayeredMoveInstanceGroup::ApplyResidualVelocity(FBulletProposedMove& ProposedMove)
{
	if (bApplyResidualVelocity)
	{
		ProposedMove.LinearVelocity = ResidualVelocity;
	}
	if (ResidualClamping >= 0.0f)
	{
		ProposedMove.LinearVelocity = ProposedMove.LinearVelocity.GetClampedToMaxSize(ResidualClamping);
	}
	ResetResidualVelocity();
}
bool FBulletLayeredMoveInstanceGroup::GenerateMixedMove(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, UBulletMovementMixer& MovementMixer, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutMixedMove)
{
	// Tick and accumulate all active moves
	// Gather all proposed moves and distill this into a cumulative movement report. May include separate additive vs override moves.
	// TODO: may want to sort by priority or other factors
	bool bHasLayeredMoveContributions = false;
	for (const TSharedPtr<FBulletLayeredMoveInstance>& Move : ActiveMoves)
	{
		FBulletProposedMove MoveStep;
		if (Move->GenerateMove(StartState, TimeStep, SimBlackboard, MoveStep))
		{	
			bHasLayeredMoveContributions = true;
			
			MovementMixer.MixLayeredMove(*Move, MoveStep, OutMixedMove);
		}
	}
	
	return bHasLayeredMoveContributions;
}

void FBulletLayeredMoveInstanceGroup::NetSerialize(FArchive& Ar, uint8 MaxNumMovesToSerialize/* = MAX_uint8*/)
{
	const auto NetSerializeMovesArrayFunc = [&Ar, this](TArray<TSharedPtr<FBulletLayeredMoveInstance>>& MovesArray, uint8 MaxArraySize)
	{
		uint8 NumMovesToSerialize;
		if (Ar.IsSaving())
		{
			UE_CLOG(MovesArray.Num() > MaxArraySize, LogBulletMover, Warning, TEXT("Too many Layered Moves (%d!) to net serialize. Clamping to %d"), MovesArray.Num(), MaxArraySize);
			NumMovesToSerialize = FMath::Min<int32>(MovesArray.Num(), MaxArraySize);
		}

		Ar << NumMovesToSerialize;

		if (Ar.IsLoading())
		{
			// Note that any instances of FBulletLayeredMoveInstance added this way won't have their constructor called
			// They are not safe to use until after the NetSerialize() that immediately follows below (so don't add anything in between!) 
			MovesArray.SetNumZeroed(NumMovesToSerialize);
			
			for (int32 MoveIdx = 0; MoveIdx < NumMovesToSerialize && !Ar.IsError(); ++MoveIdx)
			{
				MovesArray[MoveIdx] = MakeShared<FBulletLayeredMoveInstance>(MakeShared<FBulletLayeredMoveInstancedData>(), nullptr);
				FBulletLayeredMoveInstance* Move = MovesArray[MoveIdx].Get();
				Move->NetSerialize(Ar);
			}
		}
		else
		{
			for (int32 MoveIdx = 0; MoveIdx < NumMovesToSerialize && !Ar.IsError(); ++MoveIdx)
			{
				FBulletLayeredMoveInstance* Move = MovesArray[MoveIdx].Get();
				Move->NetSerialize(Ar);
			}
		}
	};

	NetSerializeMovesArrayFunc(ActiveMoves, MaxNumMovesToSerialize);
	
	const uint8 MaxNumQueuedMovesToSerialize = FMath::Max(MaxNumMovesToSerialize - ActiveMoves.Num(), 0);
	NetSerializeMovesArrayFunc(QueuedMoves, MaxNumQueuedMovesToSerialize);
}


void FBulletLayeredMoveInstanceGroup::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (const TSharedPtr<FBulletLayeredMoveInstance>& Move : ActiveMoves)
	{
		Move->AddReferencedObjects(Collector);
	}
	for (const TSharedPtr<FBulletLayeredMoveInstance>& Move : QueuedMoves)
	{
		Move->AddReferencedObjects(Collector);
	}
}

void FBulletLayeredMoveInstanceGroup::ResetResidualVelocity()
{
	bApplyResidualVelocity = false;
	ResidualVelocity = FVector::ZeroVector;
	ResidualClamping = -1.f;
}

void FBulletLayeredMoveInstanceGroup::Reset()
{
	ResetResidualVelocity();
	QueuedMoves.Empty();
	ActiveMoves.Empty();
	TagCancellationRequests.Empty();
}

void FBulletLayeredMoveInstanceGroup::PopulateMissingActiveMoveLogic(const TArray<TObjectPtr<UBulletLayeredMoveLogic>>& RegisteredMoves)
{
	for (TSharedPtr<FBulletLayeredMoveInstance> ActiveMove : ActiveMoves)
	{
		if (ActiveMove && !ActiveMove->HasLogic())
		{
			bool bPopulatedActiveMoveLogic = ActiveMove->PopulateMissingActiveMoveLogic(RegisteredMoves);
		}
	}

	for (TSharedPtr<FBulletLayeredMoveInstance> QueuedMove : QueuedMoves)
	{
		if (QueuedMove && !QueuedMove->HasLogic())
		{
			bool bPopulatedActiveMoveLogic = QueuedMove->PopulateMissingActiveMoveLogic(RegisteredMoves);
		}
	}
}

FString FBulletLayeredMoveInstanceGroup::ToSimpleString() const
{
	return FString::Printf(TEXT("FBulletLayeredMoveInstanceGroup. Active: %i Queued: %i"), ActiveMoves.Num(), QueuedMoves.Num());
}

const FBulletLayeredMoveInstance* FBulletLayeredMoveInstanceGroup::PrivateFindActiveMove(const TSubclassOf<UBulletLayeredMoveLogic>& MoveLogicClass) const
{
	for (const TSharedPtr<FBulletLayeredMoveInstance>& Move : ActiveMoves)
	{
		if (Move->GetLogicClass() == MoveLogicClass)
		{
			return Move.Get();
		}
	}

	return nullptr;
}

const FBulletLayeredMoveInstance* FBulletLayeredMoveInstanceGroup::PrivateFindActiveMove(const UScriptStruct* MoveDataType) const
{
	for (const TSharedPtr<FBulletLayeredMoveInstance>& Move : ActiveMoves)
	{
		if (Move->GetDataStructType() == MoveDataType)
		{
			return Move.Get();
		}
	}

	return nullptr;
}

const FBulletLayeredMoveInstance* FBulletLayeredMoveInstanceGroup::PrivateFindQueuedMove(const TSubclassOf<UBulletLayeredMoveLogic>& MoveLogicClass) const
{
	for (const TSharedPtr<FBulletLayeredMoveInstance>& Move : QueuedMoves)
	{
		if (Move->GetLogicClass() == MoveLogicClass)
		{
			return Move.Get();
		}
	}

	return nullptr;
}

const FBulletLayeredMoveInstance* FBulletLayeredMoveInstanceGroup::PrivateFindQueuedMove(const UScriptStruct* MoveDataType) const
{
	for (const TSharedPtr<FBulletLayeredMoveInstance>& Move : QueuedMoves)
	{
		if (Move->GetDataStructType() == MoveDataType)
		{
			return Move.Get();
		}
	}

	return nullptr;
}

void FBulletLayeredMoveInstanceGroup::CancelMovesByTag(FGameplayTag Tag, bool bRequireExactMatch)
{

	// Schedule a tag cancellation request, to be handled during simulation
	TagCancellationRequests.Add(TPair<FGameplayTag, bool>(Tag, bRequireExactMatch));

}


void FBulletLayeredMoveInstanceGroup::FlushMoveArrays(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard)
{
	bool bResidualVelocityOverridden = false;
	bool bClampVelocityOverridden = false;

	// Process any cancellations
	{
		for (TPair<FGameplayTag, bool> CancelRequest : TagCancellationRequests)
		{
			const FGameplayTag TagToMatch = CancelRequest.Key;
			const bool bRequireExactMatch = CancelRequest.Value;

			QueuedMoves.RemoveAll([TagToMatch, bRequireExactMatch](const TSharedPtr<FBulletLayeredMoveInstance>& Move)
				{
					return (!Move.IsValid() || Move->HasGameplayTag(TagToMatch, bRequireExactMatch));
				});

			// Process completion of any active moves that being canceled
			ActiveMoves.RemoveAll([&TimeStep, &SimBlackboard, &bResidualVelocityOverridden, &bClampVelocityOverridden, TagToMatch, bRequireExactMatch, this] (const TSharedPtr<FBulletLayeredMoveInstance>& Move)
				{
					if (Move.IsValid() && Move->HasGameplayTag(TagToMatch, bRequireExactMatch))
					{
						ProcessFinishedMove(*Move, bResidualVelocityOverridden, bClampVelocityOverridden);
						Move->EndMove(TimeStep, SimBlackboard);
						return true;
					}
					return false;
				});
		}

		TagCancellationRequests.Empty();
	}

	{
	
		// Process completion of any active moves that are finished
		ActiveMoves.RemoveAll([&TimeStep, &SimBlackboard, &bResidualVelocityOverridden, &bClampVelocityOverridden, this]
			(const TSharedPtr<FBulletLayeredMoveInstance>& Move)
			{
				if (Move.IsValid() && Move->IsFinished(TimeStep, SimBlackboard))
				{
					ProcessFinishedMove(*Move, bResidualVelocityOverridden, bClampVelocityOverridden);
					Move->EndMove(TimeStep, SimBlackboard);
					return true;
				}
				return false;
			});	
	}

	// Begin any queued moves
	for (TSharedPtr<FBulletLayeredMoveInstance>& QueuedMove : QueuedMoves)
	{
		if (QueuedMove.IsValid())
		{
			if (QueuedMove->HasLogic())
			{
				ActiveMoves.Add(QueuedMove);
				QueuedMove->StartMove(TimeStep, SimBlackboard);
			}
			else
			{
				// We should've populated missing logic before this so let's just clear this and warn about it
				UE_LOG(LogBulletMover, Warning, TEXT("Queued Active Move (%s) logic was not present. Move will not be activated."), *QueuedMove->GetDataStructType()->GetName());
			}
		}
	}
	
	QueuedMoves.Empty();
}

void FBulletLayeredMoveInstanceGroup::ProcessFinishedMove(const FBulletLayeredMoveInstance& Move, bool& bResidualVelocityOverridden, bool& bClampVelocityOverridden)
{
	const FBulletLayeredMoveFinishVelocitySettings& FinishVelocitySettings = Move.GetFinishVelocitySettings();
	const EBulletMoveMixMode MixMode = Move.GetMixMode();
	
	if (FinishVelocitySettings.FinishVelocityMode == EBulletLayeredMoveFinishVelocityMode::SetVelocity)
	{
		bApplyResidualVelocity = true;

		switch (MixMode)
		{
		case EBulletMoveMixMode::AdditiveVelocity:
			if (!bResidualVelocityOverridden)
			{
				ResidualVelocity += FinishVelocitySettings.SetVelocity;
			}
			break;
		case EBulletMoveMixMode::OverrideVelocity:
		case EBulletMoveMixMode::OverrideAll:
			{
				UE_CLOG(bClampVelocityOverridden, LogBulletMover, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
				bResidualVelocityOverridden = true;
				ResidualVelocity = FinishVelocitySettings.SetVelocity;
			}
			break;
		}
	}
	else if (FinishVelocitySettings.FinishVelocityMode == EBulletLayeredMoveFinishVelocityMode::ClampVelocity)
	{
		switch (MixMode)
		{
		case EBulletMoveMixMode::AdditiveVelocity:
			if (!bClampVelocityOverridden)
			{
				if (ResidualClamping < 0)
				{
					ResidualClamping = FinishVelocitySettings.ClampVelocity;
				}
				else if (ResidualClamping > FinishVelocitySettings.ClampVelocity)
				{
					// No way to really add clamping so we instead apply it if it was smaller
					ResidualClamping = FinishVelocitySettings.ClampVelocity;
				}
			}
			break;
		case EBulletMoveMixMode::OverrideVelocity:
		case EBulletMoveMixMode::OverrideAll:
			{
				UE_CLOG(bClampVelocityOverridden, LogBulletMover, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
				bClampVelocityOverridden = true;
				ResidualClamping = FinishVelocitySettings.ClampVelocity;
			}
			break;
		}
	}
}
