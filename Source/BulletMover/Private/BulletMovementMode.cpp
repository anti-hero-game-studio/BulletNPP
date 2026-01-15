// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMovementMode.h"
#include "BulletMovementModeTransition.h"
#include "BulletMoverComponent.h"
#include "BulletMoverLog.h"
#include "Engine/BlueprintGeneratedClass.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "Core/ContactHandling/BulletContactGatherer.h"
#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMovementMode)

#define LOCTEXT_NAMESPACE "BulletMover"

UWorld* UBulletBaseMovementMode::GetWorld() const
{
#if WITH_EDITOR
	// In the editor, GetWorld() is called on the CDO as part of checking ImplementsGetWorld(). Only the CDO can exist without being outer'd to a MoverComponent.
	if (IsTemplate())
	{
		return nullptr;
	}
#endif
	return GetOuterUBulletMoverComponent()->GetWorld();
}


void UBulletBaseMovementMode::OnRegistered(const FName ModeName)
{
	for (TObjectPtr<UBulletBaseMovementModeTransition>& Transition : Transitions)
	{
		if (Transition)
		{
			Transition->OnRegistered();
		}
		else
		{
			UE_LOG(LogBulletMover, Error, TEXT("Invalid or missing transition object on mode of type %s of component %s for actor %s"), *GetPathNameSafe(this), *GetNameSafe(GetOuter()), *GetNameSafe(GetOutermost()));
		}
	}
	
	K2_OnRegistered(ModeName);
}

void UBulletBaseMovementMode::OnUnregistered()
{
	for (TObjectPtr<UBulletBaseMovementModeTransition>& Transition : Transitions)
	{
		if (Transition)
		{
			Transition->OnUnregistered();
		}
		else
		{
			UE_LOG(LogBulletMover, Error, TEXT("Invalid or missing transition object on mode of type %s of component %s for actor %s"), *GetPathNameSafe(this), *GetNameSafe(GetOuter()), *GetNameSafe(GetOutermost()));
		}
	}

	K2_OnUnregistered();
}

void UBulletBaseMovementMode::Activate()
{
	if (!bSupportsAsync)
	{
		K2_OnActivated();
	}
}

void UBulletBaseMovementMode::Deactivate()
{
	if (!bSupportsAsync)
	{
		K2_OnDeactivated();
	}
}

void UBulletBaseMovementMode::Activate_External()
{
	if (bSupportsAsync)
	{
		K2_OnActivated();
	}
}

void UBulletBaseMovementMode::Deactivate_External()
{
	if (bSupportsAsync)
	{
		K2_OnDeactivated();
	}
}

void UBulletBaseMovementMode::GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const
{
}

void UBulletBaseMovementMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
}

UBulletMoverComponent* UBulletBaseMovementMode::K2_GetMoverComponent() const
{
	return GetOuterUBulletMoverComponent();
}

#if WITH_EDITOR
EDataValidationResult UBulletBaseMovementMode::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;
	for (UBulletBaseMovementModeTransition* Transition : Transitions)
	{
		if (!IsValid(Transition))
		{
			Context.AddError(FText::Format(LOCTEXT("InvalidTransitionOnModeError", "Invalid or missing transition object on mode of type {0}. Clean up the Transitions array."),
				FText::FromString(GetClass()->GetName())));

			Result = EDataValidationResult::Invalid;
		}
		else if (Transition->IsDataValid(Context) == EDataValidationResult::Invalid)
		{
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif // WITH_EDITOR


bool UBulletBaseMovementMode::HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const
{
	if (bExactMatch)
	{
		return GameplayTags.HasTagExact(TagToFind);
	}

	return GameplayTags.HasTag(TagToFind);
}

const FName UBulletNullMovementMode::NullModeName(TEXT("Null"));

UBulletNullMovementMode::UBulletNullMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBulletBaseMovementMode::FloorCheck(const FVector& StartingLocation, const FVector& ProposedLinearVelocity, const float& DeltaTime, FBulletFloorCheckResult& Result) const
{
	const UBulletMoverComponent* MoverComp = GetMoverComponent<UBulletMoverComponent>();
	if (!MoverComp) return;
	const UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	if (!Subsystem) return;
	const UBulletCommonLegacyMovementSettings* SharedSettingsPtr = GetMoverComponent<UBulletMoverComponent>()->FindSharedSettings<UBulletCommonLegacyMovementSettings>();
	if (!SharedSettingsPtr) return;
	
	TArray<FBulletHitEvent> Events = Subsystem->GetAllHitEvents();
	if (Events.IsEmpty()) return;

	for (const FBulletHitEvent& E : Events)
	{
		if (E.SelfComp != MoverComp->GetUpdatedComponent()) continue;

		const FHitResult OutHit = UBulletPhysicsWorldSubsystem::ConstructHitResult(E);

		const bool bWalkable = UBulletFloorQueryUtils::IsHitSurfaceWalkable(OutHit, MoverComp->GetUpDirection(), SharedSettingsPtr->MaxWalkSlopeCosine);
			
		Result.bBlockingHit = true;
		Result.bWalkableFloor = bWalkable;
		Result.FloorDist = MoverComp->GetUpDirection().Dot(StartingLocation - OutHit.ImpactPoint);
		Result.HitResult = OutHit;

		// Update the ground distance based on the slope. If you are on a slope the query might hit on an edge
		// rather than directly under the capsule. Also move back to the start location
		const float DP = OutHit.ImpactNormal.Dot(MoverComp->GetUpDirection());
		if (DP > UE_SMALL_NUMBER)
		{
			const btRigidBody* Rb = Subsystem->GetRigidBody(MoverComp->GetUpdatedComponent<UPrimitiveComponent>());
			if (!Rb) return;

			const void* P = Rb->getUserPointer();
			if (!P) return;
				
			const FBulletUserData* D = static_cast<const FBulletUserData*>(P);
			if (!D) return;
				
			Result.FloorDist += 2.0f *  D->ShapeRadius * (1.0f - DP) / DP
				+ FVector::VectorPlaneProject(ProposedLinearVelocity* DeltaTime, OutHit.ImpactNormal).Dot(MoverComp->GetUpDirection());
		}
		
		break;
	}
}

void UBulletNullMovementMode::SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState)
{
}

#undef LOCTEXT_NAMESPACE
