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
	int32 BlockingShapeWorldArrayIndex = -1;
	
	UPROPERTY(BlueprintReadOnly)
	int32 OverlappingShapeWorldArrayIndex = -1;
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
	
};