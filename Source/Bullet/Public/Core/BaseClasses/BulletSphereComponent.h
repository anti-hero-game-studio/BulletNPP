// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Core/DataTypes/BulletTypes.h"
#include "Core/Interfaces/BulletPrimitiveComponentInterface.h"
#include "BulletSphereComponent.generated.h"


UCLASS(ClassGroup=(Bullet), meta=(BlueprintSpawnableComponent), PrioritizeCategories="Shape Options")
class BULLET_API UBulletSphereComponent : public USphereComponent, public IBulletPrimitiveComponentInterface
{
	GENERATED_BODY()

public:
	UBulletSphereComponent(const FObjectInitializer& ObjectInitializer);
	virtual void InitializeComponent() override;
	
	
	virtual void SetSimulatePhysics(bool bSimulate) override;
	virtual bool IsSimulatingPhysics(FName BoneName = NAME_None) const override;
	virtual bool IsAnySimulatingPhysics() const override;
	virtual bool IsAnyRigidBodyAwake() override;
	
	virtual ECollisionEnabled::Type GetCollisionEnabled() const override;
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const override;
	virtual const FCollisionResponseContainer& GetCollisionResponseToChannels() const override;
	

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual FBulletShapeOptions& GetShapeOptions() override {return ShapeOptions;};
	virtual const FBulletShapeOptions& GetShapeOptions() const override { return ShapeOptions; };
	
protected:
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Shape Options")
	FBulletShapeOptions ShapeOptions;
};
