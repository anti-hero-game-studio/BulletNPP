// Fill out your copyright notice in the Description page of Project Settings.


#include "BulletCoreSettings.h"

#include "Core/BaseClasses/BulletPhysicsActor.h"
#include "Engine/StaticMeshActor.h"

UBulletCoreSettings::UBulletCoreSettings()
{
}

#if WITH_EDITOR
void UBulletCoreSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif
