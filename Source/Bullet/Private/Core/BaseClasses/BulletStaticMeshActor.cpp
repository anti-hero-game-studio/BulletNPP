// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/BaseClasses/BulletStaticMeshActor.h"

#include "Core/BaseClasses/BulletStaticMeshComponent.h"


// Sets default values
ABulletStaticMeshActor::ABulletStaticMeshActor(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer.SetDefaultSubobjectClass(StaticMeshComponentName, UBulletStaticMeshComponent::StaticClass()))
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABulletStaticMeshActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABulletStaticMeshActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

