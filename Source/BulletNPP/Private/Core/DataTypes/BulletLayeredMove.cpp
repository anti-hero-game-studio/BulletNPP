// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/DataTypes/BulletLayeredMove.h"
#include "BulletLogChannels.h"
#include "BulletNPP.h"
#include "Core/DataTypes/BulletPhysicsTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletLayeredMove)

const double LayeredMove_InvalidTime = -UE_BIG_NUMBER;

void FBulletLayeredMoveFinishVelocitySettings::NetSerialize(FArchive& Ar)
{
	uint8 bHasFinishVelocitySettings = Ar.IsSaving() ? 0 : (FinishVelocityMode == EBulletLayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity);
	Ar.SerializeBits(&bHasFinishVelocitySettings, 1);

	if (bHasFinishVelocitySettings)
	{
		uint8 FinishVelocityModeAsU8 = (uint8)(FinishVelocityMode);
		Ar << FinishVelocityModeAsU8;
		FinishVelocityMode = (EBulletLayeredMoveFinishVelocityMode)FinishVelocityModeAsU8;

		if (FinishVelocityMode == EBulletLayeredMoveFinishVelocityMode::SetVelocity)
		{
			Ar << SetVelocity;
		}
		else if (FinishVelocityMode == EBulletLayeredMoveFinishVelocityMode::ClampVelocity)
		{
			Ar << ClampVelocity;
		}
	}
}

FBulletLayeredMoveBase::FBulletLayeredMoveBase() :
	MixMode(EBulletMoveMixMode::AdditiveVelocity),
	Priority(0),
	DurationMs(-1.f),
	StartSimTimeMs(LayeredMove_InvalidTime)
{
}


void FBulletLayeredMoveBase::StartMove(const UBulletPhysicsEngineSimComp* BulletComp, UBulletBlackboard* SimBlackboard, double CurrentSimTimeMs)
{
	StartSimTimeMs = CurrentSimTimeMs;
	OnStart(BulletComp, SimBlackboard);
}

void FBulletLayeredMoveBase::StartMove_Async(UBulletBlackboard* SimBlackboard, double CurrentSimTimeMs)
{
	StartSimTimeMs = CurrentSimTimeMs;
	OnStart_Async(SimBlackboard);
}

bool FBulletLayeredMoveBase::GenerateMove_Async(const FBulletTickStartData& StartState, const FBulletTimeStep& TimeStep, UBulletBlackboard* SimBlackboard, FBulletProposedMove& OutProposedMove)
{
	ensureMsgf(false, TEXT("GenerateMove_Async is not implemented"));
	return false;
}

bool FBulletLayeredMoveBase::IsFinished(double CurrentSimTimeMs) const
{
	const bool bHasStarted = (StartSimTimeMs >= 0.0);
	const bool bTimeExpired = bHasStarted && (DurationMs > 0.f) && (StartSimTimeMs + DurationMs <= CurrentSimTimeMs);
	const bool bDidTickOnceAndExpire = bHasStarted && (DurationMs == 0.f);

	return bTimeExpired || bDidTickOnceAndExpire;
}

void FBulletLayeredMoveBase::EndMove(const UBulletPhysicsEngineSimComp* BulletComp, UBulletBlackboard* SimBlackboard, double CurrentSimTimeMs)
{
	OnEnd(BulletComp, SimBlackboard, CurrentSimTimeMs);
}

void FBulletLayeredMoveBase::EndMove_Async(UBulletBlackboard* SimBlackboard, double CurrentSimTimeMs)
{
	OnEnd_Async(SimBlackboard, CurrentSimTimeMs);
}

FBulletLayeredMoveBase* FBulletLayeredMoveBase::Clone() const
{
	// If child classes don't override this, saved moves will not work
	checkf(false, TEXT("FBulletLayeredMoveBase::Clone() being called erroneously from %s. A FBulletLayeredMoveBase should never be queued directly and Clone should always be overridden in child structs!"), *GetNameSafe(GetScriptStruct()));
	return nullptr;
}


void FBulletLayeredMoveBase::NetSerialize(FArchive& Ar)
{
	uint8 MixModeAsU8 = (uint8)MixMode;
	Ar << MixModeAsU8;
	MixMode = (EBulletMoveMixMode)MixModeAsU8;

	uint8 bHasDefaultPriority = Priority == 0;
	Ar.SerializeBits(&bHasDefaultPriority, 1);
	if (!bHasDefaultPriority)
	{
		Ar << Priority;
	}
	
	Ar << DurationMs;
	Ar << StartSimTimeMs;

	FinishVelocitySettings.NetSerialize(Ar);
}


UScriptStruct* FBulletLayeredMoveBase::GetScriptStruct() const
{
	return FBulletLayeredMoveBase::StaticStruct();
}


FString FBulletLayeredMoveBase::ToSimpleString() const
{
	return GetScriptStruct()->GetName();
}


FBulletLayeredMoveGroup::FBulletLayeredMoveGroup()
	: ResidualVelocity(FVector::Zero())
	, ResidualClamping(-1.f)
	, bApplyResidualVelocity(false)
{
}


void FBulletLayeredMoveGroup::QueueLayeredMove(TSharedPtr<FBulletLayeredMoveBase> Move)
{
	if (ensure(Move.IsValid()))
	{
		QueuedLayeredMoves.Add(Move);
		UE_LOG(LogBullet, VeryVerbose, TEXT("LayeredMove queued move (%s)"), *Move->ToSimpleString());
	}
}

void FBulletLayeredMoveGroup::CancelMovesByTag(FGameplayTag Tag, bool bRequireExactMatch)
{

	// Schedule a tag cancellation request, to be handled during simulation
	TagCancellationRequests.Add(TPair<FGameplayTag, bool>(Tag, bRequireExactMatch));

}

TArray<TSharedPtr<FBulletLayeredMoveBase>> FBulletLayeredMoveGroup::GenerateActiveMoves(const FBulletTimeStep& TimeStep, const UBulletPhysicsEngineSimComp* BulletComp, UBulletBlackboard* SimBlackboard)
{
	const double SimStartTimeMs		= TimeStep.BaseSimTimeMs;
	const double SimTimeAfterTickMs	= SimStartTimeMs + TimeStep.StepMs;

	FlushMoveArrays(BulletComp, SimBlackboard, SimStartTimeMs, /*bIsAsync =*/ false);

	return ActiveLayeredMoves;
}

TArray<TSharedPtr<FBulletLayeredMoveBase>> FBulletLayeredMoveGroup::GenerateActiveMoves_Async(const FBulletTimeStep& TimeStep, UBulletBlackboard* SimBlackboard)
{
	const double SimStartTimeMs		= TimeStep.BaseSimTimeMs;
	const double SimTimeAfterTickMs	= SimStartTimeMs + TimeStep.StepMs;

	FlushMoveArrays(nullptr, SimBlackboard, SimStartTimeMs, /*bIsAsync =*/ true);

	return ActiveLayeredMoves;
}

void FBulletLayeredMoveGroup::NetSerialize(FArchive& Ar, uint8 MaxNumMovesToSerialize/* = MAX_uint8*/)
{
	// TODO: Warn if some sources will be dropped
	const uint8 NumActiveMovesToSerialize = FMath::Min<int32>(ActiveLayeredMoves.Num(), MaxNumMovesToSerialize);
	const uint8 NumQueuedMovesToSerialize = NumActiveMovesToSerialize < MaxNumMovesToSerialize ? MaxNumMovesToSerialize - NumActiveMovesToSerialize : 0;
	NetSerializeLayeredMovesArray(Ar, ActiveLayeredMoves, NumActiveMovesToSerialize);
	NetSerializeLayeredMovesArray(Ar, QueuedLayeredMoves, NumQueuedMovesToSerialize);
}


static void CopyLayeredMoveArray(TArray<TSharedPtr<FBulletLayeredMoveBase>>& Dest, const TArray<TSharedPtr<FBulletLayeredMoveBase>>& Src)
{
	bool bCanCopyInPlace = (BulletPhysicsEngine::DisableDataCopyInPlace == 0 && Dest.Num() == Src.Num());
	if (bCanCopyInPlace)
	{
		// If copy in place is enabled and the arrays are the same size, copy by index
		for (int32 i = 0; i < Dest.Num(); ++i)
		{
			if (FBulletLayeredMoveBase* SrcData = Src[i].Get())
			{
				FBulletLayeredMoveBase* DestData = Dest[i].Get();
				UScriptStruct* SourceStruct = SrcData->GetScriptStruct();

				if (DestData && SourceStruct == DestData->GetScriptStruct())
				{
					// Same type so copy in place
					SourceStruct->CopyScriptStruct(DestData, SrcData, 1);
				}
				else
				{
					// Different type so replace the shared ptr with a clone
					Dest[i] = TSharedPtr<FBulletLayeredMoveBase>(SrcData->Clone());
				}
			}
			else
			{
				// Found invalid source, fall back to full copy
				bCanCopyInPlace = false;
				break;
			}
		}
	}
	
	if (!bCanCopyInPlace)
	{
		// Deep copy active moves
		Dest.Empty(Src.Num());
		for (int i = 0; i < Src.Num(); ++i)
		{
			if (Src[i].IsValid())
			{
				FBulletLayeredMoveBase* CopyOfSourcePtr = Src[i]->Clone();
				Dest.Add(TSharedPtr<FBulletLayeredMoveBase>(CopyOfSourcePtr));
			}
			else
			{
				UE_LOG(LogBullet, Warning, TEXT("CopyLayeredMoveArray trying to copy invalid Other Layered Move"));
			}
		}
	}
}


FBulletLayeredMoveGroup& FBulletLayeredMoveGroup::operator=(const FBulletLayeredMoveGroup& Other)
{
	// Perform deep copy of this Group
	if (this != &Other)
	{
		CopyLayeredMoveArray(ActiveLayeredMoves, Other.ActiveLayeredMoves);
		CopyLayeredMoveArray(QueuedLayeredMoves, Other.QueuedLayeredMoves);

		TagCancellationRequests = Other.TagCancellationRequests;
	}

	return *this;
}

bool FBulletLayeredMoveGroup::operator==(const FBulletLayeredMoveGroup& Other) const
{
	// Deep move-by-move comparison
	if (ActiveLayeredMoves.Num() != Other.ActiveLayeredMoves.Num())
	{
		return false;
	}
	if (QueuedLayeredMoves.Num() != Other.QueuedLayeredMoves.Num())
	{
		return false;
	}


	for (int32 i = 0; i < ActiveLayeredMoves.Num(); ++i)
	{
		if (ActiveLayeredMoves[i].IsValid() == Other.ActiveLayeredMoves[i].IsValid())
		{
			if (ActiveLayeredMoves[i].IsValid())
			{
				// TODO: Implement deep equality checks
				// 				if (!ActiveLayeredMoves[i]->MatchesAndHasSameState(Other.ActiveLayeredMoves[i].Get()))
				// 				{
				// 					return false; // They're valid and don't match/have same state
				// 				}
			}
		}
		else
		{
			return false; // Mismatch in validity
		}
	}
	for (int32 i = 0; i < QueuedLayeredMoves.Num(); ++i)
	{
		if (QueuedLayeredMoves[i].IsValid() == Other.QueuedLayeredMoves[i].IsValid())
		{
			if (QueuedLayeredMoves[i].IsValid())
			{
				// TODO: Implement deep equality checks
				// 				if (!QueuedLayeredMoves[i]->MatchesAndHasSameState(Other.QueuedLayeredMoves[i].Get()))
				// 				{
				// 					return false; // They're valid and don't match/have same state
				// 				}
			}
		}
		else
		{
			return false; // Mismatch in validity
		}
	}
	return true;
}

bool FBulletLayeredMoveGroup::operator!=(const FBulletLayeredMoveGroup& Other) const
{
	return !(FBulletLayeredMoveGroup::operator==(Other));
}

bool FBulletLayeredMoveGroup::HasSameContents(const FBulletLayeredMoveGroup& Other) const
{
	// Only compare the types of moves contained, not the state
	if (ActiveLayeredMoves.Num() != Other.ActiveLayeredMoves.Num() ||
		QueuedLayeredMoves.Num() != Other.QueuedLayeredMoves.Num())
	{
		return false;
	}

	for (int32 i = 0; i < ActiveLayeredMoves.Num(); ++i)
	{
		if (ActiveLayeredMoves[i]->GetScriptStruct() != Other.ActiveLayeredMoves[i]->GetScriptStruct())
		{
			return false;
		}
	}

	for (int32 i = 0; i < QueuedLayeredMoves.Num(); ++i)
	{
		if (QueuedLayeredMoves[i]->GetScriptStruct() != Other.QueuedLayeredMoves[i]->GetScriptStruct())
		{
			return false;
		}
	}

	return true;
}


void FBulletLayeredMoveGroup::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (const TSharedPtr<FBulletLayeredMoveBase>& LayeredMove : ActiveLayeredMoves)
	{
		if (LayeredMove.IsValid())
		{
			LayeredMove->AddReferencedObjects(Collector);
		}
	}

	for (const TSharedPtr<FBulletLayeredMoveBase>& LayeredMove : QueuedLayeredMoves)
	{
		if (LayeredMove.IsValid())
		{
			LayeredMove->AddReferencedObjects(Collector);
		}
	}
}

FString FBulletLayeredMoveGroup::ToSimpleString() const
{
	return FString::Printf(TEXT("FBulletLayeredMoveGroup. Active: %i Queued: %i"), ActiveLayeredMoves.Num(), QueuedLayeredMoves.Num());
}

const FBulletLayeredMoveBase* FBulletLayeredMoveGroup::FindActiveMove(const UScriptStruct* LayeredMoveStructType) const
{
	for (const TSharedPtr<FBulletLayeredMoveBase>& ActiveMove : ActiveLayeredMoves)
	{
		if (ActiveMove && ActiveMove->GetScriptStruct()->IsChildOf(LayeredMoveStructType))
		{
			return ActiveMove.Get();
		}
	}
	return nullptr;
}

const FBulletLayeredMoveBase* FBulletLayeredMoveGroup::FindQueuedMove(const UScriptStruct* LayeredMoveStructType) const
{
	for (const TSharedPtr<FBulletLayeredMoveBase>& QueuedMove : QueuedLayeredMoves)
	{
		if (QueuedMove && QueuedMove->GetScriptStruct()->IsChildOf(LayeredMoveStructType))
		{
			return QueuedMove.Get();
		}
	}
	return nullptr;
}

void FBulletLayeredMoveGroup::FlushMoveArrays(const UBulletPhysicsEngineSimComp* BulletComp, UBulletBlackboard* SimBlackboard, double CurrentSimTimeMs, bool bIsAsync)
{
	if (bIsAsync)
	{
		ensureMsgf(BulletComp == nullptr, TEXT("In async mode, no Bullet Component should be passed in as argument in FBulletLayeredMoveGroup::FlushMoveArrays"));
		BulletComp = nullptr;
	}

	bool bResidualVelocityOverridden = false;
	bool bClampVelocityOverridden = false;
	
	// Process any cancellations
	{
		for (TPair<FGameplayTag, bool> CancelRequest : TagCancellationRequests)
		{
			const FGameplayTag TagToMatch = CancelRequest.Key;
			const bool bRequireExactMatch = CancelRequest.Value;

			QueuedLayeredMoves.RemoveAll([TagToMatch, bRequireExactMatch](const TSharedPtr<FBulletLayeredMoveBase>& Move)
				{
					return (Move.IsValid() && Move->HasGameplayTag(TagToMatch, bRequireExactMatch));
				});

			ActiveLayeredMoves.RemoveAll([BulletComp, SimBlackboard, CurrentSimTimeMs, &bResidualVelocityOverridden, &bClampVelocityOverridden, bIsAsync, TagToMatch, bRequireExactMatch, this] (const TSharedPtr<FBulletLayeredMoveBase>& Move)
				{
					if (Move.IsValid() && Move->HasGameplayTag(TagToMatch, bRequireExactMatch))
					{
						GatherResidualVelocitySettings(Move, bResidualVelocityOverridden, bClampVelocityOverridden);
						if (bIsAsync)
						{
							Move->EndMove_Async(SimBlackboard, CurrentSimTimeMs);
						}
						else
						{
							Move->EndMove(BulletComp, SimBlackboard, CurrentSimTimeMs);
						}
						return true;
					}

					return false;
				});
		}

		TagCancellationRequests.Empty();
	}

	
	// Remove any finished moves
	ActiveLayeredMoves.RemoveAll([BulletComp, SimBlackboard, CurrentSimTimeMs, &bResidualVelocityOverridden, &bClampVelocityOverridden, bIsAsync, this]
		(const TSharedPtr<FBulletLayeredMoveBase>& Move)
		{
			if (Move.IsValid())
			{
				if (Move->IsFinished(CurrentSimTimeMs))
				{
					GatherResidualVelocitySettings(Move, bResidualVelocityOverridden, bClampVelocityOverridden);
					if (bIsAsync)
					{
						Move->EndMove_Async(SimBlackboard, CurrentSimTimeMs);
					}
					else
					{
						Move->EndMove(BulletComp, SimBlackboard, CurrentSimTimeMs);
					}
					return true;
				}
			}
			else
			{
				return true;	
			}

			return false;
		});

	// Make any queued moves active
	for (TSharedPtr<FBulletLayeredMoveBase>& QueuedMove : QueuedLayeredMoves)
	{
		ActiveLayeredMoves.Add(QueuedMove);
		if (bIsAsync)
		{
			QueuedMove->StartMove_Async(SimBlackboard, CurrentSimTimeMs);
		}
		else
		{
			QueuedMove->StartMove(BulletComp, SimBlackboard, CurrentSimTimeMs);
		}
	}

	QueuedLayeredMoves.Empty();
}

void FBulletLayeredMoveGroup::GatherResidualVelocitySettings(const TSharedPtr<FBulletLayeredMoveBase>& Move, bool& bResidualVelocityOverridden, bool& bClampVelocityOverridden)
{
	if (Move->FinishVelocitySettings.FinishVelocityMode == EBulletLayeredMoveFinishVelocityMode::SetVelocity)
	{
		if (Move->MixMode == EBulletMoveMixMode::OverrideVelocity)
		{
			if (bResidualVelocityOverridden)
			{
				UE_LOG(LogBullet, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
			}

			bResidualVelocityOverridden = true;
			ResidualVelocity = Move->FinishVelocitySettings.SetVelocity;
		}
		else if (Move->MixMode == EBulletMoveMixMode::AdditiveVelocity && !bResidualVelocityOverridden)
		{
			ResidualVelocity += Move->FinishVelocitySettings.SetVelocity;
		}
		else if (Move->MixMode == EBulletMoveMixMode::OverrideAll)
		{
			if (bResidualVelocityOverridden)
			{
				UE_LOG(LogBullet, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
			}

			bResidualVelocityOverridden = true;
			ResidualVelocity = Move->FinishVelocitySettings.SetVelocity;
		}
		else
		{
			check(0);	// unhandled case
		}
		bApplyResidualVelocity = true;
	}
	else if (Move->FinishVelocitySettings.FinishVelocityMode == EBulletLayeredMoveFinishVelocityMode::ClampVelocity)
	{
		if (Move->MixMode == EBulletMoveMixMode::OverrideVelocity)
		{
			if (bClampVelocityOverridden)
			{
				UE_LOG(LogBullet, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
			}

			bClampVelocityOverridden = true;
			ResidualClamping = Move->FinishVelocitySettings.ClampVelocity;
		}
		else if (Move->MixMode == EBulletMoveMixMode::AdditiveVelocity && !bClampVelocityOverridden)
		{
			if (ResidualClamping < 0)
			{
				ResidualClamping = Move->FinishVelocitySettings.ClampVelocity;
			}
			// No way to really add clamping so we instead apply it if it was smaller
			else if (ResidualClamping > Move->FinishVelocitySettings.ClampVelocity)
			{
				ResidualClamping = Move->FinishVelocitySettings.ClampVelocity;
			}
		}
		else if (Move->MixMode == EBulletMoveMixMode::OverrideAll)
		{
			if (bClampVelocityOverridden)
			{
				UE_LOG(LogBullet, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
			}

			bClampVelocityOverridden = true;
			ResidualClamping = Move->FinishVelocitySettings.ClampVelocity;
		}
		else
		{
			check(0);	// unhandled case
		}
	}
}

struct FBulletLayeredMoveDeleter
{
	FORCEINLINE void operator()(FBulletLayeredMoveBase* Object) const
	{
		check(Object);
		UScriptStruct* ScriptStruct = Object->GetScriptStruct();
		check(ScriptStruct);
		ScriptStruct->DestroyStruct(Object);
		FMemory::Free(Object);
	}
};


/* static */ void FBulletLayeredMoveGroup::NetSerializeLayeredMovesArray(FArchive& Ar, TArray< TSharedPtr<FBulletLayeredMoveBase> >& LayeredMovesArray, uint8 MaxNumLayeredMovesToSerialize /*=MAX_uint8*/)
{
	uint8 NumMovesToSerialize;
	if (Ar.IsSaving())
	{
		UE_CLOG(LayeredMovesArray.Num() > MaxNumLayeredMovesToSerialize, LogBullet, Warning, TEXT("Too many Layered Moves (%d!) to net serialize. Clamping to %d"),
			LayeredMovesArray.Num(), MaxNumLayeredMovesToSerialize);

		NumMovesToSerialize = FMath::Min<int32>(LayeredMovesArray.Num(), MaxNumLayeredMovesToSerialize);
	}

	Ar << NumMovesToSerialize;

	if (Ar.IsLoading())
	{
		LayeredMovesArray.SetNumZeroed(NumMovesToSerialize);
	}

	for (int32 i = 0; i < NumMovesToSerialize && !Ar.IsError(); ++i)
	{
		TCheckedObjPtr<UScriptStruct> ScriptStruct = LayeredMovesArray[i].IsValid() ? LayeredMovesArray[i]->GetScriptStruct() : nullptr;
		UScriptStruct* ScriptStructLocal = ScriptStruct.Get();
		Ar << ScriptStruct;

		if (ScriptStruct.IsValid())
		{
			// Restrict replication to derived classes of FBulletLayeredMoveBase for security reasons:
			// If FBulletLayeredMoveGroup is replicated through a Server RPC, we need to prevent clients from sending us
			// arbitrary ScriptStructs due to the allocation/reliance on GetCppStructOps below which could trigger a server crash
			// for invalid structs. All provided sources are direct children of FBulletLayeredMoveBase and we never expect to have deep hierarchies
			// so this should not be too costly
			bool bIsDerivedFromBase = false;
			UStruct* CurrentSuperStruct = ScriptStruct->GetSuperStruct();
			while (CurrentSuperStruct)
			{
				if (CurrentSuperStruct == FBulletLayeredMoveBase::StaticStruct())
				{
					bIsDerivedFromBase = true;
					break;
				}
				CurrentSuperStruct = CurrentSuperStruct->GetSuperStruct();
			}

			if (bIsDerivedFromBase)
			{
				if (Ar.IsLoading())
				{
					if (LayeredMovesArray[i].IsValid() && ScriptStructLocal == ScriptStruct.Get())
					{
						// What we have locally is the same type as we're being serialized into, so we don't need to
						// reallocate - just use existing structure
					}
					else
					{
						// For now, just reset/reallocate the data when loading.
						// Longer term if we want to generalize this and use it for property replication, we should support
						// only reallocating when necessary
						FBulletLayeredMoveBase* NewMove = (FBulletLayeredMoveBase*)FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
						ScriptStruct->InitializeStruct(NewMove);

						LayeredMovesArray[i] = TSharedPtr<FBulletLayeredMoveBase>(NewMove, FBulletLayeredMoveDeleter());
					}
				}

				LayeredMovesArray[i]->NetSerialize(Ar);
			}
			else
			{
				UE_LOG(LogBullet, Error, TEXT("FBulletLayeredMoveGroup::NetSerialize: ScriptStruct not derived from FBulletLayeredMoveBase attempted to serialize."));
				Ar.SetError();
				break;
			}
		}
		else if (ScriptStruct.IsError())
		{
			UE_LOG(LogBullet, Error, TEXT("FBulletLayeredMoveGroup::NetSerialize: Invalid ScriptStruct serialized."));
			Ar.SetError();
			break;
		}
	}
}

void FBulletLayeredMoveGroup::ResetResidualVelocity()
{
	bApplyResidualVelocity = false;
	ResidualVelocity = FVector::ZeroVector;
	ResidualClamping = -1.f;
}

void FBulletLayeredMoveGroup::Reset()
{
	ResetResidualVelocity();
	QueuedLayeredMoves.Empty();
	ActiveLayeredMoves.Empty();
	TagCancellationRequests.Empty();
}

