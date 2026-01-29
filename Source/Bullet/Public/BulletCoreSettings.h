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
	
	// ---- Configure history size (frames) ----
	UPROPERTY(config, EditAnywhere, Category="Bullet|Rollback")
	int32 StateHistorySizeFrames = 256;
	
	// If true, SaveState only stores dynamic/kinematic bodies on the client and reduces memory on the server;
	UPROPERTY(config, EditAnywhere, Category="Bullet|Rollback")
	bool bOnlyStoreSnapshotsClientSide = true;

	// If true, SaveState only stores dynamic/kinematic bodies (skips static)
	UPROPERTY(config, EditAnywhere, Category="Bullet|Rollback")
	bool bSnapshotOnlyMovers = true;

	// If true, RestoreState also restores interpolation fields
	UPROPERTY(config, EditAnywhere, Category="Bullet|Rollback")
	bool bRestoreInterpolationState = true;

	// If true, RestoreState forces bodies awake if they were awake in snapshot (helps avoid divergence)
	UPROPERTY(config, EditAnywhere, Category="Bullet|Rollback")
	bool bRestoreActivationState = true;
	
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
