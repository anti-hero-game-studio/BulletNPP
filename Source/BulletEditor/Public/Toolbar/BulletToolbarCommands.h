// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletToolbarStyle.h"

class FBulletToolbarCommands : public TCommands<FBulletToolbarCommands>
{
public:

	FBulletToolbarCommands()
		: TCommands<FBulletToolbarCommands>(TEXT("Bullet"), NSLOCTEXT("Contexts", "Bullet", "Convert Actors"), NAME_None, FBulletToolbarStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};

