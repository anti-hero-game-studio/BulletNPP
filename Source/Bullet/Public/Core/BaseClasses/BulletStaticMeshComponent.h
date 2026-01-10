// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DataTypes/BulletTypes.h"
#include "Core/Interfaces/BulletPrimitiveComponentInterface.h"
#include "BulletStaticMeshComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup=(Bullet), meta=(BlueprintSpawnableComponent), PrioritizeCategories="Shape Options")
class BULLET_API UBulletStaticMeshComponent : public UStaticMeshComponent, public IBulletPrimitiveComponentInterface
{
	GENERATED_BODY()
	
	
	
public:
	virtual FBulletShapeOptions& GetShapeOptions() override {return ShapeOptions;};
	virtual const FBulletShapeOptions& GetShapeOptions() const override { return ShapeOptions; };
	
protected:
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="Shape Options")
	FBulletShapeOptions ShapeOptions;
};
