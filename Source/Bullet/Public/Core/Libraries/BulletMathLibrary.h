// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BulletMain.h"


// Bullet scale is 1=1m, UE is 1=1cm
// So x100
#define BULLET_TO_WORLD_SCALE 100.f
#define WORLD_TO_BULLET_SCALE (1.f/BULLET_TO_WORLD_SCALE)

class BulletHelpers
{

public:
	static float ToUnrealFloat(btScalar Sz)
	{
		return Sz * BULLET_TO_WORLD_SCALE;
	}
	static btScalar ToBulletFloat(float Sz)
	{
		return Sz * WORLD_TO_BULLET_SCALE;
	}
	static btVector3 ToBulletVector3(FVector Sv)
	{
		// For clarity; this is for box sizes so no offset
		return ToBulletDirection(Sv);
	}
	static btVector3 ToBulletVector3(FVector3f Sv)
	{
		// For clarity; this is for box sizes so no offset
		return ToBulletDirection(Sv);
	}
	
	static FVector ToUnrealPosition(const btVector3& V, const FVector& WorldOrigin)
	{
		return FVector(V.x(), V.y(), V.z()) * BULLET_TO_WORLD_SCALE + WorldOrigin;
	}
	static btVector3 ToBulletPosition(const FVector& V, const FVector& WorldOrigin)
	{
		return btVector3(V.X - WorldOrigin.X, V.Y - WorldOrigin.Y, V.Z - WorldOrigin.Z) * WORLD_TO_BULLET_SCALE;
	}
	static btVector3 ToBulletPosition(const FVector3f& V, const FVector& WorldOrigin)
	{
		return btVector3(V.X - WorldOrigin.X, V.Y - WorldOrigin.Y, V.Z - WorldOrigin.Z) * WORLD_TO_BULLET_SCALE;
	}
	static FVector ToUnrealDirection(const btVector3& V, bool AdjustScale = true)
	{
		if (AdjustScale)
			return FVector(V.x(), V.y(), V.z()) * BULLET_TO_WORLD_SCALE;
		else
			return FVector(V.x(), V.y(), V.z());
	}
	static btVector3 ToBulletDirection(const FVector& V, bool AdjustScale = true)
	{
		if (AdjustScale)
			return btVector3(V.X, V.Y, V.Z) * WORLD_TO_BULLET_SCALE;
		else
			return btVector3(V.X, V.Y, V.Z);
	}
	static btVector3 ToBulletDirection(const FVector3f& V, bool AdjustScale = true)
	{
		if (AdjustScale)
			return btVector3(V.X, V.Y, V.Z) * WORLD_TO_BULLET_SCALE;
		else
			return btVector3(V.X, V.Y, V.Z);
	}
	
	static FQuat ToUnrealQuat(const btQuaternion& Q)
	{
		return FQuat(Q.x(), Q.y(), Q.z(), Q.w());
	}
	static btQuaternion ToBulletQuat(const FQuat& Q)
	{
		return btQuaternion(Q.X, Q.Y, Q.Z, Q.W);
	}
	static btQuaternion ToBulletQuat(const FRotator& r)
	{
		return ToBulletQuat(r.Quaternion());
	}
	static FColor ToUnrealColor(const btVector3& C)
	{
		return FLinearColor(C.x(), C.y(), C.z()).ToFColor(true);
	}

	static FTransform ToUnrealTransform(const btTransform& T, const FVector& WorldOrigin)
	{
		const FQuat Rot = ToUnrealQuat(T.getRotation());
		const FVector Pos = ToUnrealPosition(T.getOrigin(), WorldOrigin);
		return FTransform(Rot, Pos);
	}
	static btTransform ToBulletTransform(const FTransform& T, const FVector& WorldOrigin)
	{
		return btTransform(
				ToBulletQuat(T.GetRotation()),
				ToBulletPosition(T.GetLocation(), WorldOrigin));
	}
};
