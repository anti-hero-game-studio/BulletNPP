// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CoreMinimal.h"
#include "PhysicsEngine/BodySetup.h"
#include "Core/Libraries/BulletMathLibrary.h"
#include "Core/Simulation/BulletMotionState.h"
#include "BulletMain.h"
#include "Components/ShapeComponent.h"
#include <functional>
#include "GameFramework/Actor.h"
#include "Subsystems/SubsystemCollection.h"
#include "Templates/Function.h"
#include "BulletPhysicsWorldSubsystem.generated.h"


USTRUCT()
struct FCollisionObjectArray
{
	GENERATED_BODY()
	
	FCollisionObjectArray()
	{
		
	}
	
	FCollisionObjectArray(const int32& InObjectId)
	{
		ObjectIds.AddUnique(InObjectId);
	}
	
	UPROPERTY()
	TArray<int32> ObjectIds;
};

/**
 * 
 */
UCLASS()
class BULLETNPP_API UBulletPhysicsWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
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
	
public:
	/**
	 * Creates a bullet physics compatible rigid body shape
	 * @param Target	The actor with primitive components that will be converted to rigid shapes. ACTOR SCALE MUST BE {1,1,1}
	 * @param Friction	Manually override the surface friction of the collision shape
	 * @param Restitution	Manually override the bounciness of the collision shape
	 * @param Mass	Manually override the weight (in kg) of the collision shape
	 * @param bUsePhysicsMaterial	If true the friction and restitution params will be ignored and instead pulled from the physics material
	 * @param Id	Returns the id to use in collision lookups
	 */
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Registration", DisplayName="Register Dynamic Rigid Body")
	void RegisterDynamicRigidBody(AActor* Target, float Friction, float Restitution, float Mass, bool bUsePhysicsMaterial, UPARAM(DisplayName="RigidBodyId") int32&Id );
	
	/**
	 * Creates a bullet physics compatible rigid body shape
	 * @param Target	The actor with primitive components that will be converted to rigid shapes. ACTOR SCALE MUST BE {1,1,1}
	 * @param Friction	Manually override the surface friction of the collision shape
	 * @param Restitution	Manually override the bounciness of the collision shape
	 * @param Mass	Manually override the weight (in kg) of the collision shape
	 * @param bUsePhysicsMaterial	If true the friction and restitution params will be ignored and instead pulled from the physics material
	 * @param Id	Returns the id to use in collision lookups
	 */
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Registration", DisplayName="Register Static Rigid Body")
	void RegisterStaticRigidBody(AActor* Target, float Friction, float Restitution, bool bUsePhysicsMaterial, UPARAM(DisplayName="RigidBodyId") int32&Id );
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void SetPhysicsState(int ID, FTransform transforms, FVector Velocity, FVector AngularVelocity,FVector& Force);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void GetPhysicsState(int ID, FTransform& transforms, FVector& Velocity, FVector& AngularVelocity, FVector& Force);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void StepPhysics(float deltaSeconds, int maxSubSteps = 1, float fixedTimeStep = 0.016666667f);
	
	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void AddImpulse(AActor* Target, FVector Impulse, FVector Location);

	UFUNCTION(BlueprintCallable, Category = "Bullet Physics|Objects")
	void AddForce(AActor* Target, FVector Force, FVector Location);
	

	
	
	
	
private:
	btCollisionConfiguration* BtCollisionConfig;
	btCollisionDispatcher* BtCollisionDispatcher;
	btBroadphaseInterface* BtBroadphase;
	btConstraintSolver* BtConstraintSolver;
	btDiscreteDynamicsWorld* BtWorld;
	BulletHelpers* BulletHelpers;
	//BulletDebugDraw* btdebugdraw; TODO:@GreggoryAddison::CodeCompletion || Add debug
	btStaticPlaneShape* plane;
	// Custom debug interface
	btIDebugDraw* BtDebugDraw;
	// Dynamic bodies
	// Static colliders
	TArray<btCollisionObject*> BtStaticObjects;
	btCollisionObject* procbody;
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

	float Accumulator = 0.0f;
	
	// Holds an array of collision object id's for a specific actor.
	UPROPERTY()
	TMap<AActor*, FCollisionObjectArray> ParentObjectCollisionMap; 
	
	
		
#pragma region BULLET SHAPE CREATION
public:
	
	btDiscreteDynamicsWorld* GetBulletWorld() const {return BtWorld;};

	btCollisionShape* GetBoxCollisionShape(const FVector& Dimensions);

	btCollisionShape* GetSphereCollisionShape(float Radius);

	btCollisionShape* GetCapsuleCollisionShape(float Radius, float Height);

	btCollisionShape* GetTriangleMeshShape(TArray<FVector> a, TArray<FVector> b, TArray<FVector> c, TArray<FVector> d);

	btCollisionShape* GetConvexHullCollisionShape(UBodySetup* BodySetup, int ConvexIndex, const FVector& Scale);

	btRigidBody* AddRigidBody(AActor* Actor, const UBulletPhysicsWorldSubsystem::CachedDynamicShapeData& ShapeData, float Friction, float Restitution);

	btRigidBody* AddRigidBody(AActor* Actor, btCollisionShape* CollisionShape, btVector3 Inertia, float Mass, float Friction, float Restitution);

	btRigidBody* AddRigidBody(USkeletalMeshComponent* skel, const FTransform& localTransform, btCollisionShape* collisionShape, float Mass, float Friction, float Restitution);

	btCollisionObject* GetStaticObject(int ID);
	
	
private:
	typedef const std::function<void(btCollisionShape* /*SingleShape*/, const FTransform& /*RelativeXform*/)>& PhysicsGeometryCallback;

	void SetupStaticGeometryPhysics(TArray<AActor*> Actors, float Friction, float Restitution);

	void ExtractPhysicsGeometry(AActor* Actor, PhysicsGeometryCallback CB);

	btCollisionObject* AddStaticCollision(btCollisionShape* Shape, const FTransform& Transform, float Friction, float Restitution, AActor* Actor);

	void ExtractPhysicsGeometry(UStaticMeshComponent* SMC, const FTransform& InvActorXform, PhysicsGeometryCallback CB);

	void ExtractPhysicsGeometry(UShapeComponent* Sc, const FTransform& InvActorXform, PhysicsGeometryCallback CB);

	void ExtractPhysicsGeometry(const FTransform& XformSoFar, UBodySetup* BodySetup, PhysicsGeometryCallback CB);

	const UBulletPhysicsWorldSubsystem::CachedDynamicShapeData& GetCachedDynamicShapeData(AActor* Actor, float Mass);
#pragma endregion
	
	
};
