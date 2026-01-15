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
UCLASS(ClassGroup=(Bullet), meta=(BlueprintSpawnableComponent), PrioritizeCategories="Bullet Physics", 
	HideCategories=(Mobility, VirtualTexture, Physics))
class BULLET_API UBulletStaticMeshComponent : public UStaticMeshComponent, public IBulletPrimitiveComponentInterface
{
	GENERATED_BODY()

public:

	UBulletStaticMeshComponent(const FObjectInitializer& ObjectInitializer);
	virtual void InitializeComponent() override;
	
	virtual void SetSimulatePhysics(bool bSimulate) override;
	virtual bool IsSimulatingPhysics(FName BoneName = NAME_None) const override;
	virtual bool IsAnySimulatingPhysics() const override;
	virtual bool IsAnyRigidBodyAwake() override;
	
	virtual ECollisionEnabled::Type GetCollisionEnabled() const override;
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const override;
	virtual const FCollisionResponseContainer& GetCollisionResponseToChannels() const override;
	
	
	virtual FBulletShapeOptions& GetShapeOptions() override {return ShapeOptions;};
	virtual const FBulletShapeOptions& GetShapeOptions() const override { return ShapeOptions; };
	virtual const FCollisionResponseContainer& GetDefaultResponseContainer() const override { return BodyInstance.GetResponseToChannels();}
	
	
protected:
	
	virtual bool UpdateOverlapsImpl(const TOverlapArrayView* PendingOverlaps = nullptr, bool bDoNotifies = true, const TOverlapArrayView* OverlapsAtEndLocation = nullptr) override;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Bullet Physics")
	FBulletShapeOptions ShapeOptions;
};
