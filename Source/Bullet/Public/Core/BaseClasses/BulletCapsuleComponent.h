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
	
	virtual FBulletPhysicsBodySettings& GetBulletPhysicsBodySettings() override {return BulletPhysicsBodySettings;};
	virtual const FBulletPhysicsBodySettings& GetBulletPhysicsBodySettings() const override { return BulletPhysicsBodySettings; };
	virtual const FCollisionResponseContainer& GetDefaultResponseContainer() const override { return BodyInstance.GetResponseToChannels();}
	
	virtual float GetGroundTraceDistance() const override;
	virtual float GetShapeHeight() const override;
	virtual float GetShapeWidth() const override;
	virtual float GetShapeStepHeightRatio() const override {return StepHeightRatio;}
	
	virtual void WakeRigidBody(FName BoneName = NAME_None) override;
	
	
protected:
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Bullet Physics")
	FBulletPhysicsBodySettings BulletPhysicsBodySettings;
	
	/* This requires your collision shape to be a child of the root component allowing for a collider that floats above the ground
	* Currently not implemented.
	 */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="Shape Options")
	bool bUseFloatingShape = false;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Shape Options")
	float ColliderHeight = 88.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Shape Options")
	float ColliderRadius = 44.f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Shape Options", meta=(ClampMin="0", ClampMax="1"))
	float StepHeightRatio = 0.1f;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Shape Options")
	FVector ColliderOffset = FVector::Zero();

	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	void RecalculateCollider();
	
#endif
};
