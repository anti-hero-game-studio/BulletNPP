// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletTypes.generated.h"

USTRUCT()
struct FUnrealShape
{
	GENERATED_BODY()
	
	TWeakObjectPtr<UPrimitiveComponent> Shape = nullptr;
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
	
	int WorldArrayIndex = -1;
	
	void Add(UPrimitiveComponent* C)
	{
		Shapes.Add(FUnrealShape(C));
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