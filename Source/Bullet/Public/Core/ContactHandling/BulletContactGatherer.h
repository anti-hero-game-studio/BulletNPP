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
			SumNormal += BulletHelpers::ToUnrealDirection(Pt.m_normalWorldOnB).GetSafeNormal();

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

		// Aggregate one event per pair this step (stable centroid signature)
		TMap<FBulletPairKey, FBulletHitEvent> BestByPair;
		TMap<FBulletPairKey, FContactSignature> CurrSigByPair;

		const int32 NumManifolds = Dispatcher->getNumManifolds();
		for (int32 i = 0; i < NumManifolds; ++i)
		{
			btPersistentManifold* M = Dispatcher->getManifoldByIndexInternal(i);
			if (!M) continue;

			const btCollisionObject* Obj0 = static_cast<const btCollisionObject*>(M->getBody0());
			const btCollisionObject* Obj1 = static_cast<const btCollisionObject*>(M->getBody1());
			if (!Obj0 || !Obj1) continue;
			
			const FBulletUserData* UD0 = static_cast<const FBulletUserData*>(Obj0->getUserPointer()); 
			const FBulletUserData* UD1 = static_cast<const FBulletUserData*>(Obj1->getUserPointer()); 

			if (!UD0 || !UD1) continue;
			
			UPrimitiveComponent* C0 = Cast<UPrimitiveComponent>(UD0->Component);
			UPrimitiveComponent* C1 = Cast<UPrimitiveComponent>(UD1->Component);
			if (!C0 || !C1) continue;

			// Pair key
			const FBulletPairKey Key = FBulletPairKey::Make(Obj0, Obj1);

			// Build a stable resting-contact signature for this manifold
			FContactSignature Sig;
			FVector ImpactPoint, ImpactNormal;
			float PenDepthCm = 0.f;
			float MaxImpulse = 0.f;

			if (!BuildCentroidSignature(M, Sig, ImpactPoint, ImpactNormal, PenDepthCm, MaxImpulse))
			{
				continue;
			}

			// If you still want to gate reporting by impulse (optional):
			// For resting spam suppression, it's better NOT to gate by impulse (impulse fluctuates),
			// but we keep your knobs: only apply impulse gate when bReportAllContacts is false.
			if (!bReportAllContacts && MaxImpulse < MinImpulseToReport)
			{
				// Still allow reporting if penetration is meaningfully non-zero (common for resting).
				// If you want strictly impulse-based, delete this block and just continue.
				// continue;
			}

			// Keep the first manifold signature per pair (normally only one exists).
			// If duplicates exist, you can choose the deeper one:
			FContactSignature* ExistingSig = CurrSigByPair.Find(Key);
			if (!ExistingSig)
			{
				CurrSigByPair.Add(Key, Sig);

				FBulletHitEvent& E = BestByPair.FindOrAdd(Key);
				E.SelfComp = C0;
				E.OtherComp = C1;
				E.ImpactPoint = ImpactPoint;
				E.ImpactNormal = ImpactNormal;
				E.PenetrationDepth = PenDepthCm;
				E.AppliedImpulse = MaxImpulse;
				E.ImpulseDir = ImpactNormal;
			}
			else
			{
				// If you encounter multiple manifolds per pair, prefer the one with deeper penetration.
				// (We only stored PenDepth in the event; recompute comparison cheaply)
				FBulletHitEvent& ExistingEvent = BestByPair.FindOrAdd(Key);
				if (PenDepthCm > ExistingEvent.PenetrationDepth)
				{
					*ExistingSig = Sig;
					ExistingEvent.SelfComp = C0;
					ExistingEvent.OtherComp = C1;
					ExistingEvent.ImpactPoint = ImpactPoint;
					ExistingEvent.ImpactNormal = ImpactNormal;
					ExistingEvent.PenetrationDepth = PenDepthCm;
					ExistingEvent.AppliedImpulse = MaxImpulse;
					ExistingEvent.ImpulseDir = ImpactNormal;
				}
			}
		}

		// Diff against last frame: only emit hits when the centroid/normal meaningfully changed
		for (auto& It : BestByPair)
		{
			const FBulletPairKey Key = It.Key;
			FBulletHitEvent& Event = It.Value;

			const FContactSignature* CurrSig = CurrSigByPair.Find(Key);
			if (!CurrSig)
				continue;

			FContactSignature& PrevSig = LastSigByPair.FindOrAdd(Key);

			const bool bChanged = MeaningfulChange(
				PrevSig,
				*CurrSig,
				ContactPointEpsilonCm,
				ContactNormalDotEpsilon,
				QuantizeGridCm);

			if (bChanged || bForceUpdate)
			{
				OutEvents.Add(Event);
				PrevSig = *CurrSig; // update cache
			}
		}

		// Optional: clear cache for pairs that no longer exist so a future contact re-triggers.
		// If you want "contact ended" behavior, track this separately.
		for (auto It = LastSigByPair.CreateIterator(); It; ++It)
		{
			if (!CurrSigByPair.Contains(It.Key()))
			{
				It.RemoveCurrent();
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
