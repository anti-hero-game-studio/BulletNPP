// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/BaseClasses/BulletCapsuleComponent.h"

#include "BulletLogChannels.h"
#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"


UBulletCapsuleComponent::UBulletCapsuleComponent(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	
	SetGenerateOverlapEvents(ShapeOptions.bGenerateOverlapEventsInChaos);
}


void UBulletCapsuleComponent::InitializeComponent()
{
	Super::InitializeComponent();
	SetGenerateOverlapEvents(ShapeOptions.bGenerateOverlapEventsInChaos);
}

void UBulletCapsuleComponent::SetSimulatePhysics(const bool bSimulate)
{
	// If we are simulating physics, and we don't have a rigid body create it. If we do activate it.
	const UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	
	if (!Subsystem) return;
	
	if (Mobility != EComponentMobility::Movable)
	{
		UE_LOG(LogBullet, Error, TEXT("You are attempting to activate physics on a body not marked as movable"));
		return;
	}

	if (Subsystem->HasRigidBodyBeenCreated(this))
	{
		// Activate
		Subsystem->SetRigidBodyActiveState(this, bSimulate);
	}
	
}

bool UBulletCapsuleComponent::IsSimulatingPhysics(const FName BoneName) const
{
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return Super::IsSimulatingPhysics(BoneName);
	}
	
	const UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	
	if (!Subsystem) return false;
	
	if (Mobility != EComponentMobility::Movable)
	{
		return false;
	}
	return Subsystem->IsCollisionBodyActive(this);
}

bool UBulletCapsuleComponent::IsAnySimulatingPhysics() const
{
	return Super::IsAnySimulatingPhysics();
}

bool UBulletCapsuleComponent::IsAnyRigidBodyAwake()
{
	return IsAnySimulatingPhysics();
}

ECollisionEnabled::Type UBulletCapsuleComponent::GetCollisionEnabled() const
{
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return Super::GetCollisionEnabled();
	}
	
	const UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	
	if (!Subsystem) return Super::GetCollisionEnabled();
	
	if (!Subsystem->IsBodyValid(this))
	{
		return ECollisionEnabled::Type::NoCollision;
	}

	if (Subsystem->HasRigidBodyBeenCreated(this) && Subsystem->HasGhostBodyBeenCreated(this))
	{
		return ECollisionEnabled::Type::QueryAndPhysics;
	}
	
	if (Subsystem->HasGhostBodyBeenCreated(this) && !Subsystem->HasRigidBodyBeenCreated(this))
	{
		return ECollisionEnabled::Type::QueryOnly;
	}
	
	if (!Subsystem->HasGhostBodyBeenCreated(this) && Subsystem->HasRigidBodyBeenCreated(this))
	{
		return ECollisionEnabled::Type::PhysicsOnly;
	}
	
	
	return Super::GetCollisionEnabled();
}

ECollisionResponse UBulletCapsuleComponent::GetCollisionResponseToChannel(ECollisionChannel Channel) const
{
	const UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	
	if (!Subsystem) return Super::GetCollisionResponseToChannel(Channel);
	
	return Subsystem->GetCollisionResponseContainer(this).GetResponse(Channel);
}

const FCollisionResponseContainer& UBulletCapsuleComponent::GetCollisionResponseToChannels() const
{
	const UBulletPhysicsWorldSubsystem* Subsystem = GetWorld()->GetSubsystem<UBulletPhysicsWorldSubsystem>();
	
	if (!Subsystem) return Super::GetCollisionResponseToChannels();
	
	return Subsystem->GetCollisionResponseContainer(this);
}



// Called when the game starts
void UBulletCapsuleComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

bool UBulletCapsuleComponent::UpdateOverlapsImpl(const TOverlapArrayView* PendingOverlaps, bool bDoNotifies,
	const TOverlapArrayView* OverlapsAtEndLocation)
{
	if (!ShapeOptions.bGenerateOverlapEventsInChaos) return true;
	
	return Super::UpdateOverlapsImpl(PendingOverlaps, bDoNotifies, OverlapsAtEndLocation);
}


// Called every frame
void UBulletCapsuleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

