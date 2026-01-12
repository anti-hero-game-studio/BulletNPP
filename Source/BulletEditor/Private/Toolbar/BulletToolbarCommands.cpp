// Fill out your copyright notice in the Description page of Project Settings.


#include "Toolbar/BulletToolbarCommands.h"
#define LOCTEXT_NAMESPACE "FNewPluginModule"

void FBulletToolbarCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "Bullet", "Convert Actors", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE