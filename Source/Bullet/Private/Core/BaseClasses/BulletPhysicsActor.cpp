// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/BaseClasses/BulletPhysicsActor.h"


// Sets default values
ABulletPhysicsActor::ABulletPhysicsActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABulletPhysicsActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABulletPhysicsActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

