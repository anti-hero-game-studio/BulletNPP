// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BulletMain.h"
#include "Core/DataTypes/BulletTypes.h"


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
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToUnrealPosition);
		return FVector(V.x(), V.y(), V.z()) * BULLET_TO_WORLD_SCALE + WorldOrigin;
	}
	static btVector3 ToBulletPosition(const FVector& V, const FVector& WorldOrigin)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToBulletPosition);
		return btVector3(V.X - WorldOrigin.X, V.Y - WorldOrigin.Y, V.Z - WorldOrigin.Z) * WORLD_TO_BULLET_SCALE;
	}
	static btVector3 ToBulletPosition(const FVector3f& V, const FVector& WorldOrigin)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToBulletPosition);
		return btVector3(V.X - WorldOrigin.X, V.Y - WorldOrigin.Y, V.Z - WorldOrigin.Z) * WORLD_TO_BULLET_SCALE;
	}
	static FVector ToUnrealDirection(const btVector3& V, bool AdjustScale = true)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToUnrealDirection);
		if (AdjustScale)
			return FVector(V.x(), V.y(), V.z()) * BULLET_TO_WORLD_SCALE;
		else
			return FVector(V.x(), V.y(), V.z());
	}
	static btVector3 ToBulletDirection(const FVector& V, bool AdjustScale = true)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToUnrealDirection);
		if (AdjustScale)
			return btVector3(V.X, V.Y, V.Z) * WORLD_TO_BULLET_SCALE;
		else
			return btVector3(V.X, V.Y, V.Z);
	}
	static btVector3 ToBulletDirection(const FVector3f& V, bool AdjustScale = true)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToBulletDirection);
		if (AdjustScale)
			return btVector3(V.X, V.Y, V.Z) * WORLD_TO_BULLET_SCALE;
		else
			return btVector3(V.X, V.Y, V.Z);
	}
	
	static FQuat ToUnrealQuat(const btQuaternion& Q)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToUnrealQuat);
		return FQuat(Q.x(), Q.y(), Q.z(), Q.w());
	}
	
	static btQuaternion ToBulletQuat(const FQuat& Q)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToBulletQuat);
		return btQuaternion(Q.X, Q.Y, Q.Z, Q.W);
	}
	static btQuaternion ToBulletQuat(const FRotator& r)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToBulletQuat);
		return ToBulletQuat(r.Quaternion());
	}
	static FColor ToUnrealColor(const btVector3& C)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToUnrealColor);
		return FLinearColor(C.x(), C.y(), C.z()).ToFColor(true);
	}

	static FTransform ToUnrealTransform(const btTransform& T, const FVector& WorldOrigin)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToUnrealTransform);
		const FQuat Rot = ToUnrealQuat(T.getRotation());
		const FVector Pos = ToUnrealPosition(T.getOrigin(), WorldOrigin);
		return FTransform(Rot, Pos);
	}
	static btTransform ToBulletTransform(const FTransform& T, const FVector& WorldOrigin)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::ToBulletTransform);
		return btTransform(
				ToBulletQuat(T.GetRotation()),
				ToBulletPosition(T.GetLocation(), WorldOrigin));
	}
	
	
	static bool IsAnyCollisionAllowed(const btCollisionObject* A, const btCollisionObject* B)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::IsAnyCollisionAllowed);
		if (!A || !B) return true;
		return IsBlockingCollisionAllowed(A, B) || IsOverlappingCollisionAllowed(A, B);
	}
	
	static bool IsAnyCollisionAllowed(const TEnumAsByte<ECollisionChannel>& Channel, const btCollisionObject* B)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::IsAnyCollisionAllowed);
		if (!B) return true;
		
		// Your policy (example)
		return IsBlockingCollisionAllowed(Channel, B) || IsOverlappingCollisionAllowed(Channel, B);
	}
	
	static bool IsBlockingCollisionAllowed(const TEnumAsByte<ECollisionChannel>& Channel, const btCollisionObject* B)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::IsBlockingCollisionAllowed);
		if (!B) return true;
		
		const FBulletUserData* UB = GetUserData(B);

		// ObjectChannel must be 0..31
		const uint32 ChanA = static_cast<uint32>(Channel) & 31u;
		const uint32 ChanB = static_cast<uint32>(UB->ObjectChannel) & 31u;

		const uint32 BitA = 1u << ChanA;
		const uint32 BitB = 1u << ChanB;

		// UE "blocking" convention for two-way interaction:
		// A blocks B's channel AND B blocks A's channel.
		const bool bBBlocksA = (UB->BlockMask & BitA) != 0;

		return bBBlocksA;
	}
	
	static bool IsBlockingCollisionAllowed(const btCollisionObject* A, const btCollisionObject* B)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::IsBlockingCollisionAllowed);
		if (!A || !B) return false;

		const FBulletUserData* UA = GetUserData(A);
		const FBulletUserData* UB = GetUserData(B);

		// If you are mid-transition and some objects still store USceneComponent*,
		// choose a policy. Safest for gameplay is usually "allow" (or fall back to Super).
		if (!UA || !UB)
		{
			return true; // or false, or "return Super" at the callsite
		}

		// Optional: respect query/physics enabled flags.
		// If this function is used for sweeps/queries, gate by query.
		if (!UA->bQueryEnabled || !UB->bQueryEnabled)
		{
			return false;
		}

		// ObjectChannel must be 0..31
		const uint32 ChanA = static_cast<uint32>(UA->ObjectChannel) & 31u;
		const uint32 ChanB = static_cast<uint32>(UB->ObjectChannel) & 31u;

		const uint32 BitA = 1u << ChanA;
		const uint32 BitB = 1u << ChanB;

		// UE "blocking" convention for two-way interaction:
		// A blocks B's channel AND B blocks A's channel.
		const bool bABlocksB = (UA->BlockMask & BitB) != 0;
		const bool bBBlocksA = (UB->BlockMask & BitA) != 0;

		return bABlocksB && bBBlocksA;
	}
	
	static bool IsOverlappingCollisionAllowed(const btCollisionObject* A, const btCollisionObject* B)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::IsOverlappingCollisionAllowed);
		if (!A || !B) return false;

		const FBulletUserData* UA = GetUserData(A);
		const FBulletUserData* UB = GetUserData(B);

		// If you are mid-transition and some objects still store USceneComponent*,
		// choose a policy. Safest for gameplay is usually "allow" (or fall back to Super).
		if (!UA || !UB)
		{
			return true; // or false, or "return Super" at the callsite
		}

		// Optional: respect query/physics enabled flags.
		// If this function is used for sweeps/queries, gate by query.
		if (!UA->bQueryEnabled || !UB->bQueryEnabled)
		{
			return false;
		}

		// ObjectChannel must be 0..31
		const uint32 ChanA = static_cast<uint32>(UA->ObjectChannel) & 31u;
		const uint32 ChanB = static_cast<uint32>(UB->ObjectChannel) & 31u;

		const uint32 BitA = 1u << ChanA;
		const uint32 BitB = 1u << ChanB;

		// UE "overlapping" convention for two-way interaction:
		// A blocks B's channel AND B blocks A's channel.
		const bool bABlocksB = (UA->OverlapMask & BitB) != 0;
		const bool bBBlocksA = (UB->OverlapMask & BitA) != 0;

		return bABlocksB || bBBlocksA;
	}
	
	static bool IsOverlappingCollisionAllowed(const TEnumAsByte<ECollisionChannel>& Channel, const btCollisionObject* B)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::IsOverlappingCollisionAllowed);
		if (!B) return true;
		
		const FBulletUserData* UB = GetUserData(B);

		// ObjectChannel must be 0..31
		const uint32 ChanA = static_cast<uint32>(Channel) & 31u;
		const uint32 ChanB = static_cast<uint32>(UB->ObjectChannel) & 31u;

		const uint32 BitA = 1u << ChanA;
		const uint32 BitB = 1u << ChanB;

		// A blocks B's channel AND B blocks A's channel.
		const bool bBBlocksA = (UB->OverlapMask & BitA) != 0;

		return bBBlocksA;
	}
	
	static void BuildResponseMasks(
	const FCollisionResponseContainer& Responses,
	uint32& OutBlockMask,
	uint32& OutOverlapMask,
	uint32& OutIgnoreMask)
		{
		OutBlockMask   = 0;
		OutOverlapMask = 0;
		OutIgnoreMask  = 0;

		// UE supports up to 32 channels in ECollisionChannel (0..31)
		for (int32 i = 0; i < 32; ++i)
		{
			const ECollisionChannel Channel = static_cast<ECollisionChannel>(i);

			// Optional: restrict to object channels only
			// if (!IsObjectChannel(Channel)) { continue; }

			const ECollisionResponse R = Responses.GetResponse(Channel);

			const uint32 Bit = (1u << i);
			switch (R)
			{
			case ECR_Block:   OutBlockMask   |= Bit; break;
			case ECR_Overlap: OutOverlapMask |= Bit; break;
			default:          OutIgnoreMask  |= Bit; break; // ECR_Ignore
			}
		}
	}
	
	// Helper: safely get FBulletUserData from a Bullet object
	static FORCEINLINE const FBulletUserData* GetUserData(const btCollisionObject* Obj)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BulletHelpers::GetUserData);
		if (!Obj) return nullptr;
		void* P = Obj->getUserPointer();
		if (!P) return nullptr;

		const FBulletUserData* UD = static_cast<const FBulletUserData*>(P);
		return (UD->Magic == FBulletUserData::MagicValue) ? UD : nullptr;
	}
};
