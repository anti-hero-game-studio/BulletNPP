// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CoreMinimal.h"
#include "PhysicsEngine/BodySetup.h"
#include "Core/Libraries/BulletLibrary.h"
#include "Core/Simulation/BulletMotionState.h"
#include "BulletMain.h"
#include "Components/ShapeComponent.h"
#include <functional>

#include "Core/BaseClasses/BulletCapsuleComponent.h"
#include "Core/CollisionFilters/BulletOverlappingPairCache.h"
#include "Core/CollisionFilters/ConvexResultCallback_IgnoreActors.h"
#include "Core/CollisionFilters/OverlapFilterCallback.h"
#include "Core/CollisionFilters/RaycastResultCallback_IgnoreActors.h"
#include "Core/ContactHandling/BulletContactGatherer.h"
#include "Core/DataTypes/BulletTypes.h"
#include "GameFramework/Actor.h"
#include "Subsystems/SubsystemCollection.h"
#include "Templates/Function.h"
#include "BulletPhysicsWorldSubsystem.generated.h"

USTRUCT()
struct FBulletRigidBodySnapshot
{
	GENERATED_BODY()

	// Stable key to find the body on restore
	UPROPERTY() uint64 BodyKey = 0;

	// Core kinematic state
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FQuat   Rotation = FQuat::Identity;
	UPROPERTY() FVector LinearVelocity = FVector::ZeroVector;
	UPROPERTY() FVector AngularVelocity = FVector::ZeroVector;

	// Bullet activation / sleep state (critical for determinism)
	UPROPERTY() int32 ActivationState = 0; // btCollisionObject::ActivationState enum values

	// Interpolation state used by Bullet (important if using CCD/interpolation)
	UPROPERTY() FVector InterpPosition = FVector::ZeroVector;
	UPROPERTY() FQuat   InterpRotation = FQuat::Identity;
	UPROPERTY() FVector InterpLinearVelocity = FVector::ZeroVector;
	UPROPERTY() FVector InterpAngularVelocity = FVector::ZeroVector;

	// Optional: useful for sanity checks / ensuring same object type
	UPROPERTY() int32 CollisionFlags = 0;
	UPROPERTY() int32 IslandTag = 0; // mostly debug; can help detect mismatches
};

USTRUCT()
struct FBulletFrameSnapshot
{
	GENERATED_BODY()

	UPROPERTY() int32 CommandFrame = INDEX_NONE;
	UPROPERTY() TArray<FBulletRigidBodySnapshot> Bodies; // all rollback-participating bodies for that frame
};


class FUnrealCollisionDispatcher;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhysicsStep, const float&, DeltaTime);
DECLARE_MULTICAST_DELEGATE(FOnModifyContacts);

/**
 * 
 */
UCLASS()
class BULLET_API UBulletPhysicsWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldEndPlay(UWorld& InWorld) override;
	
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet Physics|Objects")
	bool DebugEnabled=true;

	// TODO:@GreggoryAddison::CodeLinking | Replace this with the gravity you would set in the simulation comp
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet Physics|Objects")
	FVector Gravity=FVector(0, 0, -9.8);

	// Input the fixed frame rate to calculate physics
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet Physics|Objects")
	float PhysicsRefreshRate =60.0f;

	// This is independent of the frame rate in UE
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Bullet Physics|Objects")
	float PhysicsDeltaTime;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet Physics|Objects")
	int SubSteps=1;
	
	
#pragma region DELEGATES
	UPROPERTY(BlueprintAssignable, Category="Bullet Physics|Delegates")
	FOnPhysicsStep OnPrePhysicsStep;
	
	UPROPERTY(BlueprintAssignable, Category="Bullet Physics|Delegates")
	FOnPhysicsStep OnPostPhysicsStep;

#pragma endregion
	
public:
	/**
	 * Creates a bullet physics compatible rigid body shape. Actors tagged "dynamic" will automatically register themselves. Set "bSimulatePhysics" to true if you want the body to start in an active state.
	 * @param Target	The actor with primitive components that will be converted to rigid shapes. ACTOR SCALE MUST BE {1,1,1}
	 * @return	Returns the id to use in collision lookups
	 */
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Registration", DisplayName="Register Dynamic Rigid Body", meta=(AutoCreateRefTerm = "Options"))
	void RegisterBulletRigidBody(AActor* Target);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects", DisplayName="Set Physics State")
	void K2_SetPhysicsState(const UPrimitiveComponent* Target, const FTransform& Transforms, const FVector& Velocity, const FVector& AngularVelocity);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void GetPhysicsState(const UPrimitiveComponent* Target, FTransform& Transforms, FVector& Velocity, FVector& AngularVelocity, FVector& Force);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void GetMotionState(int Id, FTransform& Transforms, FVector& Velocity, FVector& AngularVelocity, FVector& Force);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void StepPhysics(float DeltaSeconds, int MaxSubSteps = 1, float FixedTimeStep = 0.016666667f);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void AddImpulse(AActor* Target, const FVector Impulse);

	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void AddForce(AActor* Target, const FVector Force);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void WakeBody(const UPrimitiveComponent* Target);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void SleepBody(const UPrimitiveComponent* Target);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void SetAngularVelocity(AActor* Target, const FVector AngularVelocity);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void ApplyVelocity(const UPrimitiveComponent* Target, const FVector LinearVelocity, const FVector AngularVelocity);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void ZeroActorVelocity(AActor* Target);
	
	UFUNCTION(BlueprintPure, Category = "Bullet Physics|Objects")
	float GetGravity(const UPrimitiveComponent* Target) const;
	

	

	
#pragma region SCENE QUERY
	
	UFUNCTION(BlueprintCallable, Category="Bullet Physics|Scene Queries", meta=( AutoCreateRefTerm = "ActorsToIgnore"))
	FHitResult LineTraceSingleByChannel(const FVector Start, const FVector End, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, int32& HitBodyId);
	
	UFUNCTION(BlueprintCallable, Category="Bullet Physics|Scene Queries", meta=( AutoCreateRefTerm = "ActorsToIgnore"))
	UPARAM(DisplayName=Hits) TArray<FHitResult> LineTraceMultiByChannel(const FVector Start, const FVector End, const TEnumAsByte<ECollisionChannel> Channel, 
		const TArray<AActor*>& ActorsToIgnore, TArray<int32>& HitBodyIds);
	
	UFUNCTION(BlueprintCallable, Category="Bullet Physics|Scene Queries", meta=( AutoCreateRefTerm = "ActorsToIgnore"))
	FHitResult SweepSphereSingleByChannel(const float Radius, const FVector Start, const FVector End, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, int32& HitBodyId);
	
	UFUNCTION(BlueprintCallable, Category="Bullet Physics|Scene Queries", meta=( AutoCreateRefTerm = "ActorsToIgnore"))
	UPARAM(DisplayName=Hits) TArray<FHitResult> SweepSphereMultiByChannel(const float Radius, const FVector Start, const FVector End, 
		const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, TArray<int32>& HitBodyIds);
	
	UFUNCTION(BlueprintCallable, Category="Bullet Physics|Scene Queries", meta=( AutoCreateRefTerm = "ActorsToIgnore"))
	FHitResult SweepCapsuleSingleByChannel(const float Radius, const float HalfHeight, const FVector Start, const FVector End, const FRotator Rotation, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, int32& HitBodyId);
	
	UFUNCTION(BlueprintCallable, Category="Bullet Physics|Scene Queries", meta=( AutoCreateRefTerm = "ActorsToIgnore"))
	UPARAM(DisplayName=Hits) TArray<FHitResult> SweepCapsuleMultiByChannel(const float Radius, const float HalfHeight, const FVector Start, const FVector End, 
		const FRotator Rotation, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, TArray<int32>& HitBodyIds);
	
	UFUNCTION(BlueprintCallable, Category="Bullet Physics|Scene Queries", meta=( AutoCreateRefTerm = "ActorsToIgnore"))
	FHitResult SweepBoxSingleByChannel(const FVector BoxExtents, const FVector Start, const FVector End, const FRotator Rotation, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, int32& HitBodyId);
	
	UFUNCTION(BlueprintCallable, Category="Bullet Physics|Scene Queries", meta=( AutoCreateRefTerm = "ActorsToIgnore"))
	UPARAM(DisplayName=Hits) TArray<FHitResult> SweepBoxMultiByChannel(const FVector BoxExtents, const FVector Start, const FVector End, 
		const FRotator Rotation, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, TArray<int32>& HitBodyIds);
	
	UFUNCTION(BlueprintCallable, Category="Bullet Physics|Scene Queries")
	FHitResult SweepGhostSphere(const float Radius, const FTransform Start, const FTransform End);
	
	int32 LineTraceSingle(const FVector& Start, const FVector& End, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, FHitResult& OutHit);
	TArray<int32> LineTraceMulti(const FVector& Start, const FVector& End, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, TArray<FHitResult>& OutHits);
	
	int32 SweepTraceSingle(const FCollisionShape& Shape, const FVector& Start, const FVector& End, const FQuat& Rotation, const TEnumAsByte<ECollisionChannel>& Channel, const TArray<AActor*>& ActorsToIgnore, FHitResult& OutHit);
	TArray<int32> SweepTraceMulti(const FCollisionShape& Shape, const FVector& Start, const FVector& End, const FQuat& Rotation, const TEnumAsByte<ECollisionChannel>& Channel, const TArray<AActor*>& ActorsToIgnore, TArray<FHitResult>& OutHits);
	
private:
	void ConstructHitResult(const btCollisionWorld::ClosestRayResultCallback& Result, FHitResult& OutHit) const;
	void ConstructHitResult(const btCollisionWorld::ClosestConvexResultCallback& Result, FHitResult& OutHit) const;
	
	void ConstructHitResult(const btAllNotMeRaycastResultCallback& Result, TArray<FHitResult>& OutHits) const;
	void ConstructHitResult(const btAllNotMeConvexResultCallback& Result, TArray<FHitResult>& OutHits) const;
	
	int32 SweepTraceInternal(const btTransform& From, const btTransform& To, const btCollisionShape* Collider, btClosestNotMeConvexResultCallback& Result, FHitResult& OutHit) const;
	TArray<int32> SweepTraceInternal(const btTransform& From, const btTransform& To, const btCollisionShape* Collider, btAllNotMeConvexResultCallback& Result, TArray<FHitResult>& OutHits) const;
	
	
#pragma endregion 
	
	
#pragma region DEBUGGER
	void StartDebugDrawer();
	void StopDebugDrawer();
	
	void ShowDebugShapes(const FCollisionShape& Shape, const FVector& Start, const FVector& End, const FQuat& Rotation) const;
#pragma endregion
	
	
	
private:
	
	FBulletOverlapFilterCallback OverlapFilterCallback;
	FBulletOverlappingPairCache* OverlappingPairCache;
	btCollisionConfiguration* BtCollisionConfig;
	FUnrealCollisionDispatcher* BtCollisionDispatcher;
	btBroadphaseInterface* BtBroadphase;
	btConstraintSolver* BtConstraintSolver;
	btDiscreteDynamicsWorld* BtWorld;
	BulletHelpers* BulletHelpers;
	btStaticPlaneShape* Plane;
	// Custom debug interface
	btIDebugDraw* BtDebugDraw;
	// Dynamic bodies
	// Static colliders
	btCollisionObject* ProceduralBody;
	// Re-usable collision shapes
	TArray<btBoxShape*> BtBoxCollisionShapes;
	TArray<btSphereShape*> BtSphereCollisionShapes;
	TArray<btCapsuleShape*> BtCapsuleCollisionShapes;
	btSequentialImpulseConstraintSolver* mt;
	
	// Structure to hold re-usable ConvexHull shapes based on origin BodySetup / subindex / scale
	struct ConvexHullShapeHolder
	{
		UBodySetup* BodySetup;
		int HullIndex;
		FVector Scale;
		btConvexHullShape* Shape;
	};
	
	TArray<ConvexHullShapeHolder> BtConvexHullCollisionShapes;
	// These shapes are for *potentially* compound rigid body shapes
	struct CachedDynamicShapeData
	{
		FName ClassName; // class name for cache
		btCollisionShape* Shape;
		bool bIsCompound; // if true, this is a compound shape and so must be deleted
		btScalar Mass;
		btVector3 Inertia; // because we like to precalc this
	};
	TArray<CachedDynamicShapeData> CachedDynamicShapes;

	TArray<btRigidBody*> BtRigidBodies;
	TArray<btGhostObject*> BtGhostBodies;
	TArray<btCollisionObject*> BtStaticBodies;
	TArray<btBvhTriangleMeshShape*> ComplexShapes;
	
		
#pragma region BULLET SHAPE CREATION
public:
	
	btDiscreteDynamicsWorld* GetBulletWorld() const {return BtWorld;};

	btCollisionShape* GetBoxCollisionShape(const FVector& Dimensions);

	btCollisionShape* GetSphereCollisionShape(float Radius);

	btCollisionShape* GetCapsuleCollisionShape(float Radius, float Height);
	
	btBvhTriangleMeshShape* GetComplexShape(const FTransform& CurrentTransform, UStaticMesh* Mesh);

	btCollisionShape* GetTriangleMeshShape(TArray<FVector> a, TArray<FVector> b, TArray<FVector> c, TArray<FVector> d);

	btCollisionShape* GetConvexHullCollisionShape(UBodySetup* BodySetup, int ConvexIndex, const FVector& Scale);

	btRigidBody* AddRigidBodyCollider(AActor* Actor, const FTransform& FinalTransform, btCollisionShape* CollisionShape, const FBulletPhysicsBodySettings& Options);

	btRigidBody* AddRigidBodyCollider(USkeletalMeshComponent* Skel, const FTransform& localTransform, btCollisionShape* CollisionShape, const FBulletPhysicsBodySettings& Options);
	
	btCollisionObject* AddStaticCollider(btCollisionShape* Shape, const FTransform& Transform, const FBulletPhysicsBodySettings& Options);
	
	btGhostObject* AddGhostCollider(btCollisionShape* Shape, const FTransform& Transform, const FBulletPhysicsBodySettings& Options);
	
	btCollisionObject* GetStaticObject(int ID) const;
	
	
private:
	typedef const std::function<void(btCollisionShape* /*SingleShape*/, const FTransform& /*RelativeXform*/, const FBulletPhysicsBodySettings& /*ShapeOptions*/)>& PhysicsGeometryCallback;

	void ExtractPhysicsGeometry(const AActor* Actor, PhysicsGeometryCallback CB, FUnrealShapeDescriptor& ShapeDescriptor);
	
	void ExtractComplexPhysicsGeometry(const FTransform& XformSoFar, UStaticMeshComponent* Mesh, PhysicsGeometryCallback Callback, FUnrealShapeDescriptor& ShapeDescriptor);

	void ExtractPhysicsGeometry(UStaticMeshComponent* SMC, const FTransform& InvActorXform, PhysicsGeometryCallback CB, FUnrealShapeDescriptor& ShapeDescriptor);

	void ExtractPhysicsGeometry(UShapeComponent* Sc, const FTransform& InvActorXform, PhysicsGeometryCallback CB, FUnrealShapeDescriptor& ShapeDescriptor);

	void ExtractPhysicsGeometry(UPrimitiveComponent* PrimitiveComponent, const FTransform& XformSoFar, UBodySetup* BodySetup, PhysicsGeometryCallback CB, FUnrealShapeDescriptor& ShapeDescriptor);

	const UBulletPhysicsWorldSubsystem::CachedDynamicShapeData& GetCachedDynamicShapeData(AActor* Actor, float Mass);
	
	void CleanUpBulletWorld();
	
	TArray<btBoxShape*> BoxCollisionShapes;
	TArray<btSphereShape*> SphereCollisionShapes;
	TArray<btCapsuleShape*> CapsuleCollisionShapes;
	TArray<btConvexHullShape*> ConvexHullCollisionShapes;
	
#pragma endregion
	
#pragma region SNAPSHOT HISTORY
public:
	// Main API you asked for:
	// Call after physics step for CommandFrame.
	void SaveState(const int32 CommandFrame);

	// Call before client rollback replay; restores bodies to snapshot at CommandFrame.
	bool RestoreState(const int32 CommandFrame);
	
	// Optional: you can call this if your world changes drastically (level load, etc.)
	void ResetStateHistory();
	
private:
	// Circular buffer keyed by CommandFrame.
	// Index = CommandFrame % StateHistorySizeFrames.
	TArray<FBulletFrameSnapshot> FrameHistory;

	// Internal helpers
	void EnsureHistoryAllocated();
	void CaptureBodySnapshot(btRigidBody& Body, FBulletRigidBodySnapshot& Out) const;
	bool ApplyBodySnapshot(btRigidBody& Body, const FBulletRigidBodySnapshot& Snap) const;

	bool ShouldSnapshotBody(const btRigidBody& Body) const;
	
	// ... inside UBulletPhysicsWorldSubsystem
	
	

	static FORCEINLINE uint64 BulletGetBodyKey64(const btCollisionObject* Obj)
	{
		check(Obj);
		return Obj->getWorldArrayIndex();
	}
	
#pragma endregion
	

#pragma region DATA CACHE
	
protected:
	// Holds an array of collision object id's for a specific actor.
	TMap<TWeakObjectPtr<AActor>, FUnrealShapeDescriptor> GlobalShapeDescriptorDataCache; 
	
	FUnrealShapeDescriptor GetShapeDescriptorData(const AActor* Actor) const;
	
	
public:
	
	
#pragma endregion
	
#pragma region HELPERS
	
	FVector GetVelocity(const int32& ID) const;
	int32 GetActorRootShapeId(const AActor* Actor) const;
	int32 FindShapeId(const UPrimitiveComponent* Target) const;
	bool IsBodyValid(const UPrimitiveComponent* Target) const;
	bool HasGhostBodyBeenCreated(const UPrimitiveComponent* Target) const;
	bool HasRigidBodyBeenCreated(const UPrimitiveComponent* Target) const;
	bool IsCollisionBodyActive(const UPrimitiveComponent* Target) const;
	bool IsGhostBodyActive(const UPrimitiveComponent* Target) const;
	void SetRigidBodyActiveState(const UPrimitiveComponent* Target, bool Active) const;
	void SetPhysicsState(int ID, const FTransform& Transforms, const FVector& Velocity, const FVector& AngularVelocity) const;
	void UpdateAABB() const;
	
	void TestCollisions(const FTransform& T, const int32& ID, TArray<FBulletHitEvent>& OutHits) const;
	void TestCollisions(const FTransform& T, const UPrimitiveComponent* Target, TArray<FBulletHitEvent>& OutHits) const;
	
	FHitResult SweepGhostObject(const float Radius, const FTransform& Start, const FTransform& End);
	
	const FCollisionResponseContainer& GetCollisionResponseContainer(const UPrimitiveComponent* Target) const;
	btCollisionObject* GetCollisionBody(const int32& Id) const;
	btCollisionObject* GetCollisionBody(const UPrimitiveComponent* Target) const;
	btCollisionObject* GetCollisionBody(const FHitResult& Hit) const;
	btRigidBody* GetRigidBody(const int32& Id) const;
	btRigidBody* GetRigidBody(const UPrimitiveComponent* Target) const;
	btRigidBody* GetRigidBody(const FHitResult& Hit) const;
	static FHitResult ConstructHitResult(const FBulletHitEvent& E);
	const TArray<FBulletHitEvent>& GetAllHitEvents() const;
	FBulletUserData* GetUserData(const UPrimitiveComponent* Target) const;
	
	
	
	FOnModifyContacts OnModifyContacts;
	
	
	
	

private:
	
	TArray<TUniquePtr<FBulletUserData>> UserDataStore;
	
	FBulletUserData* AllocUserData()
	{
		TUniquePtr<FBulletUserData> Ptr = MakeUnique<FBulletUserData>();
		FBulletUserData* Raw = Ptr.Get();
		UserDataStore.Add(MoveTemp(Ptr));
		return Raw;
	}
	
	FCollisionResponseContainer DefaultCollisionResponseContainer;
	
	FBulletContactGatherer Gatherer;
	static void BroadcastSymmetricHits(const FBulletHitEvent& Base);
	static void BroadcastComponentHit(const FBulletHitEvent& E);


	btPairCachingGhostObject* m_ghostObject;
#pragma endregion 
};
