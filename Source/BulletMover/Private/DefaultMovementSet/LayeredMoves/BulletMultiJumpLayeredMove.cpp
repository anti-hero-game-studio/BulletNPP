// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/LayeredMoves/BulletMultiJumpLayeredMove.h"
#include "BulletMoverComponent.h"
#include "BulletMoverSimulationTypes.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMultiJumpLayeredMove)

FBulletLayeredMove_MultiJump::FBulletLayeredMove_MultiJump()
	: MaximumInAirJumps(1)
	, UpwardsSpeed(800)
	, JumpsInAirRemaining(-1)
	, TimeOfLastJumpMS(0)
{
	DurationMs = -1.f;
	MixMode = EBulletMoveMixMode::OverrideVelocity;
}

bool FBulletLayeredMove_MultiJump::WantsToJump(const FBulletMoverInputCmdContext& InputCmd)
{
	if (const FBulletCharacterDefaultInputs* CharacterInputs = InputCmd.InputCollection.FindDataByType<FBulletCharacterDefaultInputs>())
	{
		return CharacterInputs->bIsJumpJustPressed;
	}
	
	return false;
}

bool FBulletLayeredMove_MultiJump::GenerateMove(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, const UBulletMoverComponent* MoverComp, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutProposedMove)
{
	const FBulletCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FBulletCharacterDefaultInputs>();
	const FBulletMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FBulletMoverDefaultSyncState>();
	check(SyncState);

	OutProposedMove.MixMode = MixMode;

	FBulletFloorCheckResult FloorHitResult;
	bool bValidBlackboard = SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, OUT FloorHitResult);

	if (FMath::IsNearlyEqual(StartSimTimeMs, TimeStep.BaseSimTimeMs))
	{
		JumpsInAirRemaining = MaximumInAirJumps;
	}
	
	bool bPerformedJump = false;
	if (CharacterInputs && CharacterInputs->bIsJumpJustPressed)
	{
		if (StartSimTimeMs == TimeStep.BaseSimTimeMs)
		{
			// if this was the first jump and its a valid floor we do the initial jump from walking and back out so we don't get extra jump
			if (bValidBlackboard && FloorHitResult.IsWalkableFloor())
			{
				bPerformedJump = PerformJump(SyncState, TimeStep, MoverComp, OutProposedMove);
				return bPerformedJump;
			}
		}

		// perform in air jump
		if (TimeStep.BaseSimTimeMs > TimeOfLastJumpMS && JumpsInAirRemaining > 0)
		{
			JumpsInAirRemaining--;
			bPerformedJump = PerformJump(SyncState, TimeStep, MoverComp, OutProposedMove);
		}
		else
		{
			// setting mix mode to additive when we're not adding any jump input so air movement acts as expected
			OutProposedMove.MixMode = EBulletMoveMixMode::AdditiveVelocity;
		}
	}
	else
	{
		// setting mix mode to additive when we're not adding any jump input so air movement acts as expected
		OutProposedMove.MixMode = EBulletMoveMixMode::AdditiveVelocity;
	}

	// if we hit a valid floor and it's not the start of the move (since we could start this move on the ground) end this move
	if ((bValidBlackboard && FloorHitResult.IsWalkableFloor() && StartSimTimeMs < TimeStep.BaseSimTimeMs) || JumpsInAirRemaining <= 0)
	{
		DurationMs = 0;
	}

	return bPerformedJump;
}

FBulletLayeredMoveBase* FBulletLayeredMove_MultiJump::Clone() const
{
	FBulletLayeredMove_MultiJump* CopyPtr = new FBulletLayeredMove_MultiJump(*this);
	return CopyPtr;
}

void FBulletLayeredMove_MultiJump::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << MaximumInAirJumps;
	Ar << UpwardsSpeed;
	Ar << JumpsInAirRemaining;
	Ar << TimeOfLastJumpMS;
}

UScriptStruct* FBulletLayeredMove_MultiJump::GetScriptStruct() const
{
	return FBulletLayeredMove_MultiJump::StaticStruct();
}

FString FBulletLayeredMove_MultiJump::ToSimpleString() const
{
	return FString::Printf(TEXT("Multi-jump"));
}

void FBulletLayeredMove_MultiJump::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}

bool FBulletLayeredMove_MultiJump::PerformJump(const FBulletMoverDefaultSyncState* SyncState, const FBulletMoverTimeStep& TimeStep, const UBulletMoverComponent* MoverComp, FBulletProposedMove& OutProposedMove)
{
	TimeOfLastJumpMS = TimeStep.BaseSimTimeMs;
	if (const TObjectPtr<const UBulletCommonLegacyMovementSettings> CommonLegacySettings = MoverComp->FindSharedSettings<UBulletCommonLegacyMovementSettings>())
	{
		OutProposedMove.PreferredMode = CommonLegacySettings->AirMovementModeName;
	}

	const FVector UpDir = MoverComp->GetUpDirection();

	const FVector ImpulseVelocity = UpDir * UpwardsSpeed;

	switch (MixMode)
	{
	case EBulletMoveMixMode::AdditiveVelocity:
		{
			OutProposedMove.LinearVelocity = ImpulseVelocity;
			break;
		}
		
	case EBulletMoveMixMode::OverrideAll:
	case EBulletMoveMixMode::OverrideVelocity:
		{
			// Jump impulse overrides vertical velocity while maintaining the rest
			const FVector PriorVelocityWS = SyncState->GetVelocity_WorldSpace();
			const FVector StartingNonUpwardsVelocity = PriorVelocityWS - PriorVelocityWS.ProjectOnToNormal(UpDir);

			OutProposedMove.LinearVelocity = StartingNonUpwardsVelocity + ImpulseVelocity;
			break;
		}
		
	default:
		ensureMsgf(false, TEXT("Multi-Jump layered move has an invalid MixMode and will do nothing."));
		return false;
	}

	return true;
}
