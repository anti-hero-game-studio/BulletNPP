// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/PlayBulletMoverMontageCallbackProxy.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "BulletMoverComponent.h"
#include "DefaultMovementSet/LayeredMoves/BulletAnimRootMotionLayeredMove.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlayBulletMoverMontageCallbackProxy)

UPlayBulletMoverMontageCallbackProxy::UPlayBulletMoverMontageCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPlayBulletMoverMontageCallbackProxy* UPlayBulletMoverMontageCallbackProxy::CreateProxyObjectForPlayMoverMontage(
	class UBulletMoverComponent* InMoverComponent,
	class UAnimMontage* MontageToPlay,
	float PlayRate,
	float StartingPosition,
	FName StartingSection)
{
	USkeletalMeshComponent* SkelMeshComp = InMoverComponent ? InMoverComponent->GetOwner()->GetComponentByClass<USkeletalMeshComponent>() : nullptr;

	UPlayBulletMoverMontageCallbackProxy* Proxy = NewObject<UPlayBulletMoverMontageCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->PlayMoverMontage(InMoverComponent, SkelMeshComp, MontageToPlay, PlayRate, StartingPosition, StartingSection);

	return Proxy;
}


bool UPlayBulletMoverMontageCallbackProxy::PlayMoverMontage(
	UBulletMoverComponent* InMoverComponent,
	USkeletalMeshComponent* InSkeletalMeshComponent,
	UAnimMontage* MontageToPlay,
	float PlayRate,
	float StartingPosition,
	FName StartingSection)
{
	bool bDidPlay = PlayMontage(InSkeletalMeshComponent, MontageToPlay, PlayRate, StartingPosition, StartingSection);

	if (bDidPlay && PlayRate != 0.f && MontageToPlay->HasRootMotion())
	{
		if (UAnimInstance* AnimInstance = InSkeletalMeshComponent->GetAnimInstance())
		{
			if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
			{
				// Listen for possible ways the montage could end
				OnCompleted.AddUniqueDynamic(this, &UPlayBulletMoverMontageCallbackProxy::OnMoverMontageEnded);
				OnInterrupted.AddUniqueDynamic(this, &UPlayBulletMoverMontageCallbackProxy::OnMoverMontageEnded);

				// Disable the actual animation-driven root motion, in favor of our own layered move
				MontageInstance->PushDisableRootMotion();

				const float StartingMontagePosition = MontageInstance->GetPosition();	// position in seconds, disregarding PlayRate

				// Queue a layered move to perform the same anim root motion over the same time span
				TSharedPtr<FBulletLayeredMove_AnimRootMotion> AnimRootMotionMove = MakeShared<FBulletLayeredMove_AnimRootMotion>();
				AnimRootMotionMove->MontageState.Montage = MontageToPlay;
				AnimRootMotionMove->MontageState.PlayRate = PlayRate;
				AnimRootMotionMove->MontageState.StartingMontagePosition = StartingMontagePosition;
				AnimRootMotionMove->MontageState.CurrentPosition = StartingMontagePosition;
				
				float RemainingUnscaledMontageSeconds(0.f);

				if (PlayRate > 0.f)
				{
					// playing forwards, so working towards the end of the montage
					RemainingUnscaledMontageSeconds = MontageToPlay->GetPlayLength() - StartingMontagePosition;
				}
				else
				{
					// playing backwards, so working towards the start of the montage
					RemainingUnscaledMontageSeconds = StartingMontagePosition;	
				}

				AnimRootMotionMove->DurationMs = (RemainingUnscaledMontageSeconds / FMath::Abs(PlayRate)) * 1000.f;

				InMoverComponent->QueueLayeredMove(AnimRootMotionMove);
			}
		}
	}

	return bDidPlay;
}

void UPlayBulletMoverMontageCallbackProxy::OnMoverMontageEnded(FName IgnoredNotifyName)
{
	// TODO: this is where we'd want to schedule the ending of the associated move, whether the montage instance was interrupted or ended

	UnbindMontageDelegates();
}

void UPlayBulletMoverMontageCallbackProxy::UnbindMontageDelegates()
{
	OnCompleted.RemoveDynamic(this, &UPlayBulletMoverMontageCallbackProxy::OnMoverMontageEnded);
	OnInterrupted.RemoveDynamic(this, &UPlayBulletMoverMontageCallbackProxy::OnMoverMontageEnded);
}


void UPlayBulletMoverMontageCallbackProxy::BeginDestroy()
{
	UnbindMontageDelegates();

	Super::BeginDestroy();
}
