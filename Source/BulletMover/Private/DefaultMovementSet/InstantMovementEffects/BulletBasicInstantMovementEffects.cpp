// Copyright Epic Games, Inc. All Rights Reserved.


#include "DefaultMovementSet/InstantMovementEffects/BulletBasicInstantMovementEffects.h"

#include "BulletMoverComponent.h"
#include "BulletMoverDataModelTypes.h"
#include "BulletMoverSimulationTypes.h"
#include "BulletMoverSimulation.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"
#include "MoveLibrary/BulletMovementUtils.h"
#include "MoveLibrary/BulletMoverBlackboard.h"
#include "DrawDebugHelpers.h"

// -------------------------------------------------------------------
// FBulletTeleportEffect
// -------------------------------------------------------------------

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletBasicInstantMovementEffects)

static int32 ShowTeleportDiffs = 0;
static float ShowTeleportDiffsLifetimeSecs = 3.0f;
FAutoConsoleVariableRef CVarShowTeleportDiffs(
	TEXT("bullet.mover.debug.ShowTeleportDiffs"),
	ShowTeleportDiffs,
	TEXT("Whether to draw teleportation differences (red is initially blocked, green is corrected).\n")
	TEXT("0: Disable, 1: Enable"),
	ECVF_Cheat);


FBulletTeleportEffect::FBulletTeleportEffect()
	: TargetLocation(FVector::ZeroVector)
	, bUseActorRotation(true)
	, TargetRotation(FRotator::ZeroRotator)
{
}

bool FBulletTeleportEffect::ApplyMovementEffect(FBulletApplyMovementEffectParams& ApplyEffectParams, FBulletMoverSyncState& OutputState)
{
	USceneComponent* UpdatedComponent = ApplyEffectParams.UpdatedComponent;
	const FRotator FinalTargetRotation = bUseActorRotation ? UpdatedComponent->GetComponentRotation() : TargetRotation;
	
	const FVector PreviousLocation = UpdatedComponent->GetComponentLocation();
	const FQuat PreviousRotation = UpdatedComponent->GetComponentQuat();
	AActor* OwnerActor = UpdatedComponent->GetOwner();

	if (OwnerActor->TeleportTo(TargetLocation, FinalTargetRotation))
	{
		const FVector UpdatedLocation = UpdatedComponent->GetComponentLocation();
#if !defined(BUILD_SHIPPING) || !BUILD_SHIPPING
		if (ShowTeleportDiffs)
		{
			if (!(UpdatedLocation - TargetLocation).IsNearlyZero())	// if it was adjusted, show the original error
			{
				DrawDebugCapsule(OwnerActor->GetWorld(), TargetLocation, OwnerActor->GetSimpleCollisionHalfHeight(), OwnerActor->GetSimpleCollisionRadius(), FQuat::Identity, FColor::Red, false, ShowTeleportDiffsLifetimeSecs);
			}
			DrawDebugCapsule(OwnerActor->GetWorld(), UpdatedLocation, OwnerActor->GetSimpleCollisionHalfHeight(), OwnerActor->GetSimpleCollisionRadius(), FQuat::Identity, FColor(100, 100, 255), false, ShowTeleportDiffsLifetimeSecs);
		}
#endif // !defined(BUILD_SHIPPING) || !BUILD_SHIPPING

		FBulletMoverDefaultSyncState& OutputSyncState = OutputState.SyncStateCollection.FindOrAddMutableDataByType<FBulletMoverDefaultSyncState>();
		OutputSyncState.SetTransforms_WorldSpace(UpdatedLocation,
													UpdatedComponent->GetComponentRotation(),
													OutputSyncState.GetVelocity_WorldSpace(),
													OutputSyncState.GetAngularVelocityDegrees_WorldSpace(),
													nullptr ); // no movement base
		
		// TODO: instead of invalidating it, consider checking for a floor. Possibly a dynamic base?
		if (UBulletMoverBlackboard* SimBlackboard = ApplyEffectParams.MoverComp->GetSimBlackboard_Mutable())
		{
			SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
			SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
		}

		ApplyEffectParams.OutputEvents.Add(MakeShared<FBulletTeleportSucceededEventData>(ApplyEffectParams.TimeStep->BaseSimTimeMs, PreviousLocation, PreviousRotation, TargetLocation, FQuat(FinalTargetRotation)));

		return true;
	}

#if !defined(BUILD_SHIPPING) || !BUILD_SHIPPING
	if (ShowTeleportDiffs)
	{
		DrawDebugCapsule(OwnerActor->GetWorld(), TargetLocation, OwnerActor->GetSimpleCollisionHalfHeight(), OwnerActor->GetSimpleCollisionRadius(), FQuat::Identity, FColor::Red, false, ShowTeleportDiffsLifetimeSecs);
	}
#endif // !defined(BUILD_SHIPPING) || !BUILD_SHIPPING

	ApplyEffectParams.OutputEvents.Add(MakeShared<FBulletTeleportFailedEventData>(ApplyEffectParams.TimeStep->BaseSimTimeMs, PreviousLocation, PreviousRotation, TargetLocation, FQuat(FinalTargetRotation), ETeleportFailureReason::Reason_NotAvailable));

	return false;
}

bool FBulletTeleportEffect::ApplyMovementEffect_Async(FBulletApplyMovementEffectParams_Async& ApplyEffectParams, FBulletMoverSyncState& OutputState)
{
	if (ApplyEffectParams.Simulation)
	{
		ApplyEffectParams.Simulation->AttemptTeleport(*ApplyEffectParams.TimeStep, FTransform(TargetRotation, TargetLocation), bUseActorRotation, OutputState);
		return true;
	}

	return false;
}

FBulletInstantMovementEffect* FBulletTeleportEffect::Clone() const
{
	FBulletTeleportEffect* CopyPtr = new FBulletTeleportEffect(*this);
	return CopyPtr;
}

void FBulletTeleportEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << TargetLocation;
	
	Ar.SerializeBits(&bUseActorRotation, 1);
	if (!bUseActorRotation)
	{
		Ar << TargetRotation;
	}
}

UScriptStruct* FBulletTeleportEffect::GetScriptStruct() const
{
	return FBulletTeleportEffect::StaticStruct();
}

FString FBulletTeleportEffect::ToSimpleString() const
{
	return bUseActorRotation ? FString::Printf(TEXT("Teleport to %s (bUseActorRotation = True)"), *TargetLocation.ToString()) : FString::Printf(TEXT("Teleport to %s, %s (bUseActorRotation = False)"), *TargetLocation.ToString(), *FRotator(TargetRotation).ToString());
}

void FBulletTeleportEffect::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}


// -------------------------------------------------------------------
// FAsyncTeleportEffect
// -------------------------------------------------------------------

bool FAsyncTeleportEffect::ApplyMovementEffect(FBulletApplyMovementEffectParams& ApplyEffectParams, FBulletMoverSyncState& OutputState)
{
	FVector TeleportLocation = TargetLocation;
	const FRotator TeleportRotation = bUseActorRotation ? ApplyEffectParams.UpdatedComponent->GetComponentRotation() : TargetRotation;

	if (UBulletMovementUtils::FindTeleportSpot(ApplyEffectParams.MoverComp, OUT TeleportLocation, TeleportRotation))
	{
		if (ShowTeleportDiffs)
		{
			const AActor* OwnerActor = ApplyEffectParams.UpdatedComponent->GetOwner();

			if (!(TeleportLocation - TargetLocation).IsNearlyZero())	// if it was adjusted, show the original error
			{
				DrawDebugCapsule(OwnerActor->GetWorld(), TargetLocation, OwnerActor->GetSimpleCollisionHalfHeight(), OwnerActor->GetSimpleCollisionRadius(), FQuat::Identity, FColor::Red, false, ShowTeleportDiffsLifetimeSecs);
			}

			DrawDebugCapsule(OwnerActor->GetWorld(), TeleportLocation, OwnerActor->GetSimpleCollisionHalfHeight(), OwnerActor->GetSimpleCollisionRadius(), FQuat::Identity, FColor(100, 100, 255), false, ShowTeleportDiffsLifetimeSecs);
		}

		FBulletMoverDefaultSyncState& OutputSyncState = OutputState.SyncStateCollection.FindOrAddMutableDataByType<FBulletMoverDefaultSyncState>();

		if (const FBulletMoverDefaultSyncState* StartingSyncState = ApplyEffectParams.StartState->SyncState.SyncStateCollection.FindDataByType<FBulletMoverDefaultSyncState>())
		{
			OutputSyncState.SetTransforms_WorldSpace(TeleportLocation,
				TeleportRotation,
				OutputSyncState.GetVelocity_WorldSpace(),
				OutputSyncState.GetAngularVelocityDegrees_WorldSpace(),
				nullptr); // no movement base

			// TODO: instead of invalidating it, consider checking for a floor. Possibly a dynamic base?
			if (UBulletMoverBlackboard* SimBlackboard = ApplyEffectParams.MoverComp->GetSimBlackboard_Mutable())
			{
				SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
				SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
			}

			return true;
		}
	}


	if (ShowTeleportDiffs)
	{
		const AActor* OwnerActor = ApplyEffectParams.UpdatedComponent->GetOwner();
		DrawDebugCapsule(OwnerActor->GetWorld(), TargetLocation, OwnerActor->GetSimpleCollisionHalfHeight(), OwnerActor->GetSimpleCollisionRadius(), FQuat::Identity, FColor::Red, false, ShowTeleportDiffsLifetimeSecs);
	}

	return false;
}

FBulletInstantMovementEffect* FAsyncTeleportEffect::Clone() const
{
	FAsyncTeleportEffect* CopyPtr = new FAsyncTeleportEffect(*this);
	return CopyPtr;
}

UScriptStruct* FAsyncTeleportEffect::GetScriptStruct() const
{
	return FAsyncTeleportEffect::StaticStruct();
}

FString FAsyncTeleportEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("Async Teleport"));
}


// -------------------------------------------------------------------
// FJumpImpulseEffect
// -------------------------------------------------------------------

FJumpImpulseEffect::FJumpImpulseEffect()
	: UpwardsSpeed(0.f)
{
}

bool FJumpImpulseEffect::ApplyMovementEffect(FBulletApplyMovementEffectParams& ApplyEffectParams, FBulletMoverSyncState& OutputState)
{
	if (const FBulletMoverDefaultSyncState* SyncState = ApplyEffectParams.StartState->SyncState.SyncStateCollection.FindDataByType<FBulletMoverDefaultSyncState>())
	{
		FBulletMoverDefaultSyncState& OutputSyncState = OutputState.SyncStateCollection.FindOrAddMutableDataByType<FBulletMoverDefaultSyncState>();
		
		const FVector UpDir = ApplyEffectParams.MoverComp->GetUpDirection();
		const FVector ImpulseVelocity = UpDir * UpwardsSpeed;
	
		// Jump impulse overrides vertical velocity while maintaining the rest
		const FVector PriorVelocityWS = SyncState->GetVelocity_WorldSpace();
		const FVector StartingNonUpwardsVelocity = PriorVelocityWS - PriorVelocityWS.ProjectOnToNormal(UpDir);

		if (const UBulletCommonLegacyMovementSettings* CommonSettings = ApplyEffectParams.MoverComp->FindSharedSettings<UBulletCommonLegacyMovementSettings>())
		{
			OutputState.MovementMode = CommonSettings->AirMovementModeName;
		}
		
		FBulletRelativeBaseInfo MovementBaseInfo;
		if (const UBulletMoverBlackboard* SimBlackboard = ApplyEffectParams.MoverComp->GetSimBlackboard())
		{
			SimBlackboard->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, MovementBaseInfo);
		}

		const FVector FinalVelocity = StartingNonUpwardsVelocity + ImpulseVelocity;
		OutputSyncState.SetTransforms_WorldSpace( ApplyEffectParams.UpdatedComponent->GetComponentLocation(),
												  ApplyEffectParams.UpdatedComponent->GetComponentRotation(),
												  FinalVelocity,
												  FVector::ZeroVector,
												  MovementBaseInfo.MovementBase.Get(),
												  MovementBaseInfo.BoneName);
		
		ApplyEffectParams.UpdatedComponent->ComponentVelocity = FinalVelocity;
		
		return true;
	}

	return false;
}

FBulletInstantMovementEffect* FJumpImpulseEffect::Clone() const
{
	FJumpImpulseEffect* CopyPtr = new FJumpImpulseEffect(*this);
	return CopyPtr;
}

void FJumpImpulseEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	Ar << UpwardsSpeed;
}

UScriptStruct* FJumpImpulseEffect::GetScriptStruct() const
{
	return FJumpImpulseEffect::StaticStruct();
}

FString FJumpImpulseEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("JumpImpulse"));
}

void FJumpImpulseEffect::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}

// -------------------------------------------------------------------
// FBulletApplyVelocityEffect
// -------------------------------------------------------------------

FBulletApplyVelocityEffect::FBulletApplyVelocityEffect()
	: VelocityToApply(FVector::ZeroVector)
	, bAdditiveVelocity(false)
	, ForceMovementMode(NAME_None)
{
}

bool FBulletApplyVelocityEffect::ApplyMovementEffect(FBulletApplyMovementEffectParams& ApplyEffectParams, FBulletMoverSyncState& OutputState)
{
	FBulletMoverDefaultSyncState& OutputSyncState = OutputState.SyncStateCollection.FindOrAddMutableDataByType<FBulletMoverDefaultSyncState>();
	
	OutputState.MovementMode = ForceMovementMode;
	
	FBulletRelativeBaseInfo MovementBaseInfo;
	if (const UBulletMoverBlackboard* SimBlackboard = ApplyEffectParams.MoverComp->GetSimBlackboard())
	{
		SimBlackboard->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, MovementBaseInfo);
	}

	FVector Velocity = VelocityToApply;
	if (bAdditiveVelocity)
	{
		if (const FBulletMoverDefaultSyncState* SyncState = ApplyEffectParams.StartState->SyncState.SyncStateCollection.FindDataByType<FBulletMoverDefaultSyncState>())
		{
			Velocity += SyncState->GetVelocity_WorldSpace();
		}
	}

	OutputSyncState.SetTransforms_WorldSpace( ApplyEffectParams.UpdatedComponent->GetComponentLocation(),
											  ApplyEffectParams.UpdatedComponent->GetComponentRotation(),
											  Velocity,
											  FVector::ZeroVector,
											  MovementBaseInfo.MovementBase.Get(),
											  MovementBaseInfo.BoneName);

	ApplyEffectParams.UpdatedComponent->ComponentVelocity = Velocity;
	
	return true;
}

FBulletInstantMovementEffect* FBulletApplyVelocityEffect::Clone() const
{
	FBulletApplyVelocityEffect* CopyPtr = new FBulletApplyVelocityEffect(*this);
	return CopyPtr;
}

void FBulletApplyVelocityEffect::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10, 16>(VelocityToApply, Ar);

	Ar << bAdditiveVelocity;
	
	bool bUsingForcedMovementMode = !ForceMovementMode.IsNone();
	Ar.SerializeBits(&bUsingForcedMovementMode, 1);

	if (bUsingForcedMovementMode)
	{
		Ar << ForceMovementMode;
	}
}

UScriptStruct* FBulletApplyVelocityEffect::GetScriptStruct() const
{
	return FBulletApplyVelocityEffect::StaticStruct();
}

FString FBulletApplyVelocityEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("ApplyVelocity"));
}

void FBulletApplyVelocityEffect::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}
