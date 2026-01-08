// Fill out your copyright notice in the Description page of Project Settings.


#include "BulletCoreSettings.h"

#include "Core/BaseClasses/BulletPhysicsActor.h"
#include "Engine/StaticMeshActor.h"

UBulletCoreSettings::UBulletCoreSettings()
{
	ActorFilter.Add(APawn::StaticClass());
	ActorFilter.Add(AStaticMeshActor::StaticClass());
	ActorFilter.Add(ABulletPhysicsActor::StaticClass());
}

#if WITH_EDITOR
void UBulletCoreSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif
