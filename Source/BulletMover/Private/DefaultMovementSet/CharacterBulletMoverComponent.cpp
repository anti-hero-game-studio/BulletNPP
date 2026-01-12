// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/CharacterBulletMoverComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Libraries/BulletLibrary.h"
#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"
#include "DefaultMovementSet/InstantMovementEffects/BulletBasicInstantMovementEffects.h"
#include "DefaultMovementSet/Modes/BulletFallingMode.h"
#include "DefaultMovementSet/Modes/BulletFlyingMode.h"
#include "DefaultMovementSet/Modes/BulletWalkingMode.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"
#include "MoveLibrary/BulletMovementUtils.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterBulletMoverComponent)

#if !UE_BUILD_SHIPPING
FAutoConsoleVariable CVarLogSimProxyMontageReplication(
	TEXT("bullet.mover.debug.LogSimProxyMontageReplication"),
	false,
	TEXT("Whether to log detailed information about montage replication on a sim proxy using the Character-focused MoverComponent. 0: Disable, 1: Enable"),
	ECVF_Cheat);
#endif	// !UE_BUILD_SHIPPING

UCharacterBulletMoverComponent::UCharacterBulletMoverComponent()
{
	// Default movement modes
	MovementModes.Add(DefaultModeNames::Walking, CreateDefaultSubobject<UBulletWalkingMode>(TEXT("DefaultWalkingMode")));
	MovementModes.Add(DefaultModeNames::Falling, CreateDefaultSubobject<UBulletFallingMode>(TEXT("DefaultFallingMode")));
	MovementModes.Add(DefaultModeNames::Flying,  CreateDefaultSubobject<UBulletFlyingMode>(TEXT("DefaultFlyingMode")));

	StartingMovementMode = DefaultModeNames::Falling;
}

void UCharacterBulletMoverComponent::BeginPlay()
{
	Super::BeginPlay();

	OnHandlerSettingChanged();

	OnPostFinalize.AddDynamic(this, &UCharacterBulletMoverComponent::OnMoverPostFinalize);
}

bool UCharacterBulletMoverComponent::GetHandleJump() const
{
	return bHandleJump;
}

void UCharacterBulletMoverComponent::SetHandleJump(bool bInHandleJump)
{
	if (bHandleJump != bInHandleJump)
	{
		bHandleJump = bInHandleJump;
		OnHandlerSettingChanged();
	}
}

bool UCharacterBulletMoverComponent::GetHandleStanceChanges() const
{
	return bHandleStanceChanges;
}

void UCharacterBulletMoverComponent::SetHandleStanceChanges(bool bInHandleStanceChanges)
{
	if (bHandleStanceChanges != bInHandleStanceChanges)
	{
		bHandleStanceChanges = bInHandleStanceChanges;
		OnHandlerSettingChanged();
	}
}

bool UCharacterBulletMoverComponent::IsCrouching() const
{
	return HasGameplayTag(BulletMover_IsCrouching, true);
}

bool UCharacterBulletMoverComponent::IsFlying() const
{
	return HasGameplayTag(BulletMover_IsFlying, true);
}

bool UCharacterBulletMoverComponent::IsFalling() const
{
	return HasGameplayTag(BulletMover_IsFalling, true);
}

bool UCharacterBulletMoverComponent::IsAirborne() const
{
	return HasGameplayTag(BulletMover_IsInAir, true);
}

bool UCharacterBulletMoverComponent::IsOnGround() const
{
	return HasGameplayTag(BulletMover_IsOnGround, true);
}

bool UCharacterBulletMoverComponent::IsSwimming() const
{
	return HasGameplayTag(BulletMover_IsSwimming, true);
}

bool UCharacterBulletMoverComponent::IsSlopeSliding() const
{
	if (IsAirborne())
	{
		FBulletFloorCheckResult HitResult;
		const UBulletMoverBlackboard* MoverBlackboard = GetSimBlackboard();
		if (MoverBlackboard && MoverBlackboard->TryGet(CommonBlackboard::LastFloorResult, HitResult))
		{
			return HitResult.bBlockingHit && !HitResult.bWalkableFloor;
		}
	}

	return false;
}

bool UCharacterBulletMoverComponent::CanActorJump() const
{
	return IsOnGround();
}

bool UCharacterBulletMoverComponent::Jump()
{
	if (const UBulletCommonLegacyMovementSettings* CommonSettings = FindSharedSettings<UBulletCommonLegacyMovementSettings>())
	{
		TSharedPtr<FJumpImpulseEffect> JumpMove = MakeShared<FJumpImpulseEffect>();
		JumpMove->UpwardsSpeed = CommonSettings->JumpUpwardsSpeed;
		
		QueueInstantMovementEffect(JumpMove);

		return true;
	}

	return false;
}

bool UCharacterBulletMoverComponent::CanCrouch()
{
	return true;
}

void UCharacterBulletMoverComponent::Crouch()
{
	if (CanCrouch())
	{
		bWantsToCrouch = true;
	}
}

void UCharacterBulletMoverComponent::UnCrouch()
{
	bWantsToCrouch = false;
}

void UCharacterBulletMoverComponent::OnMoverPreSimulationTick(const FBulletMoverTimeStep& TimeStep, const FBulletMoverInputCmdContext& InputCmd)
{
	if (bHandleJump)
	{
		const FBulletCharacterDefaultInputs* CharacterInputs = InputCmd.Collection.FindDataByType<FBulletCharacterDefaultInputs>();
		if (CharacterInputs && CharacterInputs->bIsJumpJustPressed && CanActorJump())
		{
			Jump();
		}
	}
	
	if (bHandleStanceChanges)
	{
		const FStanceModifier* StanceModifier = static_cast<const FStanceModifier*>(FindMovementModifier(StanceModifierHandle));
		// This is a fail safe in case our handle was bad - try finding the modifier by type if we can
		if (!StanceModifier)
		{
			StanceModifier = FindMovementModifierByType<FStanceModifier>();
		}
	
		EStanceMode OldActiveStance = EStanceMode::Invalid;
		if (StanceModifier)
		{
			OldActiveStance = StanceModifier->ActiveStance;
		}
	
		const bool bIsCrouching = HasGameplayTag(BulletMover_IsCrouching, true);
		if (bIsCrouching && (!bWantsToCrouch || !CanCrouch()))
		{	
			if (StanceModifier && StanceModifier->CanExpand(this))
			{
				CancelModifierFromHandle(StanceModifier->GetHandle());
				StanceModifierHandle.Invalidate();

				StanceModifier = nullptr;
			}
		}
		else if (!bIsCrouching && bWantsToCrouch && CanCrouch())
		{
			TSharedPtr<FStanceModifier> NewStanceModifier = MakeShared<FStanceModifier>();
			StanceModifierHandle = QueueMovementModifier(NewStanceModifier);

			StanceModifier = NewStanceModifier.Get();
		}
	
		EStanceMode NewActiveStance = EStanceMode::Invalid;
		if (StanceModifier)
		{
			NewActiveStance = StanceModifier->ActiveStance;
		}

		if (OldActiveStance != NewActiveStance)
		{
			OnStanceChanged.Broadcast(OldActiveStance, NewActiveStance);
		}
	}
}

void UCharacterBulletMoverComponent::OnMoverPostFinalize(const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState)
{
	UpdateSyncedMontageState(GetLastTimeStep(), SyncState, AuxState);
}

void UCharacterBulletMoverComponent::OnHandlerSettingChanged()
{
	const bool bIsHandlingAnySettings = bHandleJump || bHandleStanceChanges;

	if (bIsHandlingAnySettings)
	{
		OnPreSimulationTick.AddUniqueDynamic(this, &UCharacterBulletMoverComponent::OnMoverPreSimulationTick);
	}
	else
	{
		OnPreSimulationTick.RemoveDynamic(this, &UCharacterBulletMoverComponent::OnMoverPreSimulationTick);
	}
}

void UCharacterBulletMoverComponent::UpdateSyncedMontageState(const FBulletMoverTimeStep& TimeStep, const FBulletMoverSyncState& SyncState, const FBulletMoverAuxStateContext& AuxState)
{
	if (GetOwnerRole() == ROLE_SimulatedProxy)
	{
		const FBulletLayeredMove_MontageStateProvider* MontageStateProvider = static_cast<const FBulletLayeredMove_MontageStateProvider*>(SyncState.LayeredMoves.FindActiveMove(FBulletLayeredMove_MontageStateProvider::StaticStruct()));

		bool bShouldStopSyncedMontage = false;
		bool bShouldStartNewMontage = false;
		FBulletMoverAnimMontageState NewMontageState;

		if (SyncedMontageState.Montage)
		{
			if (MontageStateProvider)
			{
				NewMontageState = MontageStateProvider->GetMontageState();

				if (NewMontageState.Montage != SyncedMontageState.Montage)
				{
					bShouldStartNewMontage = true;
					bShouldStopSyncedMontage = true;
				}
			}
			else
			{
				bShouldStopSyncedMontage = true;
			}
		}
		else // We aren't actively syncing a montage state yet
		{
			if (MontageStateProvider)
			{
				// We have just received a montage state to sync against
				NewMontageState = MontageStateProvider->GetMontageState();
				bShouldStartNewMontage = true;
			}
		}

		if (bShouldStopSyncedMontage || bShouldStartNewMontage)
		{
			const USkeletalMeshComponent* MeshComp = Cast<USkeletalMeshComponent>(GetPrimaryVisualComponent());
			UAnimInstance* MeshAnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;

			if (bShouldStopSyncedMontage)
			{
				#if !UE_BUILD_SHIPPING
				UE_CLOG(CVarLogSimProxyMontageReplication->GetBool(), LogBulletMover, Log, TEXT("BulletMover SP montage repl (SimF %i SimT: %.3f): STOP %s"),
					TimeStep.ServerFrame, TimeStep.BaseSimTimeMs, *SyncedMontageState.Montage->GetName());
				#endif // !UE_BUILD_SHIPPING

				if (MeshAnimInstance)
				{
					MeshAnimInstance->Montage_Stop(SyncedMontageState.Montage->GetDefaultBlendOutTime(), SyncedMontageState.Montage);
				}

				SyncedMontageState.Reset();
			}

			if (bShouldStartNewMontage && NewMontageState.Montage && MeshAnimInstance)
			{
				const float StartPosition = NewMontageState.CurrentPosition;
				const float PlaySeconds = MeshAnimInstance->Montage_Play(NewMontageState.Montage, NewMontageState.PlayRate, EMontagePlayReturnType::MontageLength, StartPosition);

				#if !UE_BUILD_SHIPPING
				UE_CLOG(CVarLogSimProxyMontageReplication->GetBool(), LogBulletMover, Log, TEXT("BulletMover SP montage repl (SimF %i SimT: %.3f): PLAY %s (StartPos: %.3f  Rate: %.3f  PlaySecs: %.3f)"),
					TimeStep.ServerFrame, TimeStep.BaseSimTimeMs, *NewMontageState.Montage->GetName(), StartPosition, NewMontageState.PlayRate, PlaySeconds);
				#endif // !UE_BUILD_SHIPPING

				if (PlaySeconds > 0.0f)
				{
					SyncedMontageState = NewMontageState;	// only consider us sync'd if the montage actually started
				}
			}
		}
	}
}

#pragma region BULLET PHYSICS
void UCharacterBulletMoverComponent::InitializeBulletCharacter()
{
	UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogBulletMover, Error, TEXT("Could not find the Physics World Subsystem"))
		return;
	}
	
	if (UBulletPhysicsWorldSubsystem* B = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>())
	{
		// TODO:@GreggoryAddison::CodeCompletion || Add support for kinematic mover
		B->RegisterBulletRigidBody(GetOwner());
	}
	
}

void UCharacterBulletMoverComponent::InitializeWithBullet()
{
	Super::InitializeWithBullet();
	InitializeBulletCharacter();
}

#pragma endregion
