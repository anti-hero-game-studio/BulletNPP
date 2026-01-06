// Copyright Epic Games, Inc. All Rights Reserved.


#include "MoveLibrary/BulletRollbackBlackboard.h"
#include "BulletMoverTypes.h"
#include "GenericPlatform/GenericPlatformMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletRollbackBlackboard)


uint32 UBulletRollbackBlackboard::BlackboardEntryBase::ComputeBufferSize(EBulletBlackboardSizingPolicy SizingPolicy, uint32 FixedBufferSize)
{
	uint32 BufferSize = 2;

	// TODO: handle these special cases:
	//     if we are in a non-networked situation w/ non-async simulation, we only need 1 entry slot
	//     if we are in a non-networked situation w/ async simulation, we only need 2 entry slots (one for simulating forwarD)

	switch (SizingPolicy)
	{
		case EBulletBlackboardSizingPolicy::FixedDeclaredSize:
			// TODO: Warn if FixedBufferSize is invalid
			BufferSize = FMath::Max(BufferSize, FixedBufferSize);
			break;

		default:	//TODO: warn about an unhandled policy type
			break;
	}

	return BufferSize;
}

UBulletRollbackBlackboard::BlackboardEntryBase::BlackboardEntryBase(const EntrySettings& InSettings, uint32 BufferSize)
	: Settings(InSettings)
	, Timestamps(BufferSize, EntryTimeStamp())
	, ExternalIdx(0)
	, InternalIdx(0)
{
}

void UBulletRollbackBlackboard::BlackboardEntryBase::RollBack(uint32 NewPendingFrame)
{
	// Goal: adjust entry to point at the value from the prior frame. May make the entry invalidated, if there were no values that old.
	check(ExternalIdx == InternalIdx);

	const uint32 LowestPossibleIdx = (ExternalIdx > Timestamps.Capacity()) ? (ExternalIdx - Timestamps.Capacity()) : 0u;

	// Walk downwards to find the highest index with a frame < NewPendingFrame. 
	// If we hit uint32::max, that indicates we wrapped around from 0 and we're out of slots to check.
	for (uint32 IdxToCheck = ExternalIdx; IdxToCheck >= LowestPossibleIdx && IdxToCheck != TNumericLimits<uint32>::Max(); --IdxToCheck)
	{
		if (Timestamps[IdxToCheck].IsValid() && Timestamps[IdxToCheck].Frame < NewPendingFrame)
		{
			ExternalIdx = InternalIdx = IdxToCheck;
			return;
		}
	}

	// If we made it here, then there are no entries that weren't rolled back, so let's make it clear
	Timestamps[ExternalIdx].Invalidate();


}



bool UBulletRollbackBlackboard::BlackboardEntryBase::CanReadEntryAt(const EntryTimeStamp& ReaderTimeStamp, EEntryIndexType IndexType) const
{
	const uint32 TimestampIdx = IndexType == EEntryIndexType::External ? ExternalIdx : InternalIdx;

	if (!Timestamps[TimestampIdx].IsValid())
	{
		return false;	// entry isn't initialized yet or never set
	}

	switch (Settings.PersistencePolicy)
	{
		case EBulletBlackboardPersistencePolicy::NextFrameOnly:
		{
			// if value wasn't last set during the current or immediate prior sim frame, then it can't be read
			if (!(ReaderTimeStamp.Frame == Timestamps[TimestampIdx].Frame ||
				  ReaderTimeStamp.Frame == Timestamps[TimestampIdx].Frame+1))
			{
				return false;
			}
		}
		break;

		case EBulletBlackboardPersistencePolicy::Forever:		// Always allow reading
			break;

		default:		// Allow reading of any not-yet-implemented policies
			break;
	}

	return true;
}



void UBulletRollbackBlackboard::BeginSimulationFrame(const FBulletMoverTimeStep& PendingTimeStep)
{
	check(!bIsSimulationInProgress && !bIsRollbackInProgress);
	InProgressSimFrameThreadId = FPlatformTLS::GetCurrentThreadId();
	bIsSimulationInProgress = true;
	bIsResimulating = PendingTimeStep.bIsResimulating;

	InProgressSimTimeStamp.TimeMs = PendingTimeStep.BaseSimTimeMs;
	InProgressSimTimeStamp.Frame = PendingTimeStep.ServerFrame;
}

void UBulletRollbackBlackboard::EndSimulationFrame()
{
	check(bIsSimulationInProgress && (InProgressSimFrameThreadId == FPlatformTLS::GetCurrentThreadId()));
	bIsSimulationInProgress = bIsResimulating = false;

	// TODO: need some kind of lock so we wait on any in-progress operations before advancing the CurrentSimTimeStamp
	CurrentSimTimeStamp = InProgressSimTimeStamp;


	// TODO: could we skip this if no "set" operations occurred? Consider different policies, like change-every-frame entries

	for (const TPair<FName, TUniquePtr<BlackboardEntryBase>>& KVP : EntryMap)
	{
		KVP.Value->OnSimulationFrameEnd();
	}
}


void UBulletRollbackBlackboard::BeginRollback(const FBulletMoverTimeStep& NewBaseTimeStep)
{
	check(!bIsSimulationInProgress && !bIsRollbackInProgress);
	InRollbackThreadId = FPlatformTLS::GetCurrentThreadId();
	bIsRollbackInProgress = true;

	UE_LOG(LogBulletMover, Verbose, TEXT("Blackboard begin rollback. From Sim F %i / T %.3f -> F %i / T %.3f"), 
		CurrentSimTimeStamp.Frame, CurrentSimTimeStamp.TimeMs, NewBaseTimeStep.ServerFrame, NewBaseTimeStep.BaseSimTimeMs);

	// TODO: Need locking mechanism

	const EntryTimeStamp NewBaseTimeStamp((double)NewBaseTimeStep.BaseSimTimeMs, NewBaseTimeStep.ServerFrame);

	for (const TPair<FName, TUniquePtr<BlackboardEntryBase>>& KVP : EntryMap)
	{
		KVP.Value->RollBack(NewBaseTimeStamp.Frame);
	}

	// As the rollback occurs, we need to pull back the timestamps to match
	CurrentSimTimeStamp = InProgressSimTimeStamp = NewBaseTimeStamp;
}

void UBulletRollbackBlackboard::EndRollback()
{
	check (bIsRollbackInProgress && (InRollbackThreadId == FPlatformTLS::GetCurrentThreadId()));
	bIsRollbackInProgress = false;
}


void UBulletRollbackBlackboard_InternalWrapper::BeginSimulationFrame(const FBulletMoverTimeStep& PendingTimeStep)
{
	Blackboard->BeginSimulationFrame(PendingTimeStep);
}

void UBulletRollbackBlackboard_InternalWrapper::EndSimulationFrame()
{
	Blackboard->EndSimulationFrame();
}

void UBulletRollbackBlackboard_InternalWrapper::BeginRollback(const FBulletMoverTimeStep& NewBaseTimeStep)
{
	Blackboard->BeginRollback(NewBaseTimeStep);
}

void UBulletRollbackBlackboard_InternalWrapper::EndRollback()
{
	Blackboard->EndRollback();
}
