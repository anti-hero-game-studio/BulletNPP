// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMovementModeTransition.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "BulletMoverComponent.h"
#include "BulletMoverSimulationTypes.h"
#include "BulletMoverTypes.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMovementModeTransition)

const FBulletTransitionEvalResult FBulletTransitionEvalResult::NoTransition = FBulletTransitionEvalResult();

UWorld* UBulletBaseMovementModeTransition::GetWorld() const
{
	if (UBulletMoverComponent* MoverComponent = GetMoverComponent())
	{
		return MoverComponent->GetWorld();
	}
	return nullptr;
}

void UBulletBaseMovementModeTransition::OnRegistered()
{
	K2_OnRegistered();
}

void UBulletBaseMovementModeTransition::OnUnregistered()
{
	K2_OnUnregistered();
}

UBulletMoverComponent* UBulletBaseMovementModeTransition::K2_GetMoverComponent() const
{
	// Transitions can belong to either a mode or the component itself - either way they're always ultimately outer'd to a mover comp
	return GetTypedOuter<UBulletMoverComponent>();
}

FBulletTransitionEvalResult UBulletBaseMovementModeTransition::Evaluate_Implementation(const FBulletSimulationTickParams& Params) const
{
	return FBulletTransitionEvalResult::NoTransition;
}

void UBulletBaseMovementModeTransition::Trigger_Implementation(const FBulletSimulationTickParams& Params)
{
}

#if WITH_EDITOR
EDataValidationResult UBulletBaseMovementModeTransition::IsDataValid(FDataValidationContext& Context) const
{
	return EDataValidationResult::Valid;
}
#endif // WITH_EDITOR

UBulletImmediateMovementModeTransition::UBulletImmediateMovementModeTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Clear();
}

FBulletTransitionEvalResult UBulletImmediateMovementModeTransition::Evaluate_Implementation(const FBulletSimulationTickParams& Params) const
{
	if (NextMode != NAME_None)
	{
		if (bAllowModeReentry)
		{
			return FBulletTransitionEvalResult(NextMode);
		}
		else if (NextMode != Params.StartState.SyncState.MovementMode)
		{
			return FBulletTransitionEvalResult(NextMode);
		}
	}

	return FBulletTransitionEvalResult::NoTransition;
}

void UBulletImmediateMovementModeTransition::Trigger_Implementation(const FBulletSimulationTickParams& Params)
{
	Clear();
}

void UBulletImmediateMovementModeTransition::SetNextMode(FName DesiredModeName, bool bShouldReenter)
{
	NextMode = DesiredModeName;
	bAllowModeReentry = bShouldReenter;
}

void UBulletImmediateMovementModeTransition::Clear()
{
	NextMode = NAME_None;
	bAllowModeReentry = false;
}
