// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FChaosVDExtension;

/** This module contains the Editor side of the BulletMover support in Chaos Visual Debugger (CVD)
* BulletMover is a module responsible for the movement of actors and its replication, for instance the input controlled movement of characters
* This module allows the display and recording specific data in CVD that makes it easier to understand and debug BulletMover
* BulletMoverCVDTab is the tab in CVD where that information is displayed
* BulletMoverCVDSimDataProcessor is the receiving and processing end of the BulletMover data trace ("Sim Data") that the game sends to CVD
* BulletMoverSimDataComponent is a component holding BulletMover data for the current visualized frame
* BulletMoverCVDExtension is where we register BulletMoverCVDTab as a displayable tab, register BulletMoverCVDSimDataProcessor and give access to the BulletMoverSimDataComponent
*/
class FBulletMoverCVDEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:

	TArray<TWeakPtr<FChaosVDExtension>> AvailableExtensions;
};
