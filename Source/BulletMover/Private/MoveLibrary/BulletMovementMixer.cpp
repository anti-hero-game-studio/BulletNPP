// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/BulletMovementMixer.h"
#include "BulletLayeredMove.h"
#include "BulletLayeredMoveBase.h"
#include "BulletMoverLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMovementMixer)

UBulletMovementMixer::UBulletMovementMixer()
	: CurrentHighestPriority(0)
	, CurrentLayeredMoveStartTime(TNumericLimits<float>::Max())
{
}

void UBulletMovementMixer::MixLayeredMove(const FBulletLayeredMoveBase& ActiveMove, const FBulletProposedMove& MoveStep, FBulletProposedMove& OutCumulativeMove)
{
	if (OutCumulativeMove.PreferredMode != MoveStep.PreferredMode && !OutCumulativeMove.PreferredMode.IsNone() && !MoveStep.PreferredMode.IsNone())
	{
		UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves are conflicting with preferred moves. %s will override %s"),
			*MoveStep.PreferredMode.ToString(), *OutCumulativeMove.PreferredMode.ToString());
	}

	if (MoveStep.bHasDirIntent && OutCumulativeMove.MixMode != EBulletMoveMixMode::OverrideAll && ActiveMove.Priority >= CurrentHighestPriority)
	{
		if (OutCumulativeMove.bHasDirIntent)
		{
			UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves are setting direction intent and the layered move with highest priority will be used."));
		}
				
		OutCumulativeMove.bHasDirIntent = MoveStep.bHasDirIntent;
		OutCumulativeMove.DirectionIntent = MoveStep.DirectionIntent;
	}

	if (MoveStep.MixMode == EBulletMoveMixMode::OverrideVelocity)
	{
		if (CheckPriority(&ActiveMove, CurrentHighestPriority, CurrentLayeredMoveStartTime))
		{
			if (OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideVelocity || OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideAll)
			{
				UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves with Override mix mode are active simultaneously. Layered move with the highest priority will take effect."));
			}

			if (!MoveStep.PreferredMode.IsNone() && OutCumulativeMove.MixMode != EBulletMoveMixMode::OverrideAll)
			{
				OutCumulativeMove.PreferredMode = MoveStep.PreferredMode;
			}
				
			OutCumulativeMove.MixMode = EBulletMoveMixMode::OverrideVelocity;
			OutCumulativeMove.LinearVelocity  = MoveStep.LinearVelocity;
			OutCumulativeMove.AngularVelocityDegrees = MoveStep.AngularVelocityDegrees;
		}
	}
	else if (MoveStep.MixMode == EBulletMoveMixMode::AdditiveVelocity)
	{
		if (OutCumulativeMove.MixMode != EBulletMoveMixMode::OverrideVelocity && OutCumulativeMove.MixMode != EBulletMoveMixMode::OverrideAll)
		{
			if (!MoveStep.PreferredMode.IsNone())
			{
				OutCumulativeMove.PreferredMode = MoveStep.PreferredMode;
			}

			OutCumulativeMove.LinearVelocity += MoveStep.LinearVelocity;
			OutCumulativeMove.AngularVelocityDegrees += MoveStep.AngularVelocityDegrees;
		}
	}
	else if (MoveStep.MixMode == EBulletMoveMixMode::OverrideAll)
	{
		if (CheckPriority(&ActiveMove, CurrentHighestPriority, CurrentLayeredMoveStartTime))
		{
			if (OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideVelocity || OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideAll)
			{
				UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves with Override mix mode are active simultaneously. Layered move with the highest priority will take effect."));
			}
				
			OutCumulativeMove = MoveStep;
			OutCumulativeMove.MixMode = EBulletMoveMixMode::OverrideAll;
		}
	}
	else if (MoveStep.MixMode == EBulletMoveMixMode::OverrideAllExceptVerticalVelocity)
	{
		if (CheckPriority(&ActiveMove, CurrentHighestPriority, CurrentLayeredMoveStartTime))
		{
			if (OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideVelocity || OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideAll || OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideAllExceptVerticalVelocity)
			{
				UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves with Override mix mode are active simultaneously. Layered move with the highest priority will take effect."));
			}

			OutCumulativeMove = MoveStep;
			OutCumulativeMove.MixMode = EBulletMoveMixMode::OverrideAllExceptVerticalVelocity;
		}
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled move mix mode was found."));
	}
}

void UBulletMovementMixer::MixLayeredMove(const FBulletLayeredMoveInstance& ActiveMove, const FBulletProposedMove& MoveStep, FBulletProposedMove& OutCumulativeMove)
{
	if (OutCumulativeMove.PreferredMode != MoveStep.PreferredMode && !OutCumulativeMove.PreferredMode.IsNone() && !MoveStep.PreferredMode.IsNone())
	{
		UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves are conflicting with preferred moves. %s will override %s"),
			*MoveStep.PreferredMode.ToString(), *OutCumulativeMove.PreferredMode.ToString());
	}

	if (MoveStep.bHasDirIntent && OutCumulativeMove.MixMode != EBulletMoveMixMode::OverrideAll && ActiveMove.GetPriority() >= CurrentHighestPriority)
	{
		if (OutCumulativeMove.bHasDirIntent)
		{
			UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves are setting direction intent and the layered move with highest priority will be used."));
		}
				
		OutCumulativeMove.bHasDirIntent = MoveStep.bHasDirIntent;
		OutCumulativeMove.DirectionIntent = MoveStep.DirectionIntent;
	}

	if (MoveStep.MixMode == EBulletMoveMixMode::OverrideVelocity)
	{
		if (CheckPriority(ActiveMove.GetPriority(), ActiveMove.GetStartingTimeMs(), CurrentHighestPriority, CurrentLayeredMoveStartTime))
		{
			if (OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideVelocity || OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideAll)
			{
				UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves with Override mix mode are active simultaneously. Layered move with the highest priority will take effect."));
			}

			if (!MoveStep.PreferredMode.IsNone() && OutCumulativeMove.MixMode != EBulletMoveMixMode::OverrideAll)
			{
				OutCumulativeMove.PreferredMode = MoveStep.PreferredMode;
			}
				
			OutCumulativeMove.MixMode = EBulletMoveMixMode::OverrideVelocity;
			OutCumulativeMove.LinearVelocity  = MoveStep.LinearVelocity;
			OutCumulativeMove.AngularVelocityDegrees = MoveStep.AngularVelocityDegrees;
		}
	}
	else if (MoveStep.MixMode == EBulletMoveMixMode::AdditiveVelocity)
	{
		if (OutCumulativeMove.MixMode != EBulletMoveMixMode::OverrideVelocity && OutCumulativeMove.MixMode != EBulletMoveMixMode::OverrideAll)
		{
			if (!MoveStep.PreferredMode.IsNone())
			{
				OutCumulativeMove.PreferredMode = MoveStep.PreferredMode;
			}

			OutCumulativeMove.LinearVelocity += MoveStep.LinearVelocity;
			OutCumulativeMove.AngularVelocityDegrees += MoveStep.AngularVelocityDegrees;
		}
	}
	else if (MoveStep.MixMode == EBulletMoveMixMode::OverrideAll)
	{
		if (CheckPriority(ActiveMove.GetPriority(), ActiveMove.GetStartingTimeMs(), CurrentHighestPriority, CurrentLayeredMoveStartTime))
		{
			if (OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideVelocity || OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideAll)
			{
				UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves with Override mix mode are active simultaneously. Layered move with the highest priority will take effect."));
			}
				
			OutCumulativeMove = MoveStep;
			OutCumulativeMove.MixMode = EBulletMoveMixMode::OverrideAll;
		}
	}
	else if (MoveStep.MixMode == EBulletMoveMixMode::OverrideAllExceptVerticalVelocity)
	{
		if (CheckPriority(ActiveMove.GetPriority(), ActiveMove.GetStartingTimeMs(), CurrentHighestPriority, CurrentLayeredMoveStartTime))
		{
			if (OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideVelocity || OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideAll || OutCumulativeMove.MixMode == EBulletMoveMixMode::OverrideAllExceptVerticalVelocity)
			{
				UE_LOG(LogBulletMover, Log, TEXT("Multiple LayeredMoves with Override mix mode are active simultaneously. Layered move with the highest priority will take effect."));
			}

			OutCumulativeMove = MoveStep;
			OutCumulativeMove.MixMode = EBulletMoveMixMode::OverrideAllExceptVerticalVelocity;
		}
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled move mix mode was found."));
	}
}

void UBulletMovementMixer::MixProposedMoves(const FBulletProposedMove& MoveToMix, FVector UpDirection, FBulletProposedMove& OutCumulativeMove)
{
	if (MoveToMix.bHasDirIntent && OutCumulativeMove.MixMode != EBulletMoveMixMode::OverrideAll)
	{
		OutCumulativeMove.bHasDirIntent = MoveToMix.bHasDirIntent;
		OutCumulativeMove.DirectionIntent = MoveToMix.DirectionIntent;
	}

	// Combine movement parameters from layered moves into what the mode wants to do
	if (MoveToMix.MixMode == EBulletMoveMixMode::OverrideAll)
	{
		OutCumulativeMove = MoveToMix;
	}
	else if (MoveToMix.MixMode == EBulletMoveMixMode::AdditiveVelocity)
	{
		OutCumulativeMove.LinearVelocity += MoveToMix.LinearVelocity;
		OutCumulativeMove.AngularVelocityDegrees += MoveToMix.AngularVelocityDegrees;
	}
	else if (MoveToMix.MixMode == EBulletMoveMixMode::OverrideVelocity)
	{
		OutCumulativeMove.LinearVelocity = MoveToMix.LinearVelocity;
		OutCumulativeMove.AngularVelocityDegrees = MoveToMix.AngularVelocityDegrees;
	}
	else if (MoveToMix.MixMode == EBulletMoveMixMode::OverrideAllExceptVerticalVelocity)
	{
		const FVector IncomingVerticalVelocity = MoveToMix.LinearVelocity.ProjectOnToNormal(UpDirection);
		const FVector IncomingNonVerticalVelocity = MoveToMix.LinearVelocity - IncomingVerticalVelocity;
		const FVector ExistingVerticalVelocity = OutCumulativeMove.LinearVelocity.ProjectOnToNormal(UpDirection);

		OutCumulativeMove = MoveToMix;
		OutCumulativeMove.LinearVelocity = IncomingNonVerticalVelocity + ExistingVerticalVelocity;
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled move mix mode was found."));
	}
}

void UBulletMovementMixer::ResetMixerState()
{
	CurrentHighestPriority = 0;
	CurrentLayeredMoveStartTime = TNumericLimits<double>::Max();
}

bool UBulletMovementMixer::CheckPriority(const FBulletLayeredMoveBase* LayeredMove, uint8& InOutHighestPriority, double& InOutCurrentLayeredMoveStartTimeMs)
{
	if (LayeredMove->Priority > InOutHighestPriority)
	{
		InOutHighestPriority = LayeredMove->Priority;
		InOutCurrentLayeredMoveStartTimeMs = LayeredMove->StartSimTimeMs;
		return true;
	}
	if (LayeredMove->Priority == InOutHighestPriority && LayeredMove->StartSimTimeMs < InOutCurrentLayeredMoveStartTimeMs)
	{
		InOutCurrentLayeredMoveStartTimeMs = LayeredMove->StartSimTimeMs;
		return true;
	}

	return false;
}

bool UBulletMovementMixer::CheckPriority(const uint8 LayeredMovePriority, const double LayeredMoveStartTimeMs, uint8& InOutHighestPriority, double& InOutCurrentLayeredMoveStartTimeMs)
{
	if (LayeredMovePriority > InOutHighestPriority)
	{
		InOutHighestPriority = LayeredMovePriority;
		InOutCurrentLayeredMoveStartTimeMs = LayeredMoveStartTimeMs;
		return true;
	}
	if (LayeredMovePriority == InOutHighestPriority && LayeredMoveStartTimeMs < InOutCurrentLayeredMoveStartTimeMs)
	{
		InOutCurrentLayeredMoveStartTimeMs = LayeredMoveStartTimeMs;
		return true;
	}

	return false;
}
