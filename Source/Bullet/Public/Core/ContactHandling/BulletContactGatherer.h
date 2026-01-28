// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "BulletCollision/BroadphaseCollision/btDispatcher.h"
#include "BulletDynamics/Dynamics/btDynamicsWorld.h"
#include "Core/Libraries/BulletLibrary.h"


struct FBulletHitEvent
{
	TWeakObjectPtr<UPrimitiveComponent> SelfComp;
	TWeakObjectPtr<UPrimitiveComponent> OtherComp;

	// Payload needed for FHitResult
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::UpVector;
	float PenetrationDepth = 0.f;

	// Optional but useful
	float AppliedImpulse = 0.f;
	FVector ImpulseDir = FVector::ZeroVector;
};

struct FBulletPairKey
{
	const btCollisionObject* A = nullptr;
	const btCollisionObject* B = nullptr;

	static FBulletPairKey Make(const btCollisionObject* In0, const btCollisionObject* In1)
	{
		FBulletPairKey K;
		if (In0 < In1) { K.A = In0; K.B = In1; }
		else           { K.A = In1; K.B = In0; }
		return K;
	}

	friend uint32 GetTypeHash(const FBulletPairKey& K)
	{
		return HashCombine(::GetTypeHash((UPTRINT)K.A), ::GetTypeHash((UPTRINT)K.B));
	}

	bool operator==(const FBulletPairKey& Other) const
	{
		return A == Other.A && B == Other.B;
	}
};

/**
 * 
 */
struct FBulletContactGatherer
{
	// --- Existing knobs (kept) ---
	float MinImpulseToReport = 0.5f;
	bool  bReportAllContacts = false;

	// --- New: resting-contact spam suppression knobs ---
	// Position change threshold in Unreal units (cm). Start at 2 cm for resting contacts.
	float ContactPointEpsilonCm = 2.0f;

	// Normal change threshold as dot product. 0.995 ~= ~5.7 degrees.
	float ContactNormalDotEpsilon = 0.995f;

	// Quantize the centroid to reduce solver jitter (cm). 0 disables quantization.
	float QuantizeGridCm = 1.0f;

	// Optional: require a minimum penetration to consider the contact "real" (cm). 0 disables.
	float MinPenetrationCm = 0.0f;

	// Output for this step (physics thread)
	TArray<FBulletHitEvent> OutEvents;
	
	// Output for this step (physics thread)
	TArray<FBulletHitEvent> CachedEvents;
	
	
	
private:
	struct FContactSignature
	{
		bool   bHadContact = false;
		FVector Point = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
	};

	// Cache of last signature per Bullet pair
	TMap<FBulletPairKey, FContactSignature> LastSigByPair;

	static FVector Quantize(const FVector& V, float GridCm)
	{
		if (GridCm <= 0.f)
			return V;

		return FVector(
			FMath::GridSnap(V.X, GridCm),
			FMath::GridSnap(V.Y, GridCm),
			FMath::GridSnap(V.Z, GridCm));
	}

	static bool MeaningfulChange(
		const FContactSignature& Prev,
		const FContactSignature& Curr,
		float PosEpsCm,
		float NormalDotEps,
		float GridCm)
	{
		
		TRACE_CPUPROFILER_EVENT_SCOPE(FBulletContactGatherer::MeaningfulChange);
		if (Prev.bHadContact != Curr.bHadContact)
			return true;

		if (!Curr.bHadContact)
			return false;

		const FVector P0 = Quantize(Prev.Point, GridCm);
		const FVector P1 = Quantize(Curr.Point, GridCm);

		if (FVector::DistSquared(P0, P1) > FMath::Square(PosEpsCm))
			return true;

		const float Dot = FVector::DotProduct(Prev.Normal, Curr.Normal);
		if (Dot < NormalDotEps)
			return true;

		return false;
	}

	// Build a stable signature for resting contact: centroid of all penetrating points in this manifold.
	// Returns false if no valid penetrating points.
	bool BuildCentroidSignature(
		btPersistentManifold* M,
		FContactSignature& OutSig,
		FVector& OutImpactPoint,
		FVector& OutImpactNormal,
		float& OutPenDepthCm,
		float& OutMaxImpulse) const
	{
		
		TRACE_CPUPROFILER_EVENT_SCOPE(FBulletContactGatherer::BuildCentroidSignature);
		const int32 NumContacts = M->getNumContacts();
		if (NumContacts <= 0)
			return false;

		FVector SumPoint = FVector::ZeroVector;
		FVector SumNormal = FVector::ZeroVector;
		int32 Count = 0;

		float MaxPenDepthRaw = 0.f; // raw bullet units
		float MaxImpulse = 0.f;

		for (int32 p = 0; p < NumContacts; ++p)
		{
			const btManifoldPoint& Pt = M->getContactPoint(p);

			const float Dist = Pt.getDistance(); // < 0 penetrating
			/*if (Dist >= 0.f)
				continue;*/

			const float Impulse = Pt.getAppliedImpulse();
			MaxImpulse = FMath::Max(MaxImpulse, Impulse);
			MaxPenDepthRaw = FMath::Max(MaxPenDepthRaw, -Dist);

			// Midpoint between contact positions on A and B gives a stable "patch" representative point.
			const FVector UEPointA = BulletHelpers::ToUnrealPosition(Pt.m_positionWorldOnA, FVector::Zero());
			const FVector UEPointB = BulletHelpers::ToUnrealPosition(Pt.m_positionWorldOnB, FVector::Zero());
			const FVector Mid = (UEPointA + UEPointB) * 0.5f;

			SumPoint += Mid;

			// Bullet: m_normalWorldOnB points from B toward A (into body0 for body0/body1 ordering).
			SumNormal += BulletHelpers::ToUnrealVector3(Pt.m_normalWorldOnB).GetSafeNormal();

			++Count;
		}

		if (Count == 0)
			return false;
			
		const FVector Centroid = SumPoint / float(Count);
		const FVector AvgNormal = SumNormal.GetSafeNormal();

		const float PenDepthCm = BulletHelpers::ToUnrealFloat(MaxPenDepthRaw);

		// Optional penetration gate (helps ignore tiny numerical "contacts")
		if (MinPenetrationCm > 0.f && PenDepthCm < MinPenetrationCm)
			return false;

		OutSig.bHadContact = true;
		OutSig.Point = Centroid;
		OutSig.Normal = AvgNormal;

		OutImpactPoint = Centroid;
		OutImpactNormal = AvgNormal;
		OutPenDepthCm = PenDepthCm;
		OutMaxImpulse = MaxImpulse;
		return true;
		
		
	}

public:
	void Gather(btDynamicsWorld* World)
	{
		constexpr bool bForceUpdate = true;
		TRACE_CPUPROFILER_EVENT_SCOPE(FBulletContactGatherer::Gather);
		OutEvents.Reset();
		if (!World) return;

		btDispatcher* Dispatcher = World->getDispatcher();
		if (!Dispatcher) return;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FBulletContactGatherer::FilterManifolds);
			const int32 NumManifolds = Dispatcher->getNumManifolds();
			for (int32 i = 0; i < NumManifolds; ++i)
			{
				btPersistentManifold* M = Dispatcher->getManifoldByIndexInternal(i);
				if (!M) continue;
				
				if (M->getNumContacts() == 0) continue;

				for (int j = 0; j < M->getNumContacts(); ++j)
				{
					btManifoldPoint& Pt = M->getContactPoint(j);

					if (Pt.getDistance() <= 0.f)
					{
						const btCollisionObject* Obj0 = static_cast<const btCollisionObject*>(M->getBody0());
						const btCollisionObject* Obj1 = static_cast<const btCollisionObject*>(M->getBody1());
						
						if (!Obj0 || !Obj1) continue;
						
						const FBulletUserData* UD0 = static_cast<const FBulletUserData*>(Obj0->getUserPointer()); 
						const FBulletUserData* UD1 = static_cast<const FBulletUserData*>(Obj1->getUserPointer()); 

						if (!UD0 || !UD1) continue;
			
						UPrimitiveComponent* C0 = Cast<UPrimitiveComponent>(UD0->Component);
						UPrimitiveComponent* C1 = Cast<UPrimitiveComponent>(UD1->Component);
						
						if (!C0 || !C1) continue;
						
						FBulletHitEvent Event;
						Event.SelfComp = C0;
						Event.OtherComp = C1;
						Event.ImpactPoint = BulletHelpers::ToUnrealPosition(Pt.getPositionWorldOnA(), FVector::ZeroVector);
						Event.ImpactNormal = BulletHelpers::ToUnrealNormal(Pt.m_normalWorldOnB);
						Event.PenetrationDepth = BulletHelpers::ToUnrealFloat(Pt.getDistance());
						Event.AppliedImpulse = BulletHelpers::ToUnrealFloat(Pt.getAppliedImpulse());
						Event.ImpulseDir = BulletHelpers::ToUnrealNormal(Pt.m_normalWorldOnB);
						
						OutEvents.Add(Event);
					}
				}
			}
		}
	}
	
	void CacheHitsThisFrame()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBulletContactGatherer::CacheHitsThisFrame);
		CachedEvents = OutEvents;
		OutEvents.Reset();
	}
};
