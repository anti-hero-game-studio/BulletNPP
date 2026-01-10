// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Core/DataTypes/BulletTypes.h"
#include "Core/Interfaces/BulletPrimitiveComponentInterface.h"
#include "BulletBoxComponent.generated.h"


UCLASS(ClassGroup=(Bullet), meta=(BlueprintSpawnableComponent), PrioritizeCategories="Shape Options")
class BULLET_API UBulletBoxComponent : public UBoxComponent, public IBulletPrimitiveComponentInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UBulletBoxComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual FBulletShapeOptions& GetShapeOptions() override {return ShapeOptions;};
	virtual const FBulletShapeOptions& GetShapeOptions() const override { return ShapeOptions; };
	
protected:
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="Shape Options")
	FBulletShapeOptions ShapeOptions;
};
