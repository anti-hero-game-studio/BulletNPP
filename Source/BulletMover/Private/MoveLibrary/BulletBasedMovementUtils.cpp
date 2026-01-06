// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/BulletBasedMovementUtils.h"
#include "MoveLibrary/BulletMovementUtils.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"
#include "Components/PrimitiveComponent.h"
#include "BulletMoverComponent.h"
#include "BulletMoverLog.h"
#include "Kismet/KismetMathLibrary.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletBasedMovementUtils)

void FBulletRelativeBaseInfo::Clear()
{
	MovementBase = nullptr;
	BoneName = NAME_None;
	Location = FVector::ZeroVector;
	Rotation = FQuat::Identity;
	ContactLocalPosition = FVector::ZeroVector;
}

bool FBulletRelativeBaseInfo::HasRelativeInfo() const
{
	return MovementBase != nullptr;
}

bool FBulletRelativeBaseInfo::UsesSameBase(const FBulletRelativeBaseInfo& Other) const
{
	return UsesSameBase(Other.MovementBase.Get(), Other.BoneName);
}

bool FBulletRelativeBaseInfo::UsesSameBase(const UPrimitiveComponent* OtherComp, FName OtherBoneName) const
{
	return HasRelativeInfo()
		&& (MovementBase == OtherComp)
		&& (BoneName == OtherBoneName);
}

void FBulletRelativeBaseInfo::SetFromFloorResult(const FBulletFloorCheckResult& FloorTestResult)
{
	bool bDidSucceed = false;

	if (FloorTestResult.bWalkableFloor)
	{
		MovementBase = FloorTestResult.HitResult.GetComponent();

		if (MovementBase.IsValid())
		{
			BoneName = FloorTestResult.HitResult.BoneName;

			if (UBulletBasedMovementUtils::GetMovementBaseTransform(MovementBase.Get(), BoneName, OUT Location, OUT Rotation) &&
				UBulletBasedMovementUtils::TransformWorldLocationToBased(MovementBase.Get(), BoneName, FloorTestResult.HitResult.ImpactPoint, OUT ContactLocalPosition))
			{
				bDidSucceed = true;
			}
		}
	}

	if (!bDidSucceed)
	{
		Clear();
	}
}

void FBulletRelativeBaseInfo::SetFromComponent(UPrimitiveComponent* InRelativeComp, FName InBoneName)
{
	bool bDidSucceed = false;

	MovementBase = InRelativeComp;

	if (MovementBase.IsValid())
	{
		BoneName = InBoneName;
		bDidSucceed = UBulletBasedMovementUtils::GetMovementBaseTransform(MovementBase.Get(), BoneName, /*out*/Location, /*out*/Rotation);
	}

	if (!bDidSucceed)
	{
		Clear();
	}
}


FString FBulletRelativeBaseInfo::ToString() const
{
	if (MovementBase.IsValid())
	{
		return FString::Printf(TEXT("Base: %s, Loc: %s, Rot: %s, LocalContact: %s"),
			*GetNameSafe(MovementBase->GetOwner()),
			*Location.ToCompactString(),
			*Rotation.Rotator().ToCompactString(),
			*ContactLocalPosition.ToCompactString());
	}

	return FString(TEXT("Base: NULL"));
}

bool UBulletBasedMovementUtils::IsADynamicBase(const UPrimitiveComponent* MovementBase)
{
	return (MovementBase && MovementBase->Mobility == EComponentMobility::Movable);
}

bool UBulletBasedMovementUtils::IsBaseSimulatingPhysics(const UPrimitiveComponent* MovementBase)
{
	bool bBaseIsSimulatingPhysics = false;
	const USceneComponent* AttachParent = MovementBase;
	while (!bBaseIsSimulatingPhysics && AttachParent)
	{
		bBaseIsSimulatingPhysics = AttachParent->IsSimulatingPhysics();
		AttachParent = AttachParent->GetAttachParent();
	}
	return bBaseIsSimulatingPhysics;
}


bool UBulletBasedMovementUtils::GetMovementBaseTransform(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector& OutLocation, FQuat& OutQuat)
{
	if (MovementBase)
	{
		bool bBoneNameIsInvalid = false;

		if (BoneName != NAME_None)
		{
			// Check if this socket or bone exists (DoesSocketExist checks for either, as does requesting the transform).
			if (MovementBase->DoesSocketExist(BoneName))
			{
				MovementBase->GetSocketWorldLocationAndRotation(BoneName, OutLocation, OutQuat);
				return true;
			}

			bBoneNameIsInvalid = true;
			UE_LOG(LogBulletMover, Warning, TEXT("GetMovementBaseTransform(): Invalid bone or socket '%s' for PrimitiveComponent base %s. Using component's root transform instead."), *BoneName.ToString(), *GetPathNameSafe(MovementBase));
		}

		OutLocation = MovementBase->GetComponentLocation();
		OutQuat = MovementBase->GetComponentQuat();
		return !bBoneNameIsInvalid;
	}

	// nullptr MovementBase
	OutLocation = FVector::ZeroVector;
	OutQuat = FQuat::Identity;
	return false;
}


bool UBulletBasedMovementUtils::TransformBasedLocationToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalLocation, FVector& OutLocationWorldSpace)
{
	FVector BaseLocation;
	FQuat BaseQuat;
	
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ BaseLocation, /*out*/ BaseQuat))
	{ 
		TransformLocationToWorld(BaseLocation, BaseQuat, LocalLocation, OutLocationWorldSpace);
		return true;
	}
	
	return false;
}


bool UBulletBasedMovementUtils::TransformWorldLocationToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceLocation, FVector& OutLocalLocation)
{
	FVector BaseLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ BaseLocation, /*out*/ BaseQuat))
	{
		TransformLocationToLocal(BaseLocation, BaseQuat, WorldSpaceLocation, OutLocalLocation);
		return true;
	}

	return false;
}


bool UBulletBasedMovementUtils::TransformBasedDirectionToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector LocalDirection, FVector& OutDirectionWorldSpace)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformDirectionToWorld(BaseQuat, LocalDirection, OutDirectionWorldSpace);
		return true;
	}

	return false;
}


bool UBulletBasedMovementUtils::TransformWorldDirectionToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FVector WorldSpaceDirection, FVector& OutLocalDirection)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformDirectionToLocal(BaseQuat, WorldSpaceDirection, OutLocalDirection);
		return true;
	}

	return false;
}


bool UBulletBasedMovementUtils::TransformBasedRotatorToWorld(const UPrimitiveComponent* MovementBase, const FName BoneName, FRotator LocalRotator, FRotator& OutWorldSpaceRotator)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformRotatorToWorld(BaseQuat, LocalRotator, OutWorldSpaceRotator);
		return true;
	}

	return false;
}


bool UBulletBasedMovementUtils::TransformWorldRotatorToBased(const UPrimitiveComponent* MovementBase, const FName BoneName, FRotator WorldSpaceRotator, FRotator& OutLocalRotator)
{
	FVector IgnoredLocation;
	FQuat BaseQuat;
	if (GetMovementBaseTransform(MovementBase, BoneName, /*out*/ IgnoredLocation, /*out*/ BaseQuat))
	{
		TransformRotatorToLocal(BaseQuat, WorldSpaceRotator, OutLocalRotator);
		return true;
	}
	return false;
}


void UBulletBasedMovementUtils::TransformLocationToWorld(FVector BasePos, FQuat BaseQuat, FVector LocalLocation, FVector& OutLocationWorldSpace)
{
	OutLocationWorldSpace = FTransform(BaseQuat, BasePos).TransformPositionNoScale(LocalLocation);
}

void UBulletBasedMovementUtils::TransformLocationToLocal(FVector BasePos, FQuat BaseQuat, FVector WorldSpaceLocation, FVector& OutLocalLocation)
{
	OutLocalLocation = FTransform(BaseQuat, BasePos).InverseTransformPositionNoScale(WorldSpaceLocation);
}

void UBulletBasedMovementUtils::TransformDirectionToWorld(FQuat BaseQuat, FVector LocalDirection, FVector& OutDirectionWorldSpace)
{
	OutDirectionWorldSpace = BaseQuat.RotateVector(LocalDirection);
}

void UBulletBasedMovementUtils::TransformDirectionToLocal(FQuat BaseQuat, FVector WorldSpaceDirection, FVector& OutLocalDirection)
{
	OutLocalDirection = BaseQuat.UnrotateVector(WorldSpaceDirection);
}

void UBulletBasedMovementUtils::TransformRotatorToWorld(FQuat BaseQuat, FRotator LocalRotator, FRotator& OutWorldSpaceRotator)
{
	FQuat LocalQuat(LocalRotator);
	OutWorldSpaceRotator = (BaseQuat * LocalQuat).Rotator();
}

void UBulletBasedMovementUtils::TransformRotatorToLocal(FQuat BaseQuat, FRotator WorldSpaceRotator, FRotator& OutLocalRotator)
{
	FQuat WorldQuat(WorldSpaceRotator);
	OutLocalRotator = (BaseQuat.Inverse() * WorldQuat).Rotator();
}

void UBulletBasedMovementUtils::AddTickDependency(FTickFunction& BasedObjectTick, UPrimitiveComponent* NewBase)
{
	if (NewBase && IsADynamicBase(NewBase))
	{
		if (NewBase->PrimaryComponentTick.bCanEverTick)
		{
			BasedObjectTick.AddPrerequisite(NewBase, NewBase->PrimaryComponentTick);
		}

		AActor* NewBaseOwner = NewBase->GetOwner();
		if (NewBaseOwner)
		{
			if (NewBaseOwner->PrimaryActorTick.bCanEverTick)
			{
				BasedObjectTick.AddPrerequisite(NewBaseOwner, NewBaseOwner->PrimaryActorTick);
			}

			// @TODO: We need to find a more efficient way of finding all ticking components in an actor.
			for (UActorComponent* Component : NewBaseOwner->GetComponents())
			{
				// Dont allow a based component (e.g. a particle system) to push us into a different tick group
				if (Component && Component->PrimaryComponentTick.bCanEverTick && Component->PrimaryComponentTick.TickGroup <= BasedObjectTick.TickGroup)
				{
					BasedObjectTick.AddPrerequisite(Component, Component->PrimaryComponentTick);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogBulletMover, Warning, TEXT("Attempted to AddTickDependency on an invalid or non-dynamic base: %s"), *GetNameSafe(NewBase));
	}
}

void UBulletBasedMovementUtils::RemoveTickDependency(FTickFunction& BasedObjectTick, UPrimitiveComponent* OldBase)
{
	if (OldBase)
	{
		BasedObjectTick.RemovePrerequisite(OldBase, OldBase->PrimaryComponentTick);
		
		if (AActor* OldBaseOwner = OldBase->GetOwner())
		{
			BasedObjectTick.RemovePrerequisite(OldBaseOwner, OldBaseOwner->PrimaryActorTick);

			// @TODO: We need to find a more efficient way of finding all ticking components in an actor.
			for (UActorComponent* Component : OldBaseOwner->GetComponents())
			{
				if (Component && Component->PrimaryComponentTick.bCanEverTick)
				{
					BasedObjectTick.RemovePrerequisite(Component, Component->PrimaryComponentTick);
				}
			}
		}
	}
}


void UBulletBasedMovementUtils::UpdateSimpleBasedMovement(UBulletMoverComponent* TargetMoverComp)
{
	if (!TargetMoverComp)
	{
		return;
	}

	UBulletMoverBlackboard* SimBlackboard = TargetMoverComp->GetSimBlackboard_Mutable();
	USceneComponent* UpdatedComponent = TargetMoverComp->UpdatedComponent;

	bool bIgnoreBaseRotation = false;

	if (const UBulletCommonLegacyMovementSettings* CommonSettings = TargetMoverComp->FindSharedSettings<UBulletCommonLegacyMovementSettings>())
	{
		bIgnoreBaseRotation = CommonSettings->bIgnoreBaseRotation;
	}

	bool bDidGetUpToDate = false;

	FBulletRelativeBaseInfo LastFoundBaseInfo;	// Last-found is the most recent capture during movement, likely set this sim frame
	FBulletRelativeBaseInfo LastAppliedBaseInfo;	// Last-applied is the one that our based movement is up to date with, likely set in the last sim frame
	FBulletRelativeBaseInfo CurrentBaseInfo;		// Current info is the current snapshot of the current base, with up-to-date transform that may be different than last-found.

	const bool bHasLastFoundInfo = SimBlackboard->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, LastFoundBaseInfo);
	const bool bHasLastAppliedInfo = SimBlackboard->TryGet(CommonBlackboard::LastAppliedDynamicMovementBase, LastAppliedBaseInfo);
	if (bHasLastFoundInfo)
	{
		if (!bHasLastAppliedInfo || !LastFoundBaseInfo.UsesSameBase(LastAppliedBaseInfo))
		{
			LastAppliedBaseInfo = LastFoundBaseInfo;	// This is the first time we've checked this base, so start with the last-found capture
		}

		if (!ensureMsgf(LastFoundBaseInfo.HasRelativeInfo() && LastFoundBaseInfo.UsesSameBase(LastAppliedBaseInfo),
				TEXT("Attempting to update based movement with a missing or mismatched base. This may indicate a logic problem with detecting bases.")))
		{ 
			SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
			SimBlackboard->Invalidate(CommonBlackboard::LastAppliedDynamicMovementBase);
			return;
		}

		CurrentBaseInfo.SetFromComponent(LastFoundBaseInfo.MovementBase.Get(), LastFoundBaseInfo.BoneName);
		CurrentBaseInfo.ContactLocalPosition = LastFoundBaseInfo.ContactLocalPosition;

		FVector CurrentBaseLocation;
		FQuat CurrentBaseQuat;
			

		if (UBulletBasedMovementUtils::GetMovementBaseTransform(CurrentBaseInfo.MovementBase.Get(), CurrentBaseInfo.BoneName, OUT CurrentBaseLocation, OUT CurrentBaseQuat))
		{
			const bool bDidBaseRotationChange = !LastAppliedBaseInfo.Rotation.Equals(CurrentBaseQuat, UE_SMALL_NUMBER);
			const bool bDidBaseLocationChange = (LastAppliedBaseInfo.Location != CurrentBaseLocation);

			FQuat DeltaQuat = FQuat::Identity;
			FVector WorldDeltaLocation = FVector::ZeroVector;
			FQuat WorldTargetQuat = UpdatedComponent->GetComponentQuat();

			// Find change in rotation

			if (bDidBaseRotationChange && !bIgnoreBaseRotation)
			{
				DeltaQuat = CurrentBaseQuat * LastAppliedBaseInfo.Rotation.Inverse();
				WorldTargetQuat = DeltaQuat * WorldTargetQuat;

				FVector TargetForwVector = WorldTargetQuat.GetForwardVector();
				TargetForwVector = FVector::VectorPlaneProject(TargetForwVector, -TargetMoverComp->GetUpDirection());
				TargetForwVector.Normalize();

				FVector TargetRightVector = WorldTargetQuat.GetRightVector();
				TargetRightVector = FVector::VectorPlaneProject(TargetRightVector, -TargetMoverComp->GetUpDirection());
				TargetRightVector.Normalize();
					
				WorldTargetQuat = UKismetMathLibrary::MakeRotFromXY(TargetForwVector, TargetRightVector).Quaternion();
			}

			if (bDidBaseLocationChange || bDidBaseRotationChange)
			{
				// Calculate new transform matrix of base actor (ignoring scale).
				const FQuatRotationTranslationMatrix OldLocalToWorld(LastAppliedBaseInfo.Rotation, LastAppliedBaseInfo.Location);
				const FQuatRotationTranslationMatrix NewLocalToWorld(CurrentBaseQuat, CurrentBaseLocation);

				// Find change in location
				// NOTE that we are using the floor hit location, not the actor's root position which may be floating above the base
				const FVector NewWorldBaseContactPos = NewLocalToWorld.TransformPosition(CurrentBaseInfo.ContactLocalPosition);
				const FVector OldWorldBaseContactPos = OldLocalToWorld.TransformPosition(CurrentBaseInfo.ContactLocalPosition);
				WorldDeltaLocation = NewWorldBaseContactPos - OldWorldBaseContactPos;

				const FVector OldWorldLocation = UpdatedComponent->GetComponentLocation();
				EMoveComponentFlags MoveComponentFlags = MOVECOMP_IgnoreBases;
				const bool bSweep = true;
				FHitResult MoveHitResult;

				bool bDidMove = UBulletMovementUtils::TryMoveUpdatedComponent_Internal(FBulletMovingComponentSet(TargetMoverComp), WorldDeltaLocation, WorldTargetQuat, bSweep, MoveComponentFlags, &MoveHitResult, ETeleportType::None);
					
				const FVector NewWorldLocation = UpdatedComponent->GetComponentLocation();

				if ((NewWorldLocation - (OldWorldLocation + WorldDeltaLocation)).IsNearlyZero() == false)
				{
					// Find the remaining delta that wasn't achieved
					const FVector UnachievedWorldDelta = (OldWorldLocation + WorldDeltaLocation) - NewWorldLocation;

					// Convert the remaining delta to current base space
					FVector UnachievedLocalDelta;
					UBulletBasedMovementUtils::TransformLocationToLocal(CurrentBaseLocation, CurrentBaseQuat, UnachievedWorldDelta, OUT UnachievedLocalDelta);
						
					// Subtract the remaining delta to reflect the change in the contact position
					CurrentBaseInfo.ContactLocalPosition -= UnachievedLocalDelta;
				}

				// Propagate the movement changes to the backend's state, if supported

				// Note that this is occurring out-of-band with the movement simulation, in order to support based movement regardless of update order or
				// whether the movement base is also simulated through Mover.

				FBulletMoverSyncState PendingSimSyncState;
				if (TargetMoverComp->BackendLiaisonComp->ReadPendingSyncState(OUT PendingSimSyncState))
				{
					// Modify the PENDING sync state that has not yet been committed to simulation history nor replicated
					if (FBulletMoverDefaultSyncState* PendingMoverState = PendingSimSyncState.Collection.FindMutableDataByType<FBulletMoverDefaultSyncState>())
					{
						FTransform OldSyncTransformWs = PendingMoverState->GetTransform_WorldSpace();
						FTransform NewSyncTransformWs = UpdatedComponent->GetComponentTransform();

						PendingMoverState->SetTransforms_WorldSpace(
							NewSyncTransformWs.GetLocation(),
							NewSyncTransformWs.GetRotation().Rotator(),
							PendingMoverState->GetVelocity_WorldSpace(),	// keep same velocity and base
							PendingMoverState->GetAngularVelocityDegrees_WorldSpace(),
							PendingMoverState->GetMovementBase(), PendingMoverState->GetMovementBaseBoneName());

						TargetMoverComp->BackendLiaisonComp->WritePendingSyncState(PendingSimSyncState);	// writes pending Simulation state

						// If smoothing, modify presentation-related states as well so that the visual offset location stays anchored to the movement base
						if (TargetMoverComp->SmoothingMode != EBulletMoverSmoothingMode::None)
						{
							const FTransform OldToNewTransform = NewSyncTransformWs.GetRelativeTransform(OldSyncTransformWs);

							// Modify the PRESENTATION sync state that we're smoothing TO
							FBulletMoverSyncState PresentationSyncState;
							if (TargetMoverComp->BackendLiaisonComp->ReadPresentationSyncState(OUT PresentationSyncState))
							{
								if (FBulletMoverDefaultSyncState* PresentationMoverState = PresentationSyncState.Collection.FindMutableDataByType<FBulletMoverDefaultSyncState>())
								{
									OldSyncTransformWs = PresentationMoverState->GetTransform_WorldSpace();
									NewSyncTransformWs = OldToNewTransform * OldSyncTransformWs;

									PresentationMoverState->SetTransforms_WorldSpace(
										NewSyncTransformWs.GetLocation(),
										NewSyncTransformWs.GetRotation().Rotator(),
										PresentationMoverState->GetVelocity_WorldSpace(),	// keep same velocity and base
										PresentationMoverState->GetAngularVelocityDegrees_WorldSpace(),
										PresentationMoverState->GetMovementBase(), PresentationMoverState->GetMovementBaseBoneName());

									TargetMoverComp->BackendLiaisonComp->WritePresentationSyncState(PresentationSyncState);
								}
							}

							// Modify the PREV PRESENTATION sync state that we're smoothing FROM
							FBulletMoverSyncState PrevPresentationSyncState;
							if (TargetMoverComp->BackendLiaisonComp->ReadPrevPresentationSyncState(OUT PrevPresentationSyncState))
							{
								if (FBulletMoverDefaultSyncState* PrevPresentationMoverState = PrevPresentationSyncState.Collection.FindMutableDataByType<FBulletMoverDefaultSyncState>())
								{
									OldSyncTransformWs = PrevPresentationMoverState->GetTransform_WorldSpace();
									NewSyncTransformWs = OldToNewTransform * OldSyncTransformWs;

									PrevPresentationMoverState->SetTransforms_WorldSpace(
										NewSyncTransformWs.GetLocation(),
										NewSyncTransformWs.GetRotation().Rotator(),
										PrevPresentationMoverState->GetVelocity_WorldSpace(),	// keep same velocity and base
										PrevPresentationMoverState->GetAngularVelocityDegrees_WorldSpace(),
										PrevPresentationMoverState->GetMovementBase(), PrevPresentationMoverState->GetMovementBaseBoneName());

									TargetMoverComp->BackendLiaisonComp->WritePrevPresentationSyncState(PrevPresentationSyncState);	
								}
							}
						}
					}
				}
			}

			SimBlackboard->Set(CommonBlackboard::LastAppliedDynamicMovementBase, CurrentBaseInfo);
			bDidGetUpToDate = true;
		}
	}

	if (!bDidGetUpToDate)
	{
		SimBlackboard->Invalidate(CommonBlackboard::LastAppliedDynamicMovementBase);
	}
}



// FBulletMoverDynamicBasedMovementTickFunction ////////////////////////////////////

void FBulletMoverDynamicBasedMovementTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FActorComponentTickFunction::ExecuteTickHelper(TargetMoverComp, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
		{
			UBulletBasedMovementUtils::UpdateSimpleBasedMovement(TargetMoverComp);
		});

	if (bAutoDisableAfterTick)
	{
		SetTickFunctionEnable(false);
	}
}
FString FBulletMoverDynamicBasedMovementTickFunction::DiagnosticMessage()
{
	return TargetMoverComp->GetFullName() + TEXT("[FBulletMoverDynamicBasedMovementTickFunction]");
}
FName FBulletMoverDynamicBasedMovementTickFunction::DiagnosticContext(bool bDetailed)
{
	if (bDetailed)
	{
		return FName(*FString::Printf(TEXT("UBulletMoverComponent/%s"), *GetFullNameSafe(TargetMoverComp)));
	}
	return FName(TEXT("FBulletMoverDynamicBasedMovementTickFunction"));
}

