// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/NavBulletMoverComponent.h"
#include "AI/NavigationSystemBase.h"
#include "AI/Navigation/PathFollowingAgentInterface.h"
#include "Components/CapsuleComponent.h"
#include "DefaultMovementSet/InstantMovementEffects/BulletBasicInstantMovementEffects.h"
#include "DefaultMovementSet/Settings/BulletCommonLegacyMovementSettings.h"
#include "MoveLibrary/BulletMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavBulletMoverComponent)

UNavBulletMoverComponent::UNavBulletMoverComponent()
{
	bWantsInitializeComponent = true;
	bAutoActivate = true;
}

void UNavBulletMoverComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (const AActor* MovementCompOwner = GetOwner())
	{
		MoverComponent = MovementCompOwner->FindComponentByClass<UBulletMoverComponent>();
	}
	
	if (!MoverComponent.IsValid())
	{
		UE_LOG(LogBulletMover, Warning, TEXT("NavMoverComponent on %s could not find a valid MoverComponent and will not function properly."), *GetNameSafe(GetOwner()));
	}
}

void UNavBulletMoverComponent::BeginPlay()
{
	Super::BeginPlay();

	if (MoverComponent.IsValid() && MoverComponent->GetUpdatedComponent())
	{
		UpdateNavAgent(*MoverComponent->GetUpdatedComponent());
	}
	else
	{
		UpdateNavAgent(*GetOwner());
	}
}

float UNavBulletMoverComponent::GetMaxSpeedForNavMovement() const
{
	float MaxSpeed = 0.0f;
	
	if (MoverComponent.IsValid())
	{
		if (const UBulletCommonLegacyMovementSettings* MovementSettings = MoverComponent.Get()->FindSharedSettings_Mutable<UBulletCommonLegacyMovementSettings>())
		{
			MaxSpeed = MovementSettings->MaxSpeed;
		}
	}

	return MaxSpeed;
}

void UNavBulletMoverComponent::StopMovementImmediately()
{
	if (MoverComponent.IsValid())
	{
		TSharedPtr<FBulletApplyVelocityEffect> VelocityEffect = MakeShared<FBulletApplyVelocityEffect>();
		MoverComponent->QueueInstantMovementEffect(VelocityEffect);
	}
	
	CachedNavMoveInputIntent = FVector::ZeroVector;
	CachedNavMoveInputVelocity = FVector::ZeroVector;
}

bool UNavBulletMoverComponent::ConsumeNavMovementData(FVector& OutMoveInputIntent, FVector& OutMoveInputVelocity)
{
	const bool bHasFrameAdvanced = GFrameCounter > GameFrameNavMovementConsumed;
	const bool bNoNewRequests = GameFrameNavMovementConsumed > GameFrameNavMovementRequested;
	bool bHasNavMovement = false;
	
	if (bHasFrameAdvanced && bNoNewRequests)
	{
		CachedNavMoveInputIntent = FVector::ZeroVector;
		CachedNavMoveInputVelocity = FVector::ZeroVector;
	}
	else
	{
		OutMoveInputIntent = CachedNavMoveInputIntent;
		OutMoveInputVelocity = CachedNavMoveInputVelocity;
		bHasNavMovement = true;
		
		UE_LOG(LogBulletMover, VeryVerbose, TEXT("Applying %s as NavMoveInputIntent."), *CachedNavMoveInputIntent.ToString());
		UE_LOG(LogBulletMover, VeryVerbose, TEXT("Applying %s as NavMoveInputVelocity."), *CachedNavMoveInputVelocity.ToString());
	}

	GameFrameNavMovementConsumed = GFrameCounter;

	return bHasNavMovement;
}

FVector UNavBulletMoverComponent::GetLocation() const
{
	if (MoverComponent.IsValid())
	{
		if (const USceneComponent* UpdatedComponent = MoverComponent->GetUpdatedComponent())
		{
			return UpdatedComponent->GetComponentLocation();
		}
	}
	
	return FVector(FLT_MAX);
}

FVector UNavBulletMoverComponent::GetFeetLocation() const
{
	if (MoverComponent.IsValid())
	{
		if (const USceneComponent* UpdatedComponent = MoverComponent->GetUpdatedComponent())
		{
			return UpdatedComponent->GetComponentLocation() - FVector(0,0,UpdatedComponent->Bounds.BoxExtent.Z);
		}
	}

	return FNavigationSystem::InvalidLocation;
}

FVector UNavBulletMoverComponent::GetFeetLocationAt(FVector ComponentLocation) const
{
	if (MoverComponent.IsValid())
	{
		if (const USceneComponent* UpdatedComponent = MoverComponent->GetUpdatedComponent())
		{
			return ComponentLocation - FVector(0, 0, UpdatedComponent->Bounds.BoxExtent.Z);
		}
	}
	
	return FNavigationSystem::InvalidLocation;
}

FBasedPosition UNavBulletMoverComponent::GetFeetLocationBased() const
{
	FBasedPosition BasedPosition(NULL, GetFeetLocation());
	
	if (MoverComponent.IsValid())
	{
		if (const UBulletMoverBlackboard* Blackboard = MoverComponent->GetSimBlackboard())
		{
			FBulletRelativeBaseInfo MovementBaseInfo;
			if (Blackboard->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, MovementBaseInfo)) 
			{
				BasedPosition.Base = MovementBaseInfo.MovementBase->GetOwner();
				BasedPosition.Position = MovementBaseInfo.Location;
				BasedPosition.CachedBaseLocation = MovementBaseInfo.ContactLocalPosition;
				BasedPosition.CachedBaseRotation = MovementBaseInfo.Rotation.Rotator();
			}
		}
	}

	return BasedPosition;
}

void UNavBulletMoverComponent::UpdateNavAgent(const UObject& ObjectToUpdateFrom)
{
	if (!NavMovementProperties.bUpdateNavAgentWithOwnersCollision)
	{
		return;
	}
	
	if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(&ObjectToUpdateFrom))
	{
		NavAgentProps.AgentRadius = CapsuleComponent->GetScaledCapsuleRadius();
		NavAgentProps.AgentHeight = CapsuleComponent->GetScaledCapsuleHalfHeight() * 2.f;;
	}
	else if (const AActor* ObjectAsActor = Cast<AActor>(&ObjectToUpdateFrom))
	{
		ensureMsgf(&ObjectToUpdateFrom == GetOwner(), TEXT("Object passed to UpdateNavAgent should be the owner actor of the Nav Movement Component"));
		// Can't call GetSimpleCollisionCylinder(), because no components will be registered.
		float BoundRadius, BoundHalfHeight;	
		ObjectAsActor->GetSimpleCollisionCylinder(BoundRadius, BoundHalfHeight);
		NavAgentProps.AgentRadius = BoundRadius;
		NavAgentProps.AgentHeight = BoundHalfHeight * 2.f;
	}
}

void UNavBulletMoverComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	if (MoveVelocity.SizeSquared() < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	GameFrameNavMovementRequested = GFrameCounter;
	
	if (IsFalling())
	{
		const FVector FallVelocity = MoveVelocity.GetClampedToMaxSize(GetMaxSpeedForNavMovement());
		// TODO: NS - we may eventually need something to help with air control and pathfinding
		//PerformAirControlForPathFollowing(FallVelocity, FallVelocity.Z);
		CachedNavMoveInputVelocity = FallVelocity;
		return;
	}

	CachedNavMoveInputVelocity = MoveVelocity;
	
	if (IsMovingOnGround())
	{
		const FPlane MovementPlane(FVector::ZeroVector, FVector::UpVector);
		CachedNavMoveInputVelocity = UBulletMovementUtils::ConstrainToPlane(CachedNavMoveInputVelocity, MovementPlane, true);
	}
}

void UNavBulletMoverComponent::RequestPathMove(const FVector& MoveInput)
{
	FVector AdjustedMoveInput(MoveInput);

	// preserve magnitude when moving on ground/falling and requested input has Z component
	// see ConstrainInputAcceleration for details
	if (MoveInput.Z != 0.f && (IsMovingOnGround() || IsFalling()))
	{
		const float Mag = MoveInput.Size();
		AdjustedMoveInput = MoveInput.GetSafeNormal2D() * Mag;
	}

	GameFrameNavMovementRequested = GFrameCounter;
	CachedNavMoveInputIntent = AdjustedMoveInput.GetSafeNormal();
}

bool UNavBulletMoverComponent::CanStopPathFollowing() const
{
	return true;
}

void UNavBulletMoverComponent::SetPathFollowingAgent(IPathFollowingAgentInterface* InPathFollowingAgent)
{
	PathFollowingComp = InPathFollowingAgent;
}

IPathFollowingAgentInterface* UNavBulletMoverComponent::GetPathFollowingAgent()
{
	return PathFollowingComp.Get();
}

const IPathFollowingAgentInterface* UNavBulletMoverComponent::GetPathFollowingAgent() const
{
	return PathFollowingComp.Get();
}

const FNavAgentProperties& UNavBulletMoverComponent::GetNavAgentPropertiesRef() const
{
	return NavAgentProps;
}

FNavAgentProperties& UNavBulletMoverComponent::GetNavAgentPropertiesRef()
{
	return NavAgentProps;
}

void UNavBulletMoverComponent::ResetMoveState()
{
	MovementState = NavAgentProps;
}

bool UNavBulletMoverComponent::CanStartPathFollowing() const
{
	return true;
}

bool UNavBulletMoverComponent::IsCrouching() const
{
	return MoverComponent.IsValid() ? MoverComponent->HasGameplayTag(BulletMover_IsCrouching, true) : false;
}

bool UNavBulletMoverComponent::IsFalling() const
{
	return MoverComponent.IsValid() ? MoverComponent->HasGameplayTag(BulletMover_IsFalling, true) : false;
}

bool UNavBulletMoverComponent::IsMovingOnGround() const
{
	return MoverComponent.IsValid() ? MoverComponent->HasGameplayTag(BulletMover_IsOnGround, true) : false;
}

bool UNavBulletMoverComponent::IsSwimming() const
{
	return MoverComponent.IsValid() ? MoverComponent->HasGameplayTag(BulletMover_IsSwimming, true) : false;
}

bool UNavBulletMoverComponent::IsFlying() const
{
	return MoverComponent.IsValid() ? MoverComponent->HasGameplayTag(BulletMover_IsFlying, true) : false;
}

void UNavBulletMoverComponent::GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const
{
	GetOwner()->GetSimpleCollisionCylinder(CollisionRadius, CollisionHalfHeight);
}

FVector UNavBulletMoverComponent::GetSimpleCollisionCylinderExtent() const
{
	return GetOwner()->GetSimpleCollisionCylinderExtent();
}

FVector UNavBulletMoverComponent::GetForwardVector() const
{
	return GetOwner()->GetActorForwardVector();
}

FVector UNavBulletMoverComponent::GetVelocityForNavMovement() const
{
	return MoverComponent.IsValid() ? MoverComponent->GetVelocity() : FVector::ZeroVector;
}
