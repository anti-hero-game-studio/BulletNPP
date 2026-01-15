// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Core/Interfaces/BulletPrimitiveComponentInterface.h"
#include "BulletCapsuleComponent.generated.h"


UCLASS(ClassGroup=(Bullet), meta=(BlueprintSpawnableComponent), PrioritizeCategories="Bullet Physics", 
	HideCategories=(Mobility, VirtualTexture, Physics))
class BULLET_API UBulletCapsuleComponent : public UCapsuleComponent, public IBulletPrimitiveComponentInterface
{
	GENERATED_BODY()

public:
	
	UBulletCapsuleComponent(const FObjectInitializer& ObjectInitializer);
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
	
	virtual bool UpdateOverlapsImpl(const TOverlapArrayView* PendingOverlaps = nullptr, bool bDoNotifies = true, const TOverlapArrayView* OverlapsAtEndLocation = nullptr) override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	virtual FBulletShapeOptions& GetShapeOptions() override {return ShapeOptions;};
	virtual const FBulletShapeOptions& GetShapeOptions() const override { return ShapeOptions; };
	virtual const FCollisionResponseContainer& GetDefaultResponseContainer() const override { return BodyInstance.GetResponseToChannels();}
	
	
protected:
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Bullet Physics")
	FBulletShapeOptions ShapeOptions;
};
