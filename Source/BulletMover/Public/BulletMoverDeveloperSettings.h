// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"

#include "BulletMoverDeveloperSettings.generated.h"

#define UE_API BULLETMOVER_API

/** Developer settings for the Mover plugin */
UCLASS(MinimalAPI, config = Engine, defaultconfig, meta = (DisplayName = "BulletMover Settings"))
class UBulletMoverDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()
public:
	UE_API UBulletMoverDeveloperSettings();
	
	/**
     * This specifies the number of times a movement mode can refund all of the time in a substep before we back out to avoid freezing the game/editor
     */
    UPROPERTY(config, EditAnywhere, Category = "BulletMover")
    int32 MaxTimesToRefundSubstep;
	
};

#undef UE_API
