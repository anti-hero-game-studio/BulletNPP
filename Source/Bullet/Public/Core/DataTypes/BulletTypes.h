// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletMain.h"
#include "BulletTypes.generated.h"


USTRUCT(BlueprintType)
struct FUnrealShapeId
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	int32 BlockingShapeWorldArrayIndex = INDEX_NONE;
	
	UPROPERTY(BlueprintReadOnly)
	int32 OverlappingShapeWorldArrayIndex = INDEX_NONE;
};

USTRUCT()
struct FUnrealShape
{
	GENERATED_BODY()
	
	FUnrealShape()
	{
		
	}
	
	FUnrealShape(UPrimitiveComponent* NewPrimitive)
	{
		bIsRootComponent = false;
		Shape = NewPrimitive;
		BlockingCollider = nullptr;
		OverlappingCollider = nullptr;
	}
	
	uint8 bIsRootComponent : 1 = false;
	FUnrealShapeId Id;
	TWeakObjectPtr<UPrimitiveComponent> Shape = nullptr;
	btCollisionObject* BlockingCollider = nullptr;
	btGhostObject* OverlappingCollider = nullptr;
	FCollisionResponseContainer CollisionResponses;
	float ShapeRadius = 0.f; // X
	float ShapeWidth = 0.f; // Y
	float ShapeHeight = 0.f;// Z
};


USTRUCT()
struct FUnrealShapeDescriptor
{
	GENERATED_BODY()
	
	FUnrealShapeDescriptor()
	{
		
	}
	
	TWeakObjectPtr<AActor> ShapeOwner = nullptr;
	
	TArray<FUnrealShape> Shapes;
	
	FCollisionResponseContainer CollisionResponseContainer = FCollisionResponseContainer();
	
	
	
	
	void Add(UPrimitiveComponent* C, const bool& bIsRoot)
	{
		Shapes.Add(FUnrealShape(C));
		Shapes.Last().bIsRootComponent = bIsRoot;
	}
	
	UPrimitiveComponent* GetRootComponent() const
	{
		for (const FUnrealShape& Shape : Shapes)
		{
			if (!Shape.bIsRootComponent) continue;
			
			return Shape.Shape.Get();
		}
		
		return nullptr;
	}
	
	btCollisionObject* GetRootBlockingCollider() const
	{
		for (const FUnrealShape& Shape : Shapes)
		{
			if (!Shape.bIsRootComponent) continue;
			
			return Shape.BlockingCollider;
		}
		
		return nullptr;
	}
	
	btGhostObject* GetRootOverlappingCollider() const
	{
		for (const FUnrealShape& Shape : Shapes)
		{
			if (!Shape.bIsRootComponent) continue;
			
			return Shape.OverlappingCollider;
		}
		
		return nullptr;
	}
	
	TArray<btGhostObject*> GetAllOverlappingColliders() const
	{
		TArray<btGhostObject*> OverlappingColliders;
		for (const FUnrealShape& Shape : Shapes)
		{
			OverlappingColliders.Add(Shape.OverlappingCollider);
		}
		
		return OverlappingColliders;
	}
	
	int GetRootColliderId() const
	{
		for (const FUnrealShape& Shape : Shapes)
		{
			if (!Shape.bIsRootComponent) continue;
			
			return Shape.Id.BlockingShapeWorldArrayIndex;
		}
		
		return INDEX_NONE;
	}
	
	UPrimitiveComponent* FindClosestPrimitive(const FVector& Location) const
	{
		float Distance = TNumericLimits<float>::Max();
		UPrimitiveComponent* NearestComponent = nullptr;
		for (const FUnrealShape& S : Shapes)
		{
			TWeakObjectPtr<UPrimitiveComponent> Shape = S.Shape;
			
			if (!Shape.Get()) continue;
			
			float CurrentDistance = 0.f;
			
			CurrentDistance = FVector::Distance(Shape.Get()->GetComponentLocation(), Location);

			if (CurrentDistance < Distance)
			{
				Distance = CurrentDistance;
				NearestComponent = Shape.Get();
			}
		}

		return NearestComponent;
	}
	
	int32 Find(const UPrimitiveComponent* T, const bool bFindBlockingShape = true) const
	{
		for (const FUnrealShape& S : Shapes)
		{
			if (!S.Shape.Get()) continue;
			if (S.Shape.Get() != T) continue;
			return bFindBlockingShape ? S.Id.BlockingShapeWorldArrayIndex : S.Id.OverlappingShapeWorldArrayIndex;
		}
		
		return INDEX_NONE;
	}
	
	const FCollisionResponseContainer& GetCollisionResponseContainer(const UPrimitiveComponent* Target) const
	{
		for (const FUnrealShape& S : Shapes)
		{
			if (!S.Shape.Get()) continue;
			if (S.Shape.Get() != Target) continue;
			return S.CollisionResponses;
		}
		
		return CollisionResponseContainer;
	}
	
};


UENUM(BlueprintType)
enum class EBulletShapeType : uint8
{
	STATIC = 0,
	DYNAMIC = 1,
	KINEMATIC = 2,
};



USTRUCT(BlueprintType)
struct FBulletShapeOptions
{
	GENERATED_BODY()
	
	FBulletShapeOptions()
	{
		
	}
	
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EBulletShapeType ShapeType = EBulletShapeType::STATIC;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bAutomaticallyActivate = false;
	
	/* Useful for player controlled bodies that should never be sent to sleep*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDisableDeactivation = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bUsePhysicsMaterial = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bGenerateOverlapEventsInBullet = true;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bGenerateOverlapEventsInChaos = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bGenerateCollisionEventsInBullet = true;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bGenerateCollisionEventsInChaos = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bKeepShapeVertical = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(EditCondition="bUsePhysicsMaterial", EditConditionHides))
	TObjectPtr<UPhysicalMaterial> PhysMaterial;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(EditCondition="!bUsePhysicsMaterial", EditConditionHides))
	float Restitution = 1.f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(EditCondition="!bUsePhysicsMaterial", EditConditionHides))
	float Friction = 1.f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Mass = 10.f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(EditCondition="ShapeType != EBulletShapeType::STATIC", EditConditionHides))
	bool bHasGravityOverride = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(EditCondition="bHasGravityOverride && ShapeType != EBulletShapeType::STATIC", EditConditionHides))
	FVector GravityOverride = FVector(0, 0, -980.f);
	
};


struct FBulletUserData
{
	static constexpr uint32 MagicValue = 0xB011E7DA; // any constant you like

	uint32 Magic = MagicValue;

	// For hit construction/gameplay (not used by collision filtering)
	USceneComponent* Component = nullptr;
	
	float ShapeRadius = 1.f;
	float ShapeWidth = 1.f;
	float ShapeHeight = 1.f;

	float DefaultSlidingFriction = 0.f;
	float DefaultRollingFriction = 0.f;
	float DefaultSpinningFriction = 0.f;
	float DefaultRestitution = 1.f;

	// Collision policy data used in hot paths
	uint8  ObjectChannel = 0;    // 0..31 (ECollisionChannel as uint8)
	uint8  bQueryEnabled = 1;    // optional
	uint8  bPhysicsEnabled = 1;  // optional
	uint8  Pad = 0;

	uint32 BlockMask = 0;        // bits for channels this blocks
	uint32 OverlapMask = 0;      // bits for channels this overlaps (optional)c.)
};