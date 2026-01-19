// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "../../../../../../../../EpicGames/UE_5.7/Engine/Plugins/ChaosVD/Source/ChaosVD/Public/ExtensionsSystem/ChaosVDExtension.h"
#include "Templates/SubclassOf.h"
#include "Templates/SharedPointer.h"

class FChaosVDTraceProvider;
class UActorComponent;
class SChaosVDMainTab;

/** BulletMoverCVDExtension is where we register BulletMoverCVDTab as a displayable tab, register BulletMoverCVDSimDataProcessor and give access to the BulletMoverSimDataComponent */
class FBulletMoverCVDExtension final : public FChaosVDExtension
{
public:
	
	FBulletMoverCVDExtension();
	virtual ~FBulletMoverCVDExtension() override;

	virtual void RegisterDataProcessorsInstancesForProvider(const TSharedRef<FChaosVDTraceProvider>& InTraceProvider) override;
	virtual TConstArrayView<TSubclassOf<UActorComponent>> GetSolverDataComponentsClasses() override;

	// Registers all available Tab Spawner instances in this extension, if any
	virtual void RegisterCustomTabSpawners(const TSharedRef<SChaosVDMainTab>& InParentTabWidget) override;

private:
	TArray<TSubclassOf<UActorComponent>> DataComponentsClasses;
};


