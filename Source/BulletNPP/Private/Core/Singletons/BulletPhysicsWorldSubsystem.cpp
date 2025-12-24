// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"

#include "EngineUtils.h"

const FVector UE_WORLD_ORIGIN = FVector(0);

void UBulletPhysicsWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	PhysicsDeltaTime = 1/PhysicsRefreshRate;

	BtCollisionConfig = new btDefaultCollisionConfiguration();
	BtCollisionDispatcher = new btCollisionDispatcher(BtCollisionConfig);

	BtBroadphase = new btDbvtBroadphase();

	mt = new btSequentialImpulseConstraintSolver;
	mt->setRandSeed(1234);

	BtConstraintSolver = mt;
	BtWorld = new btDiscreteDynamicsWorld(BtCollisionDispatcher, BtBroadphase, BtConstraintSolver, BtCollisionConfig);
	BtWorld->setGravity(btVector3(Gravity.X,Gravity.Y, Gravity.Z));

	UE_LOG(LogTemp, Warning, TEXT("UBulletPhysicsWorldSubsystem:: Bullet world init"));

	if (GetWorld() == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("UBulletPhysicsWorldSubsystem::GetWorld() returned null"));
		return;
	}
	
}

void UBulletPhysicsWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	FName bulletStaticTag = FName("B_STATIC");
	FName bulletDynamicTag = FName("B_DYNAMIC");
	
	Super::OnWorldBeginPlay(InWorld);
	for (TActorIterator<AActor> actorItr(&InWorld); actorItr; ++actorItr)
	{
		AActor* actor = *actorItr;
		if (!actor)
			continue;

		int dummyID = 0; // Doesn't really matter
		// Check if the actor has a UStaticMeshComponent directly
		if (actor->ActorHasTag(bulletStaticTag))
		{
			RegisterStaticRigidBody(actor,0.5,0.9, false, dummyID);
		}else if (actor->ActorHasTag(bulletDynamicTag))
		{
			RegisterDynamicRigidBody(actor,0.5,0.9,10.f, false,dummyID );
		}
	}
}


// TODO:@GreggoryAddison::CodeUpgrade || This function needs to change. We need an option for a specific primitive component along with the parent to attach it to. In my project a single actor can have multiple shapes that are not all active at one time
void UBulletPhysicsWorldSubsystem::RegisterDynamicRigidBody(AActor* Target, float Friction, float Restitution, float Mass, bool bUsePhysicsMaterial, UPARAM(DisplayName="RigidBodyId") int32&Id )
{
	if (!bUsePhysicsMaterial)
	{
		AddRigidBody(Target, GetCachedDynamicShapeData(Target, Mass), Friction, Restitution);	
	}
	else
	{
		
	}
	
	const int32 objectId = BtRigidBodies.Num() - 1;

	if (ParentObjectCollisionMap.Contains(Target))
	{
		ParentObjectCollisionMap[Target].ObjectIds.AddUnique(objectId);
	}
	else
	{
		ParentObjectCollisionMap.Add(Target, FCollisionObjectArray(objectId));
	}
	
	Id = objectId;
}

void UBulletPhysicsWorldSubsystem::RegisterStaticRigidBody(AActor* Target, float Friction, float Restitution, bool bUsePhysicsMaterial, UPARAM(DisplayName="RigidBodyId") int32&Id )
{
	ExtractPhysicsGeometry(Target,[Target, this, Friction, Restitution](btCollisionShape* Shape, const FTransform& RelTransform)
	{
	// Every sub-collider in the actor is passed to this callback function
	// We're baking this in world space, so apply actor transform to relative
	const FTransform FinalXform = RelTransform * Target->GetActorTransform();
	AddStaticCollision(Shape, FinalXform, Friction, Restitution, Target);
	});
	

	const int32 objectId = BtRigidBodies.Num() - 1;

	if (ParentObjectCollisionMap.Contains(Target))
	{
		ParentObjectCollisionMap[Target].ObjectIds.AddUnique(objectId);
	}
	else
	{
		ParentObjectCollisionMap.Add(Target, FCollisionObjectArray(objectId));
	}
	
	Id = objectId;
}


btCollisionShape* UBulletPhysicsWorldSubsystem::GetBoxCollisionShape(const FVector& Dimensions)
{
	// Simple brute force lookup for now, probably doesn't need anything more clever
	btVector3 HalfSize = BulletHelpers::ToBtSize(Dimensions * 0.5);
	for (auto&& S : BtBoxCollisionShapes)
	{
		btVector3 Sz = S->getHalfExtentsWithMargin();
		if (FMath::IsNearlyEqual(Sz.x(), HalfSize.x()) &&
				FMath::IsNearlyEqual(Sz.y(), HalfSize.y()) &&
				FMath::IsNearlyEqual(Sz.z(), HalfSize.z()))
		{
			return S;
		}
	}

	// Not found, create
	auto S = new btBoxShape(HalfSize);
	// Get rid of margins, just cause issues for me
	S->setMargin(0);
	BtBoxCollisionShapes.Add(S);

	return S;

}

btCollisionShape* UBulletPhysicsWorldSubsystem::GetSphereCollisionShape(float Radius)
{
	// Simple brute force lookup for now, probably doesn't need anything more clever
	btScalar Rad = BulletHelpers::ToBtSize(Radius);
	for (auto&& S : BtSphereCollisionShapes)
	{
		// Bullet subtracts a margin from its internal shape, so add back to compare
		if (FMath::IsNearlyEqual(S->getRadius(), Rad))
		{
			return S;
		}
	}

	// Not found, create
	auto S = new btSphereShape(Rad);
	// Get rid of margins, just cause issues for me
	S->setMargin(0);
	BtSphereCollisionShapes.Add(S);

	return S;

}

btCollisionShape* UBulletPhysicsWorldSubsystem::GetCapsuleCollisionShape(float Radius, float Height)
{
	// Simple brute force lookup for now, probably doesn't need anything more clever
	btScalar R = BulletHelpers::ToBtSize(Radius);
	btScalar H = BulletHelpers::ToBtSize(Height);
	btScalar HalfH = H * 0.5f;

	for (auto&& S : BtCapsuleCollisionShapes)
	{
		// Bullet subtracts a margin from its internal shape, so add back to compare
		if (FMath::IsNearlyEqual(S->getRadius(), R) &&
				FMath::IsNearlyEqual(S->getHalfHeight(), HalfH))
		{
			return S;
		}
	}

	// Not found, create
	auto S = new btCapsuleShape(R, H);
	BtCapsuleCollisionShapes.Add(S);

	return S;

}

btCollisionShape* UBulletPhysicsWorldSubsystem::GetTriangleMeshShape(TArray<FVector> a, TArray<FVector> b, TArray<FVector> c, TArray<FVector> d)
{
	btTriangleMesh* triangleMesh = new btTriangleMesh();

	for (int i =0;i<a.Num();i++)
	{
		triangleMesh->addTriangle(BulletHelpers::ToBtPos(a[i], FVector::ZeroVector), BulletHelpers::ToBtPos(b[i], FVector::ZeroVector), BulletHelpers::ToBtPos(c[i], FVector::ZeroVector));
		triangleMesh->addTriangle(BulletHelpers::ToBtPos(a[i], FVector::ZeroVector), BulletHelpers::ToBtPos(c[i], FVector::ZeroVector), BulletHelpers::ToBtPos(d[i], FVector::ZeroVector));

	}
	btBvhTriangleMeshShape* Trimesh= new btBvhTriangleMeshShape(triangleMesh,true);
	return Trimesh;
}

btCollisionShape* UBulletPhysicsWorldSubsystem::GetConvexHullCollisionShape(UBodySetup* BodySetup, int ConvexIndex, const FVector& Scale)
{
	for (auto&& S : BtConvexHullCollisionShapes)
	{ 
		if (S.BodySetup == BodySetup && S.HullIndex == ConvexIndex && S.Scale.Equals(Scale))
		{
			return S.Shape;
		}
	}

	const FKConvexElem& Elem = BodySetup->AggGeom.ConvexElems[ConvexIndex];
	auto C = new btConvexHullShape();
	for (auto&& P : Elem.VertexData)
	{
		C->addPoint(BulletHelpers::ToBtPos(P, FVector::ZeroVector));
	}
	// Very important! Otherwise there's a gap between 
	C->setMargin(0);
	// Apparently this is good to call?
	C->initializePolyhedralFeatures();

	BtConvexHullCollisionShapes.Add({
			BodySetup,
			ConvexIndex,
			Scale,
			C
			});

	return C;
}




const UBulletPhysicsWorldSubsystem::CachedDynamicShapeData& UBulletPhysicsWorldSubsystem::GetCachedDynamicShapeData(AActor* Actor, float Mass)
{
	// We re-use compound shapes based on (leaf) BP class
	const FName ClassName = Actor->GetClass()->GetFName();



	// Because we want to support compound colliders, we need to extract all colliders first before
	// constructing the final body.
	TArray<btCollisionShape*, TInlineAllocator<20>> Shapes;
	TArray<FTransform, TInlineAllocator<20>> ShapeRelXforms;
	ExtractPhysicsGeometry(Actor,
			[&Shapes, &ShapeRelXforms](btCollisionShape* Shape, const FTransform& RelTransform)
			{
			Shapes.Add(Shape);
			ShapeRelXforms.Add(RelTransform);
			});


	CachedDynamicShapeData ShapeData;
	ShapeData.ClassName = ClassName;

	// Single shape with no transform is simplest
	if (ShapeRelXforms.Num() == 1 &&
			ShapeRelXforms[0].EqualsNoScale(FTransform::Identity))
	{
		ShapeData.Shape = Shapes[0];
		// just to make sure we don't think we have to clean it up; simple shapes are already stored
		ShapeData.bIsCompound = false;
	}
	else
	{
		// Compound or offset single shape; we will cache these by blueprint type
		btCompoundShape* CS = new btCompoundShape();
		for (int i = 0; i < Shapes.Num(); ++i)
		{
			// We don't use the actor origin when converting transform in this case since object space
			// Note that btCompoundShape doesn't free child shapes, which is fine since they're tracked separately
			CS->addChildShape(BulletHelpers::ToBt(ShapeRelXforms[i], FVector::ZeroVector), Shapes[i]);
		}

		ShapeData.Shape = CS;
		ShapeData.bIsCompound = true;
	}

	// Calculate Inertia
	ShapeData.Mass = Mass;
	ShapeData.Shape->calculateLocalInertia(Mass, ShapeData.Inertia);

	// Cache for future use
	CachedDynamicShapes.Add(ShapeData);

	return CachedDynamicShapes.Last();

}

btRigidBody* UBulletPhysicsWorldSubsystem::AddRigidBody(AActor* Actor, const UBulletPhysicsWorldSubsystem::CachedDynamicShapeData& ShapeData, float Friction, float Restitution)
{
	return AddRigidBody(Actor, ShapeData.Shape, ShapeData.Inertia, ShapeData.Mass, Friction, Restitution);
}


btRigidBody* UBulletPhysicsWorldSubsystem::AddRigidBody(AActor* Actor, btCollisionShape* CollisionShape, btVector3 Inertia, float Mass, float Friction, float Restitution)
{

	FBulletMotionState* MotionState = new FBulletMotionState(Actor, UE_WORLD_ORIGIN);
	const btRigidBody::btRigidBodyConstructionInfo rbInfo(Mass, MotionState, CollisionShape, Inertia);
	btRigidBody* body = new btRigidBody(rbInfo);
	body->setUserPointer(Actor);
	body->setActivationState(DISABLE_DEACTIVATION);
	body->setDeactivationTime(0);
	BtWorld->addRigidBody(body);
	BtRigidBodies.Add(body);
	return body;
}

btRigidBody* UBulletPhysicsWorldSubsystem::AddRigidBody(USkeletalMeshComponent* skel, const FTransform& PhysicsAssetTransform, btCollisionShape* collisionShape, float mass, float friction, float restitution)
{
	checkf(skel!=nullptr, TEXT("Got null skeletal mesh"));
	btVector3 inertia(0,0,0);
	checkf(collisionShape!=nullptr, TEXT("Please configure physics asset for: %s"), *skel->GetName());
	collisionShape->calculateLocalInertia(mass, inertia);
	FBulletUEMotionState* objMotionState = new FBulletUEMotionState(skel, UE_WORLD_ORIGIN, PhysicsAssetTransform);
	const btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, objMotionState, collisionShape, inertia);
	btRigidBody* body = new btRigidBody(rbInfo);
	body->setUserPointer(skel->GetOwner());
	body->setActivationState(DISABLE_DEACTIVATION);
	body->setDeactivationTime(0);

	BtWorld->addRigidBody(body);
	BtRigidBodies.Add(body);
	return body;
}

void UBulletPhysicsWorldSubsystem::SetPhysicsState(int ID, FTransform transforms, FVector Velocity, FVector AngularVelocity, FVector& Force)
{

	if (BtRigidBodies[ID]) {
		BtRigidBodies[ID]->setWorldTransform(BulletHelpers::ToBt(transforms, UE_WORLD_ORIGIN));
		BtRigidBodies[ID]->setLinearVelocity(BulletHelpers::ToBtPos(Velocity, UE_WORLD_ORIGIN));
		BtRigidBodies[ID]->setAngularVelocity(BulletHelpers::ToBtPos(AngularVelocity, FVector(0)));
	}


}

void UBulletPhysicsWorldSubsystem::GetPhysicsState(int ID, FTransform& transforms, FVector& Velocity, FVector& AngularVelocity,FVector& Force)
{
	if (BtRigidBodies.Num()<=ID) {
		UE_LOG(LogTemp, Warning, TEXT("No rigid "));
		return;
	}
	if (BtRigidBodies[ID]) {
		transforms= BulletHelpers::ToUE( BtRigidBodies[ID]->getWorldTransform(),UE_WORLD_ORIGIN) ;
		Velocity = BulletHelpers::ToUEPos(BtRigidBodies[ID]->getLinearVelocity(), UE_WORLD_ORIGIN);
		AngularVelocity = BulletHelpers::ToUEPos(BtRigidBodies[ID]->getAngularVelocity(), FVector(0));
		Force = BulletHelpers::ToUEPos(BtRigidBodies[ID]->getTotalForce(), UE_WORLD_ORIGIN);
	}
}

void UBulletPhysicsWorldSubsystem::StepPhysics(float deltaSeconds, int maxSubSteps, float fixedTimeStep)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StepPhysics);
	BtWorld->stepSimulation(deltaSeconds,maxSubSteps,fixedTimeStep);

#if WITH_EDITOR
	if (DebugEnabled) {
		BtWorld->debugDrawWorld();
	}
#endif
}

void UBulletPhysicsWorldSubsystem::AddImpulse(AActor* Target, FVector Impulse, FVector Location)
{
	int32 Id = INDEX_NONE;
	if (!ParentObjectCollisionMap.Contains(Target)) return;
	if (ParentObjectCollisionMap[Target].ObjectIds.Num() == 0) return;
	
	Id = ParentObjectCollisionMap[Target].ObjectIds[0];
	
	BtRigidBodies[Id]->applyImpulse(BulletHelpers::ToBtDir(Impulse, true), BulletHelpers::ToBtPos(Location, UE_WORLD_ORIGIN));
}

void UBulletPhysicsWorldSubsystem::AddForce(AActor* Target, FVector Force, FVector Location)
{
	int32 Id = INDEX_NONE;
	if (!ParentObjectCollisionMap.Contains(Target)) return;
	if (ParentObjectCollisionMap[Target].ObjectIds.Num() == 0) return;
	
	Id = ParentObjectCollisionMap[Target].ObjectIds[0];
	BtRigidBodies[Id]->applyForce(BulletHelpers::ToBtDir(Force, true), BulletHelpers::ToBtPos(Location, UE_WORLD_ORIGIN));
}


btCollisionObject* UBulletPhysicsWorldSubsystem::GetStaticObject(int ID)
{
	return BtStaticObjects[ID];
}


void UBulletPhysicsWorldSubsystem::SetupStaticGeometryPhysics(TArray<AActor*> Actors, float Friction, float Restitution)
{
	for (AActor* Actor : Actors)
	{
		ExtractPhysicsGeometry(Actor,[Actor, this, Friction, Restitution](btCollisionShape* Shape, const FTransform& RelTransform)
		{
		// Every sub-collider in the actor is passed to this callback function
		// We're baking this in world space, so apply actor transform to relative
		const FTransform FinalXform = RelTransform * Actor->GetActorTransform();
		AddStaticCollision(Shape, FinalXform, Friction, Restitution, Actor);
		});
	}
}

void UBulletPhysicsWorldSubsystem::ExtractPhysicsGeometry(AActor* Actor, PhysicsGeometryCallback CB)
{
	TInlineComponentArray<UActorComponent*, 20> Components;
	// Used to easily get a component's transform relative to actor, not parent component
	const FTransform InvActorTransform = Actor->GetActorTransform().Inverse();

	// Collisions from meshes

	Actor->GetComponents(UStaticMeshComponent::StaticClass(), Components);
	for (auto&& Comp : Components)
	{
		ExtractPhysicsGeometry(Cast<UStaticMeshComponent>(Comp), InvActorTransform, CB);
	}

	// Collisions from separate collision components
	Actor->GetComponents(UShapeComponent::StaticClass(), Components);
	for (auto&& Comp : Components)
	{
		ExtractPhysicsGeometry(Cast<UShapeComponent>(Comp), InvActorTransform, CB);
	}
}

btCollisionObject* UBulletPhysicsWorldSubsystem::AddStaticCollision(btCollisionShape* Shape, const FTransform& Transform, float Friction,
		float Restitution, AActor* Actor)
{
	if (!BtWorld){
		UE_LOG(LogTemp, Warning, TEXT("UBulletPhysicsWorldSubsystem::AddStaticCollision: BtWorld is empty"));
		return nullptr;
	}
	btTransform Xform = BulletHelpers::ToBt(Transform, UE_WORLD_ORIGIN);
	btCollisionObject* Obj = new btCollisionObject();
	Obj->setCollisionShape(Shape);
	Obj->setWorldTransform(Xform);
	Obj->setFriction(Friction);
	Obj->setRestitution(Restitution);
	Obj->setUserPointer(Actor);
	Obj->setActivationState(DISABLE_DEACTIVATION);
	BtWorld->addCollisionObject(Obj);
	BtStaticObjects.Add(Obj);
	return Obj;
}

void UBulletPhysicsWorldSubsystem::ExtractPhysicsGeometry(UStaticMeshComponent* SMC, const FTransform& InvActorXform, PhysicsGeometryCallback CB)
{
	UStaticMesh* Mesh = SMC->GetStaticMesh();
	if (!Mesh)
		return;

	
	const FTransform CompTransform = SMC->GetComponentTransform();
	
	// We want the complete transform from actor to this component, not just relative to parent
	FTransform CompFullRelXForm = CompTransform * InvActorXform;
	ExtractPhysicsGeometry(CompFullRelXForm, Mesh->GetBodySetup(), CB);

	// Not supporting complex collision shapes right now
	// If we did, note that Mesh->ComplexCollisionMesh is WITH_EDITORONLY_DATA so not available at runtime
	// See StaticMeshRender.cpp, FStaticMeshSceneProxy::GetDynamicMeshElements
	// Line 1417+, bDrawComplexCollision
	// Looks like we have to access LODForCollision, RenderData->LODResources
	// So they use a mesh LOD for collision for complex shapes, never drawn usually?

}


void UBulletPhysicsWorldSubsystem::ExtractPhysicsGeometry(UShapeComponent* Sc, const FTransform& InvActorXform, PhysicsGeometryCallback CB)
{
	// We want the complete transform from actor to this component, not just relative to parent
	FTransform CompFullRelXForm = Sc->GetComponentTransform() * InvActorXform;
	ExtractPhysicsGeometry(CompFullRelXForm, Sc->ShapeBodySetup, CB);
}


void UBulletPhysicsWorldSubsystem::ExtractPhysicsGeometry(const FTransform& XformSoFar, UBodySetup* BodySetup, PhysicsGeometryCallback CB)
{
	FVector Scale = XformSoFar.GetScale3D();
	btCollisionShape* Shape = nullptr;

	// Iterate over the simple collision shapes
	for (auto&& Box : BodySetup->AggGeom.BoxElems)
	{
		FVector Dimensions = FVector(Box.X, Box.Y, Box.Z) * Scale;
		//TODO: find a better way to represent planes in unreal maybe?
		if (Scale.Z==0) {
			btVector3 planeNormal(0,0,1);
			Shape = new btStaticPlaneShape(planeNormal, 0);	
			UE_LOG(LogTemp, Warning, TEXT("UBulletPhysicsWorldSubsystem:: creating plane"));
		}else { 
			// We'll re-use based on just the LxWxH, including actor scale
			// Rotation and centre will be baked in world space
			UE_LOG(LogTemp, Warning, TEXT("UBulletPhysicsWorldSubsystem:: creating box"));
			Shape = GetBoxCollisionShape(Dimensions);
		}
		FTransform ShapeXform(Box.Rotation, Box.Center);
		// Shape transform adds to any relative transform already here
		FTransform XForm = ShapeXform * XformSoFar;
		CB(Shape, XForm);
	}
	for (auto&& Sphere : BodySetup->AggGeom.SphereElems)
	{
		// Only support uniform scale so use X
		Shape = GetSphereCollisionShape(Sphere.Radius * Scale.X);
		FTransform ShapeXform(FRotator::ZeroRotator, Sphere.Center);
		// Shape transform adds to any relative transform already here
		FTransform XForm = ShapeXform * XformSoFar;
		CB(Shape, XForm);
	}
	// Sphyl == Capsule (??)
	for (auto&& Capsule : BodySetup->AggGeom.SphylElems)
	{
		// X scales radius, Z scales height
		Shape = GetCapsuleCollisionShape(Capsule.Radius * Scale.X, Capsule.Length * Scale.Z);
		// Capsules are in Z in UE, in Y in Bullet, so roll -90
		FRotator Rot(0, 0, -90);
		// Also apply any local rotation
		Rot += Capsule.Rotation;
		FTransform ShapeXform(Rot, Capsule.Center);
		// Shape transform adds to any relative transform already here
		FTransform XForm = ShapeXform * XformSoFar;
		CB(Shape, XForm);
	}
	for (int i = 0; i < BodySetup->AggGeom.ConvexElems.Num(); ++i)
	{
		Shape = GetConvexHullCollisionShape(BodySetup, i, Scale);
		CB(Shape, XformSoFar);
	}

}




