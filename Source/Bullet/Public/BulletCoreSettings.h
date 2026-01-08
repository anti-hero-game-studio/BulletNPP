// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletCoreSettings.generated.h"

/**
 * 
 */
UCLASS(config=Bullet, defaultconfig, meta=(DisplayName="Bullet Core"))
class BULLET_API UBulletCoreSettings : public UObject
{
	GENERATED_BODY()
	
public:
	UBulletCoreSettings();
	
	/* A list of classes allowed to be automatically added to the bullet physics world.
	 *
	 */
	UPROPERTY(config, EditAnywhere, Category = "Bullet")
	TArray<TSubclassOf<AActor>> ActorFilter;
	
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
