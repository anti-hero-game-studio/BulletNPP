// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/DataTypes/BulletTypes.h"
#include "BulletPrimitiveComponentInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class UBulletPrimitiveComponentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class BULLET_API IBulletPrimitiveComponentInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	
	virtual const FBulletShapeOptions& GetShapeOptions() const = 0;
	
	virtual FBulletShapeOptions& GetShapeOptions() = 0;
	
	virtual const FCollisionResponseContainer& GetDefaultResponseContainer() const = 0;
};
