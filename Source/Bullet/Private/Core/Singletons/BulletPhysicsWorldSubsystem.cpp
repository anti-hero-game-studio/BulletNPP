// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/Singletons/BulletPhysicsWorldSubsystem.h"
#include "Core/DataTypes/BulletTypes.h"
#include "BulletCoreSettings.h"
#include "BulletDebugDrawer.h"
#include "BulletLogChannels.h"
#include "EngineUtils.h"
#include "BulletCollision/CollisionDispatch/btGhostObject.h"
#include "Core/CollisionFilters/BulletGhostPairCallBack.h"
#include "Core/CollisionFilters/ConvexResultCallback_IgnoreActors.h"
#include "Core/CollisionFilters/OverlapFilterCallback.h"
#include "Core/CollisionFilters/RaycastResultCallback_IgnoreActors.h"
#include "Core/CollisionFilters/UnrealCollisionDispatcher.h"
#include "Core/CollisionFilters/BulletOverlappingPairCache.h"
#include "Core/ContactHandling/BulletContactGatherer.h"
#include "Core/Interfaces/BulletPrimitiveComponentInterface.h"

int32 DrawDebugShapes = 0;
static FAutoConsoleVariableRef CVarDisableDataCopyInPlace(
	TEXT("b.debug.draw"),
	DrawDebugShapes,
	TEXT("Show the bullet collision shapes according to the bullet world view"),
	ECVF_Default);

const FVector UE_WORLD_ORIGIN = FVector(0);

void UBulletPhysicsWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	PhysicsDeltaTime = 1/PhysicsRefreshRate;

	BtCollisionConfig = new btDefaultCollisionConfiguration();
	BtCollisionDispatcher = new FUnrealCollisionDispatcher(BtCollisionConfig);
	OverlappingPairCache = new FBulletOverlappingPairCache();
	BtBroadphase = new btDbvtBroadphase(OverlappingPairCache);

	mt = new btSequentialImpulseConstraintSolver;
	mt->setRandSeed(1234);

	BtConstraintSolver = mt;
	BtWorld = new btDiscreteDynamicsWorld(BtCollisionDispatcher, BtBroadphase, BtConstraintSolver, BtCollisionConfig);
	BtWorld->setGravity(btVector3(Gravity.X,Gravity.Y, Gravity.Z));
	
	BtWorld->getPairCache()->setOverlapFilterCallback(&OverlapFilterCallback);
	BtBroadphase->getOverlappingPairCache()->setInternalGhostPairCallback(new FBulletGhostPairCallBack());
	
	BtWorld->setForceUpdateAllAabbs(false); //TODO:@GreggoryAddison::UserOptions || This should be a configurable option in the settings object.

	UE_LOG(LogTemp, Warning, TEXT("UBulletPhysicsWorldSubsystem:: Bullet world init"));

	if (GetWorld() == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("UBulletPhysicsWorldSubsystem::GetWorld() returned null"));
		return;
	}
	
}

void UBulletPhysicsWorldSubsystem::OnWorldEndPlay(UWorld& InWorld)
{
	if (BtWorld)
	{
		BtWorld->getPairCache()->setOverlapFilterCallback(nullptr);
	}
	Super::OnWorldEndPlay(InWorld);
}

void UBulletPhysicsWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	FName BypassTag = FName("bypass");
	
	Super::OnWorldBeginPlay(InWorld);
	
	const UBulletCoreSettings* Settings = GetDefault<UBulletCoreSettings>(); 
	for (TActorIterator<AActor> ActorItr(&InWorld); ActorItr; ++ActorItr)
	{
		AActor* Actor = *ActorItr;
		if (!Actor) continue;

		bool bShouldRegister = true;
		
		if (GlobalShapeDescriptorDataCache.Contains(Actor))
		{
			bShouldRegister = false;
		}
		
		if (!bShouldRegister) continue;
		
		RegisterBulletRigidBody(Actor);
		
	}
}


FUnrealShapeId UBulletPhysicsWorldSubsystem::RegisterBulletRigidBody(AActor* Target)
{
	FUnrealShapeDescriptor Descriptor = GlobalShapeDescriptorDataCache.Contains(Target) ? GlobalShapeDescriptorDataCache[Target] : FUnrealShapeDescriptor();
	Descriptor.ShapeOwner = Target;
	
	ExtractPhysicsGeometry(Target,[Target, this, &Descriptor](btCollisionShape* Shape, const FTransform& RelTransform, const FBulletShapeOptions& Options)
	{
		// Every sub-collider in the actor is passed to this callback function
		// We're baking this in world space, so apply actor transform to relative
		const FTransform FinalXform = RelTransform;
		
		bool bShouldCreateGhostCollider = false;
		bool bShouldCreateBlockCollider = false;
		bool IsBlockingAnyTraceChannel = false;

		if (UPrimitiveComponent* P = Descriptor.Shapes.Last().Shape.Get())
		{
			IBulletPrimitiveComponentInterface* I = Cast<IBulletPrimitiveComponentInterface>(P);
			if (!I) return;
			
			const FCollisionResponseContainer& ResponseContainer = I->GetDefaultResponseContainer();
			const UCollisionProfile* Profile = UCollisionProfile::Get();
			for (int i = 0; i < 32; i++)
			{
				const FString& ChannelName = Profile->ReturnChannelNameFromContainerIndex(i).ToString();
				const bool bShouldConsiderChannel = !(ChannelName.Contains(TEXT("GameTraceChannel")) || ChannelName.Contains(TEXT("EngineTraceChannel")));
				if (!bShouldConsiderChannel) continue;
				
				const ECollisionChannel& Channel = (ECollisionChannel)(i);
				const ECollisionResponse& Response = P->GetCollisionResponseToChannels().GetResponse(Channel);
					
				if (Response == ECR_Overlap || Options.bGenerateOverlapEventsInBullet)
				{
					bShouldCreateGhostCollider = true;
				}
				else if (Response == ECR_Block)
				{
					bShouldCreateBlockCollider = Options.bGenerateCollisionEventsInBullet;
					IsBlockingAnyTraceChannel = Profile->ConvertToTraceType(Channel) != TraceTypeQuery_MAX;
				}
			}
			
			if (!Options.bGenerateCollisionEventsInChaos)
			{
				P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				P->GetBodyInstance()->bNotifyRigidBodyCollision = false;
			}
			if (!Options.bGenerateOverlapEventsInChaos)
			{
				P->SetGenerateOverlapEvents(false);
			}
			
			Descriptor.Shapes.Last().CollisionResponses = ResponseContainer;
		}
		
		if (Options.ShapeType == EBulletShapeType::DYNAMIC || Options.ShapeType == EBulletShapeType::KINEMATIC)
		{
			if (btCollisionObject* CollisionObject = AddRigidBodyCollider(Target, RelTransform, Shape, Options))
			{
				CollisionObject->setUserPointer(Descriptor.Shapes.Last().Shape.Get());
				Descriptor.Shapes.Last().Id.BlockingShapeWorldArrayIndex = CollisionObject->getWorldArrayIndex();
				Descriptor.Shapes.Last().BlockingCollider = CollisionObject;
			}
		}
		else
		{
			if (bShouldCreateBlockCollider)
			{
				if (btCollisionObject* CollisionObject = AddStaticCollider(Shape, FinalXform, Options.Friction, Options.Restitution, Target))
				{
					CollisionObject->setUserPointer(Descriptor.Shapes.Last().Shape.Get());
					Descriptor.Shapes.Last().Id.BlockingShapeWorldArrayIndex = CollisionObject->getWorldArrayIndex();
					Descriptor.Shapes.Last().BlockingCollider = CollisionObject;
				}
			}
		}
		
		
		
		if (bShouldCreateGhostCollider)
		{
			if (btGhostObject* CollisionObject = AddGhostCollider(Shape, FinalXform, Target, IsBlockingAnyTraceChannel))
			{
				CollisionObject->setUserPointer(Descriptor.Shapes.Last().Shape.Get());
				Descriptor.Shapes.Last().Id.OverlappingShapeWorldArrayIndex = CollisionObject->getWorldArrayIndex();
				Descriptor.Shapes.Last().OverlappingCollider = CollisionObject;
			}
		}
		
		GlobalShapeDescriptorDataCache.Add(Target, Descriptor);
		
	}, Descriptor);
	
	
	return Descriptor.Shapes.IsEmpty() ? FUnrealShapeId() : Descriptor.Shapes.Last().Id;
}


btCollisionShape* UBulletPhysicsWorldSubsystem::GetBoxCollisionShape(const FVector& Dimensions)
{
	// Simple brute force lookup for now, probably doesn't need anything more clever
	btVector3 HalfSize = BulletHelpers::ToBulletVector3(Dimensions * 0.5);
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
	btScalar Rad = BulletHelpers::ToBulletFloat(Radius);
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
	btScalar R = BulletHelpers::ToBulletFloat(Radius);
	btScalar H = BulletHelpers::ToBulletFloat(Height);
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
	auto S = new btCapsuleShapeZ(R, H);
	BtCapsuleCollisionShapes.Add(S);

	return S;

}

btBvhTriangleMeshShape* UBulletPhysicsWorldSubsystem::GetComplexShape(const FTransform& CurrentTransform, UStaticMesh* Mesh)
{
	/* Note that Mesh->ComplexCollisionMesh is WITH_EDITORONLY_DATA so not available at runtime
	   Looks like we have to access LODForCollision, RenderData->LODResources
	   So they use a mesh LOD for collision for complex shapes, never drawn usually?*/

	FStaticMeshRenderData* renderData = Mesh->GetRenderData();
	if (!renderData)
	{
		UE_LOG(LogBullet, Error, TEXT("Invalid render data. (complex collision extraction)"));
		return nullptr;
	}
	if (renderData->LODResources.Num() == 0)
	{
		UE_LOG(LogBullet, Error, TEXT("LODResources zero. (complex collision extraction)"));
		return nullptr;
	}
	
	bool bHasExistingMeshInCache = false;
	
	FStaticMeshLODResources& LODResources = renderData->LODResources[0]; //TODO@GreggoryAddison::CodeOptimization || Do NOT do this if the mesh is a nanite mesh. To be safe use the proxy mesh if possible.

	const FPositionVertexBuffer& VertexBuffer = LODResources.VertexBuffers.PositionVertexBuffer;

	const uint32 vertices = VertexBuffer.GetNumVertices();
	
	btTriangleMesh* TriangleMesh = new btTriangleMesh();
	FVector scale = CurrentTransform.GetScale3D();
	
	const FIndexArrayView Indices = LODResources.IndexBuffer.GetArrayView();
	
	//TODO:@GreggoryAddison::CodeOptimization || Check if the cache already has a shape with similar verts and scaling.
	/*for (btBvhTriangleMeshShape* CacheShape : ComplexShapes)
	{
		
	}*/
	
	/* Only supporting 1 material for the mesh for now
	 * 
	 */
	const int MaterialIDX = 0; //TODO@GreggoryAddison::CodeCompletion || To support multiple material ids I would need to find out what material id is associated with this triangle. ONLY IMPORTANT IF THIS IS RENDERED
	for (int32 i = 0; i < Indices.Num(); i += 3)
	{
		uint32 verIdx1 = Indices[i];
		uint32 verIdx2 = Indices[i + 1];
		uint32 verIdx3 = Indices[i + 2];

		// Validate indices
		if (verIdx1 >= vertices || verIdx2 >= vertices || verIdx3 >= vertices)
		{
			UE_LOG(LogBullet, Error, TEXT("Invalid triangle indices detected!"));
			continue;
		}
		const btVector3 Vert1 = BulletHelpers::ToBulletPosition(VertexBuffer.VertexPosition(verIdx1), UE_WORLD_ORIGIN);
		const btVector3 Vert2 = BulletHelpers::ToBulletPosition(VertexBuffer.VertexPosition(verIdx2), UE_WORLD_ORIGIN);
		const btVector3 Vert3 = BulletHelpers::ToBulletPosition(VertexBuffer.VertexPosition(verIdx3), UE_WORLD_ORIGIN);
		TriangleMesh->addTriangle(Vert1, Vert2, Vert3);
	}
	
	TriangleMesh->setScaling(BulletHelpers::ToBulletVector3(scale));

	if (Mesh->GetBodySetup())
	{
		//physicsMaterialList.push_back(GetJoltPhysicsMaterial(Mesh->GetBodySetup()->GetPhysMaterial()));
	}
	// TODO@GreggoryAddison::CodeCompletion || Caching mechanism for MeshShapes
	
	btBvhTriangleMeshShape* TriMesh = new btBvhTriangleMeshShape(TriangleMesh,true);
	
	ComplexShapes.Add(TriMesh);

	if (!TriMesh)
	{
		UE_LOG(LogBullet, Error, TEXT("Failed to create mesh."));
		return nullptr;
	}
	
	return TriMesh;
}

btCollisionShape* UBulletPhysicsWorldSubsystem::GetTriangleMeshShape(TArray<FVector> a, TArray<FVector> b, TArray<FVector> c, TArray<FVector> d)
{
	btTriangleMesh* triangleMesh = new btTriangleMesh();

	for (int i =0;i<a.Num();i++)
	{
		triangleMesh->addTriangle(BulletHelpers::ToBulletPosition(a[i], FVector::ZeroVector), BulletHelpers::ToBulletPosition(b[i], FVector::ZeroVector), BulletHelpers::ToBulletPosition(c[i], FVector::ZeroVector));
		triangleMesh->addTriangle(BulletHelpers::ToBulletPosition(a[i], FVector::ZeroVector), BulletHelpers::ToBulletPosition(c[i], FVector::ZeroVector), BulletHelpers::ToBulletPosition(d[i], FVector::ZeroVector));

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
		C->addPoint(BulletHelpers::ToBulletPosition(P, FVector::ZeroVector));
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
	FUnrealShapeDescriptor Descriptor;
	ExtractPhysicsGeometry(Actor,
			[&Shapes, &ShapeRelXforms](btCollisionShape* Shape, const FTransform& RelTransform,  const FBulletShapeOptions& Options)
			{
				Shapes.Add(Shape);
				ShapeRelXforms.Add(RelTransform);
			}, Descriptor);


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
			CS->addChildShape(BulletHelpers::ToBulletTransform(ShapeRelXforms[i], FVector::ZeroVector), Shapes[i]);
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

FUnrealShapeDescriptor UBulletPhysicsWorldSubsystem::GetShapeDescriptorData(const AActor* Actor) const
{
	if (!Actor) return FUnrealShapeDescriptor();
	if (GlobalShapeDescriptorDataCache.IsEmpty()) return FUnrealShapeDescriptor();
	if (!GlobalShapeDescriptorDataCache.Contains(Actor)) return FUnrealShapeDescriptor();
	
	return GlobalShapeDescriptorDataCache[Actor];
}

int32 UBulletPhysicsWorldSubsystem::GetActorRootShapeId(const AActor* Actor) const
{
	if (!Actor) return INDEX_NONE;
	if (GlobalShapeDescriptorDataCache.IsEmpty()) return INDEX_NONE;
	if (!GlobalShapeDescriptorDataCache.Contains(Actor)) return INDEX_NONE;
	
	return GlobalShapeDescriptorDataCache[Actor].GetRootColliderId(); 
}

bool UBulletPhysicsWorldSubsystem::IsBodyValid(const UPrimitiveComponent* Target) const
{
	if (!Target) return false;
	
	if (!Target->GetOwner()) return false;
	
	if (!GlobalShapeDescriptorDataCache.Contains(Target->GetOwner())) return false;
	
	return true;
}

bool UBulletPhysicsWorldSubsystem::HasGhostBodyBeenCreated(const UPrimitiveComponent* Target) const
{
	if (!IsBodyValid(Target)) return false;
	
	const FUnrealShapeDescriptor& Desc = GlobalShapeDescriptorDataCache[Target->GetOwner()];

	const btCollisionObject* Block = BtWorld->getCollisionObjectArray()[Desc.Find(Target, false)];
	
	if (!Block) return false;
	
	return true;
}

bool UBulletPhysicsWorldSubsystem::HasRigidBodyBeenCreated(const UPrimitiveComponent* Target) const
{
	if (!IsBodyValid(Target)) return false;
	
	const FUnrealShapeDescriptor& Desc = GlobalShapeDescriptorDataCache[Target->GetOwner()];

	const btCollisionObject* Block = BtWorld->getCollisionObjectArray()[Desc.Find(Target)];
	
	if (!Block) return false;
	
	return true;
}

bool UBulletPhysicsWorldSubsystem::IsCollisionBodyActive(const UPrimitiveComponent* Target) const
{
	if (!IsBodyValid(Target)) return false;
	
	const FUnrealShapeDescriptor& Desc = GlobalShapeDescriptorDataCache[Target->GetOwner()];

	const btCollisionObject* Block = BtWorld->getCollisionObjectArray()[Desc.Find(Target)];
	
	if (!Block) return false;
	
	return Block->getActivationState() == ACTIVE_TAG;
}

bool UBulletPhysicsWorldSubsystem::IsGhostBodyActive(const UPrimitiveComponent* Target) const
{
	if (!IsBodyValid(Target)) return false;
	
	const FUnrealShapeDescriptor& Desc = GlobalShapeDescriptorDataCache[Target->GetOwner()];

	const btCollisionObject* Overlap = BtWorld->getCollisionObjectArray()[Desc.Find(Target, false)];
	
	if (!Overlap) return false;
	
	return Overlap->getActivationState() == ACTIVE_TAG;
}

void UBulletPhysicsWorldSubsystem::SetRigidBodyActiveState(const UPrimitiveComponent* Target, bool Active) const
{
	if (!IsBodyValid(Target)) return;
	
	const FUnrealShapeDescriptor& Desc = GlobalShapeDescriptorDataCache[Target->GetOwner()];
	
	btCollisionObject* C = BtWorld->getCollisionObjectArray()[Desc.Find(Target)];
	
	if (!C) return;
	
	btRigidBody* Rb = btRigidBody::upcast(C);
	
	if (!Rb) return;
	
	if (Active)
	{
		Rb->forceActivationState(WANTS_DEACTIVATION);
		Rb->setDeactivationTime(0.f);
	}
	else
	{
		Rb->activate();
	}
}

const FCollisionResponseContainer& UBulletPhysicsWorldSubsystem::GetCollisionResponseContainer(const UPrimitiveComponent* Target) const
{
	if (!IsBodyValid(Target)) return DefaultCollisionResponseContainer;
	
	const FUnrealShapeDescriptor& Desc = GlobalShapeDescriptorDataCache[Target->GetOwner()];
	
	return Desc.GetCollisionResponseContainer(Target); 
}

void UBulletPhysicsWorldSubsystem::BroadcastSymmetricHits(const FBulletHitEvent& Base)
{
	// Self (as stored)
	BroadcastComponentHit(Base);

	// Mirrored for the other component
	FBulletHitEvent Mirror = Base;
	Mirror.SelfComp = Base.OtherComp;
	Mirror.OtherComp = Base.SelfComp;
	Mirror.ImpactNormal = -Base.ImpactNormal;
	Mirror.ImpulseDir = -Base.ImpulseDir;
	BroadcastComponentHit(Mirror);
}

void UBulletPhysicsWorldSubsystem::BroadcastComponentHit(const FBulletHitEvent& E)
{
	UPrimitiveComponent* Self = E.SelfComp.Get();
	UPrimitiveComponent* Other = E.OtherComp.Get();
	if (!Self || !Other) return;

	AActor* SelfOwner = Self->GetOwner();
	AActor* OtherOwner = Other->GetOwner();
	if (!SelfOwner || !OtherOwner) return;

	FHitResult Hit;
	Hit.bBlockingHit = true;

	Hit.Component = Self;
	Hit.HitObjectHandle = OtherOwner;

	Hit.ImpactPoint = E.ImpactPoint;
	Hit.Location = E.ImpactPoint;

	Hit.ImpactNormal = E.ImpactNormal;
	Hit.Normal = E.ImpactNormal;

	Hit.PenetrationDepth = E.PenetrationDepth;

	// These are optional; set if you can compute them.
	Hit.BoneName = NAME_None;
	Hit.MyBoneName = NAME_None;

	// HitResult has PhysMaterial, FaceIndex, Item, etc. (usually unavailable from Bullet without extra bookkeeping)

	// Unreal's OnComponentHit signature is:
	// (UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	//  FVector NormalImpulse, const FHitResult& Hit)
	const FVector NormalImpulse = E.ImpulseDir * E.AppliedImpulse; // You may scale/tune as needed

	// Broadcast on component
	Self->OnComponentHit.Broadcast(Self, OtherOwner, Other, NormalImpulse, Hit);

	// Optional: call NotifyHit on the owning actor (many gameplay systems listen here)
	// Actor NotifyHit signature differs slightly; pass values as best-effort.
	SelfOwner->NotifyHit(
		Self,
		OtherOwner,
		Other,
		true,               // bSelfMoved (unknown; set true if self is kinematic/moved)
		E.ImpactPoint,
		E.ImpactNormal,
		NormalImpulse,
		Hit
	);
}


btRigidBody* UBulletPhysicsWorldSubsystem::AddRigidBodyCollider(AActor* Actor, const FTransform& FinalTransform, btCollisionShape* CollisionShape,  const FBulletShapeOptions& Options)
{
	btVector3 Inertia;
	CollisionShape->calculateLocalInertia(Options.Mass, Inertia);
	FBulletMotionState* MotionState = new FBulletMotionState(Actor, UE_WORLD_ORIGIN);
	const btRigidBody::btRigidBodyConstructionInfo RBInfo(Options.Mass, MotionState, CollisionShape, Inertia);
	btRigidBody* Body = new btRigidBody(RBInfo);
	Body->setWorldTransform(BulletHelpers::ToBulletTransform(FinalTransform, UE_WORLD_ORIGIN));
	
	if (Options.bKeepShapeVertical)
	{
		Body->setAngularFactor(btVector3(0, 0, 1));
	}
	
	if (Options.bAutomaticallyActivate)
	{
		Body->forceActivationState(Options.bDisableDeactivation ? DISABLE_DEACTIVATION : ACTIVE_TAG);
		Body->setDeactivationTime(0);
	}
	else
	{
		Body->forceActivationState(ISLAND_SLEEPING);
		Body->setDeactivationTime(0.f);
	}
	
	BtWorld->addRigidBody(Body);
	BtRigidBodies.Add(Body);
	return Body;
}

btRigidBody* UBulletPhysicsWorldSubsystem::AddRigidBodyCollider(USkeletalMeshComponent* Skel, const FTransform& PhysicsAssetTransform, btCollisionShape* CollisionShape, const float Mass, float Friction, float Restitution)
{
	checkf(Skel!=nullptr, TEXT("Got null skeletal mesh"));
	btVector3 Inertia(0,0,0);
	checkf(CollisionShape!=nullptr, TEXT("Please configure physics asset for: %s"), *Skel->GetName());
	CollisionShape->calculateLocalInertia(Mass, Inertia);
	FBulletUEMotionState* OBJMotionState = new FBulletUEMotionState(Skel, UE_WORLD_ORIGIN, PhysicsAssetTransform);
	const btRigidBody::btRigidBodyConstructionInfo RBInfo(Mass, OBJMotionState, CollisionShape, Inertia);
	btRigidBody* Body = new btRigidBody(RBInfo);
	Body->setUserPointer(Skel);
	Body->setActivationState(DISABLE_DEACTIVATION);
	Body->setDeactivationTime(0);
	BtWorld->addRigidBody(Body);
	BtRigidBodies.Add(Body);
	return Body;
}

btCollisionObject* UBulletPhysicsWorldSubsystem::AddStaticCollider(btCollisionShape* Shape, const FTransform& Transform, float Friction, float Restitution, AActor* Actor)
{
	if (!BtWorld){
		UE_LOG(LogTemp, Warning, TEXT("UBulletPhysicsWorldSubsystem::AddStaticCollision: BtWorld is empty"));
		return nullptr;
	}
	btTransform Xform = BulletHelpers::ToBulletTransform(Transform, UE_WORLD_ORIGIN);
	btCollisionObject* Obj = new btCollisionObject();
	Obj->setCollisionShape(Shape);
	Obj->setWorldTransform(Xform);
	Obj->setFriction(Friction);
	Obj->setRestitution(Restitution);
	Obj->setActivationState(DISABLE_DEACTIVATION);
	BtWorld->addCollisionObject(Obj);
	return Obj;
}

btGhostObject* UBulletPhysicsWorldSubsystem::AddGhostCollider(btCollisionShape* Shape, const FTransform& Transform, AActor* Actor, const bool bIsBlockingAnyTraceChannel)
{
	btGhostObject* Ghost = new btGhostObject();
	
	Ghost->setCollisionShape(Shape);
	if (!bIsBlockingAnyTraceChannel)
	{
		Ghost->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
	}
	Ghost->setWorldTransform(BulletHelpers::ToBulletTransform(Transform, UE_WORLD_ORIGIN));
	BtWorld->addCollisionObject(Ghost);
	
	
	return Ghost;
}


void UBulletPhysicsWorldSubsystem::SetPhysicsState(const int ID, const FTransform Transforms, const FVector Velocity, const FVector AngularVelocity, FVector& Force)
{
	if (BtRigidBodies[ID]) {
		BtRigidBodies[ID]->setWorldTransform(BulletHelpers::ToBulletTransform(Transforms, UE_WORLD_ORIGIN));
		BtRigidBodies[ID]->setLinearVelocity(BulletHelpers::ToBulletPosition(Velocity, UE_WORLD_ORIGIN));
		BtRigidBodies[ID]->setAngularVelocity(BulletHelpers::ToBulletPosition(AngularVelocity, FVector(0)));
	}
}

void UBulletPhysicsWorldSubsystem::GetPhysicsState(int ID, FTransform& Transforms, FVector& Velocity, FVector& AngularVelocity,FVector& Force)
{
	btCollisionObjectArray& CollisionObjects = GetBulletWorld()->getCollisionObjectArray();
	
	if (CollisionObjects.size()<=ID) {
		UE_LOG(LogTemp, Warning, TEXT("No Collision Object"));
		return;
	}
	
	if (!CollisionObjects[ID]) {
		UE_LOG(LogTemp, Warning, TEXT("Null Collision Object"));
		return;
	}

	if (btRigidBody* Rb = btRigidBody::upcast(CollisionObjects[ID]))
	{
		Transforms= BulletHelpers::ToUnrealTransform( Rb->getWorldTransform(),UE_WORLD_ORIGIN) ;
		Velocity = BulletHelpers::ToUnrealPosition(Rb->getLinearVelocity(), UE_WORLD_ORIGIN);
		AngularVelocity = BulletHelpers::ToUnrealPosition(Rb->getAngularVelocity(), FVector(0));
		Force = BulletHelpers::ToUnrealPosition(Rb->getTotalForce(), UE_WORLD_ORIGIN);
	}
	
}

void UBulletPhysicsWorldSubsystem::GetMotionState(int Id, FTransform& Transforms, FVector& Velocity, FVector& AngularVelocity, FVector& Force)
{
	if (Id == INDEX_NONE) return; 
	
	btCollisionObjectArray& CollisionObjects = GetBulletWorld()->getCollisionObjectArray();
	
	if (CollisionObjects.size()<=Id)
	{
		UE_LOG(LogBullet, Error, TEXT("No Collision Object"));
		return;
	}
	
	if (!CollisionObjects[Id]) 
	{
		UE_LOG(LogBullet, Error, TEXT("Null Collision Object"));
		return;
	}

	if (btRigidBody* Rb = btRigidBody::upcast(CollisionObjects[Id]))
	{
		if (!Rb->getMotionState()) 
		{
			UE_LOG(LogBullet, Error, TEXT("Rigid body doesn't have a valid motion state"));
			return;
		}
		
		const FBulletMotionState* MotionState = static_cast<FBulletMotionState*>(Rb->getMotionState());
		if (!MotionState) 
		{
			UE_LOG(LogBullet, Error, TEXT("Rigid body doesn't have a valid motion state"));
			return;
		}
		
		Transforms = MotionState->GetFinalTransform();
		Velocity = BulletHelpers::ToUnrealPosition(Rb->getLinearVelocity(), UE_WORLD_ORIGIN);
		AngularVelocity = BulletHelpers::ToUnrealPosition(Rb->getAngularVelocity(), FVector(0));
		Force = BulletHelpers::ToUnrealPosition(Rb->getTotalForce(), UE_WORLD_ORIGIN);
	}
	else
	{
		UE_LOG(LogBullet, Error, TEXT("ID did not point to a rigid body"));
	}
}

void UBulletPhysicsWorldSubsystem::StepPhysics(const float DeltaSeconds, const int MaxSubSteps, const float FixedTimeStep)
{
	if (OnPrePhysicsStep.IsBound())
	{
		OnPrePhysicsStep.Broadcast(FixedTimeStep);
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(StepPhysics);
	BtWorld->stepSimulation(DeltaSeconds,MaxSubSteps,FixedTimeStep);

#if WITH_EDITOR
	if (DrawDebugShapes == 1) 
	{
		StartDebugDrawer();
		BtWorld->debugDrawWorld();
	}
	else
	{
		StopDebugDrawer();
	}
#endif

	if (OnPostPhysicsStep.IsBound())
	{
		OnPostPhysicsStep.Broadcast(FixedTimeStep);
	}
	
	Gatherer.Gather(BtWorld);

	for (const FBulletHitEvent& Event : Gatherer.OutEvents)
	{
		BroadcastSymmetricHits(Event);
	}
	
}

void UBulletPhysicsWorldSubsystem::AddImpulse(AActor* Target, const FVector Impulse)
{
	int32 Id = INDEX_NONE;
	const FUnrealShapeDescriptor& Descriptor = GetShapeDescriptorData(Target);
	Id = Descriptor.GetRootColliderId();
	
	if (Id == INDEX_NONE) return;
	
	btCollisionObject* C = GetBulletWorld()->getCollisionObjectArray()[Id];
	btRigidBody* Rb = btRigidBody::upcast(C);
	if (!Rb) return;
	Rb->applyCentralImpulse(BulletHelpers::ToBulletDirection(Impulse, true));
}

void UBulletPhysicsWorldSubsystem::AddForce(AActor* Target, const FVector Force)
{
	int32 Id = INDEX_NONE;
	const FUnrealShapeDescriptor& Descriptor = GetShapeDescriptorData(Target);
	Id = Descriptor.GetRootColliderId();
	
	if (Id == INDEX_NONE) return;
	
	btCollisionObject* C = GetBulletWorld()->getCollisionObjectArray()[Id];
	btRigidBody* Rb = btRigidBody::upcast(C);
	if (!Rb) return;
	Rb->applyCentralForce(BulletHelpers::ToBulletDirection(Force, true));
}

void UBulletPhysicsWorldSubsystem::SetAngularVelocity(AActor* Target, const FVector AngularVelocity)
{
	int32 Id = INDEX_NONE;
	const FUnrealShapeDescriptor& Descriptor = GetShapeDescriptorData(Target);
	Id = Descriptor.GetRootColliderId();
	
	if (Id == INDEX_NONE) return;
	
	btCollisionObject* C = GetBulletWorld()->getCollisionObjectArray()[Id];
	btRigidBody* Rb = btRigidBody::upcast(C);
	if (!Rb) return;
	Rb->setAngularVelocity(BulletHelpers::ToBulletVector3(AngularVelocity));
}

void UBulletPhysicsWorldSubsystem::UpdateActorVelocity(AActor* Target, const FVector LinearVelocity, const FVector AngularVelocity)
{
	int32 Id = INDEX_NONE;
	const FUnrealShapeDescriptor& Descriptor = GetShapeDescriptorData(Target);
	Id = Descriptor.GetRootColliderId();
	
	if (Id == INDEX_NONE) return;
	
	btCollisionObject* C = GetBulletWorld()->getCollisionObjectArray()[Id];
	btRigidBody* Rb = btRigidBody::upcast(C);
	if (!Rb) return;
	Rb->applyCentralForce(BulletHelpers::ToBulletDirection(LinearVelocity, true));
	Rb->setAngularVelocity(BulletHelpers::ToBulletVector3(AngularVelocity));
}

TArray<AActor*> UBulletPhysicsWorldSubsystem::GetOverlappingActors(AActor* Target) const
{
	TArray<AActor*> OverlappingActors;
	const FUnrealShapeDescriptor& Descriptor = GetShapeDescriptorData(Target);
	
	TArray<btGhostObject*> OverlappedObjects = Descriptor.GetAllOverlappingColliders();

	if (OverlappedObjects.IsEmpty()) return OverlappingActors;
	
	for (const btGhostObject* O : OverlappedObjects)
	{
		if (!O) continue;
		for (int i = 0; i < O->getNumOverlappingObjects(); ++i)
		{
			if (!O->getOverlappingObject(i)->getUserPointer()) continue;
			if (const USceneComponent* OverlappedActor = static_cast<USceneComponent*>(O->getOverlappingObject(i)->getUserPointer()))
			{
				if (!OverlappedActor->GetOwner()) continue;
				OverlappingActors.Add(OverlappedActor->GetOwner());
			}
		}
	}
	
	return OverlappingActors;
}

void UBulletPhysicsWorldSubsystem::StartDebugDrawer()
{
	if (BtDebugDraw != nullptr) return;
	BtDebugDraw = new BulletDebugDraw(GetWorld(), UE_WORLD_ORIGIN);
	BtWorld->setDebugDrawer(BtDebugDraw);
}

void UBulletPhysicsWorldSubsystem::StopDebugDrawer()
{
	if (!BtDebugDraw) return;
	BtWorld->setDebugDrawer(nullptr);
	delete BtDebugDraw;
}

FHitResult UBulletPhysicsWorldSubsystem::LineTraceSingleByChannel(const FVector Start, const FVector End, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, int32& HitBodyId)
{
	FHitResult Hit(-1);
	HitBodyId = LineTraceSingle(Start, End, Channel, ActorsToIgnore, Hit);
	
	return Hit;
}

TArray<FHitResult> UBulletPhysicsWorldSubsystem::LineTraceMultiByChannel(const FVector Start, const FVector End,
	const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, TArray<int32>& HitBodyIds)
{
	TArray<FHitResult> Hits;
	HitBodyIds = LineTraceMulti(Start, End, Channel, ActorsToIgnore, Hits);
	
	return Hits;
}

FHitResult UBulletPhysicsWorldSubsystem::SweepSphereSingleByChannel(const float Radius, const FVector Start, const FVector End, 
                                                                    const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, int32& HitBodyId)
{
	FHitResult Hit(-1);

	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	HitBodyId = SweepTraceSingle(Shape, Start, End, FQuat::Identity, Channel, ActorsToIgnore, Hit);
	
	return Hit;
}

TArray<FHitResult> UBulletPhysicsWorldSubsystem::SweepSphereMultiByChannel(const float Radius, const FVector Start,
	const FVector End, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore,
	TArray<int32>& HitBodyIds)
{
	TArray<FHitResult> Hits;

	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
	HitBodyIds = SweepTraceMulti(Shape, Start, End, FQuat::Identity, Channel, ActorsToIgnore, Hits);
	
	return Hits;
}

FHitResult UBulletPhysicsWorldSubsystem::SweepCapsuleSingleByChannel(const float Radius, const float HalfHeight,
                                                                     const FVector Start, const FVector End, const FRotator Rotation, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, int32& HitBodyId)
{
	FHitResult Hit(-1);

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	HitBodyId = SweepTraceSingle(Shape, Start, End,Rotation.Quaternion(), Channel, ActorsToIgnore, Hit);
	
	return Hit;
}

TArray<FHitResult> UBulletPhysicsWorldSubsystem::SweepCapsuleMultiByChannel(const float Radius, const float HalfHeight,
	const FVector Start, const FVector End, const FRotator Rotation, const TEnumAsByte<ECollisionChannel> Channel,
	const TArray<AActor*>& ActorsToIgnore, TArray<int32>& HitBodyIds)
{
	TArray<FHitResult> Hits;

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	HitBodyIds = SweepTraceMulti(Shape, Start, End,Rotation.Quaternion(), Channel, ActorsToIgnore, Hits);
	
	return Hits;
}

FHitResult UBulletPhysicsWorldSubsystem::SweepBoxSingleByChannel(const FVector BoxExtents, const FVector Start, const FVector End, 
                                                                 const FRotator Rotation, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, int32& HitBodyId)
{
	FHitResult Hit(-1);

	const FCollisionShape Shape = FCollisionShape::MakeBox(BoxExtents);
	HitBodyId = SweepTraceSingle(Shape, Start, End, Rotation.Quaternion(), Channel, ActorsToIgnore, Hit);
	
	return Hit;
}

TArray<FHitResult> UBulletPhysicsWorldSubsystem::SweepBoxMultiByChannel(const FVector BoxExtents, const FVector Start,
	const FVector End, const FRotator Rotation, const TEnumAsByte<ECollisionChannel> Channel,
	const TArray<AActor*>& ActorsToIgnore, TArray<int32>& HitBodyIds)
{
	TArray<FHitResult> Hits;

	const FCollisionShape Shape = FCollisionShape::MakeBox(BoxExtents);
	HitBodyIds = SweepTraceMulti(Shape, Start, End, Rotation.Quaternion(), Channel, ActorsToIgnore, Hits);
	
	return Hits;
}

int32 UBulletPhysicsWorldSubsystem::LineTraceSingle(const FVector& Start, const FVector& End, const TEnumAsByte<ECollisionChannel> Channel, const TArray<AActor*>& ActorsToIgnore, FHitResult& OutHit)
{
	if (!BtWorld) 
	{
		UE_LOG(LogTemp, Warning, TEXT("UBulletSubsystem::RayTest: loaded wihout a bullet world't work"));
		return INDEX_NONE;
	} 

	
	btVector3 FromV = BulletHelpers::ToBulletPosition(Start, FVector(0));
	btVector3 ToV = BulletHelpers::ToBulletPosition(End, FVector(0));
	
	btCollisionObjectArray CollisionArray;
	if (!ActorsToIgnore.IsEmpty())
	{
		for (AActor* IgnoredActor : ActorsToIgnore)
		{
			if (!GlobalShapeDescriptorDataCache.Contains(IgnoredActor)) continue;
			const FUnrealShapeDescriptor& Ref = GlobalShapeDescriptorDataCache[IgnoredActor];
			for (const FUnrealShape& S : Ref.Shapes)
			{
				if (S.BlockingCollider)
				{
					CollisionArray.push_back(S.BlockingCollider);
				}

				if (S.OverlappingCollider)
				{
					CollisionArray.push_back(S.OverlappingCollider);
				}
			}
		}
	}
	
	btClosestNotMeRaycastResultCallback RayCallback(&CollisionArray, FromV, ToV, Channel);

	BtWorld->rayTest
	(
		FromV,
		ToV,
		RayCallback
	);
	
	ConstructHitResult(RayCallback, OutHit);
	
	if (DrawDebugShapes > 0)
	{
		if (OutHit.bBlockingHit)
		{
			DrawDebugLine(GetWorld(), Start, OutHit.Location, FColor::Green, false, 10.f, 0, 1);
			DrawDebugSolidBox(GetWorld(), OutHit.Location, FVector(10.f), FColor::Red, false, 10.f, 1);
		}
		else
		{
			DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 10.f, 0, 1);
		}
	}
	
	return RayCallback.hasHit() ? RayCallback.m_collisionObject->getWorldArrayIndex() : INDEX_NONE;
}

TArray<int32> UBulletPhysicsWorldSubsystem::LineTraceMulti(const FVector& Start, const FVector& End, const TEnumAsByte<ECollisionChannel> Channel, 
	const TArray<AActor*>& ActorsToIgnore, TArray<FHitResult>& OutHits)
{
	TArray<int32> Results;
	
	if (!BtWorld) 
	{
		UE_LOG(LogTemp, Warning, TEXT("UBulletSubsystem::RayTest: loaded wihout a bullet world't work"));
		return Results;
	} 

	
	btVector3 FromV = BulletHelpers::ToBulletPosition(Start, FVector(0));
	btVector3 ToV = BulletHelpers::ToBulletPosition(End, FVector(0));
	
	btCollisionObjectArray CollisionArray;
	if (!ActorsToIgnore.IsEmpty())
	{
		for (AActor* IgnoredActor : ActorsToIgnore)
		{
			if (!GlobalShapeDescriptorDataCache.Contains(IgnoredActor)) continue;
			const FUnrealShapeDescriptor& Ref = GlobalShapeDescriptorDataCache[IgnoredActor];
			for (const FUnrealShape& S : Ref.Shapes)
			{
				if (S.BlockingCollider)
				{
					CollisionArray.push_back(S.BlockingCollider);
				}

				if (S.OverlappingCollider)
				{
					CollisionArray.push_back(S.OverlappingCollider);
				}
			}
		}
	}
	
	btAllNotMeRaycastResultCallback RayCallback(&CollisionArray, FromV, ToV, Channel);

	BtWorld->rayTest
	(
		FromV,
		ToV,
		RayCallback
	);
	
	ConstructHitResult(RayCallback, OutHits);
	
	if (DrawDebugShapes > 0)
	{
		for (const FHitResult& Hit : OutHits)
		{
			if (Hit.bBlockingHit)
			{
				DrawDebugLine(GetWorld(), Start, Hit.Location, FColor::Green, false, 10.f, 0, 1);
				DrawDebugSolidBox(GetWorld(), Hit.Location, FVector(10.f), FColor::Red, false, 10.f, 1);
			}
			else
			{
				DrawDebugLine(GetWorld(), Start, End, FColor::Green, false, 10.f, 0, 1);
			}
		}
	}

	for (int i = 0; i < RayCallback.m_collisionObjects.size(); ++i)
	{
		const btCollisionObject* Hit = RayCallback.m_collisionObjects.at(i);
		if (!Hit) continue;
		Results.Add(Hit->getWorldArrayIndex());
	}
	
	return Results;
	
}

int32 UBulletPhysicsWorldSubsystem::SweepTraceSingle(const FCollisionShape& Shape, const FVector& Start, const FVector& End, 
	const FQuat& Rotation, const TEnumAsByte<ECollisionChannel>& Channel, const TArray<AActor*>& ActorsToIgnore, FHitResult& OutHit)
{
	if (!BtWorld) {
		UE_LOG(LogTemp, Warning, TEXT("UBulletSubsystem::RayTestSingle: loaded without a bullet wouldn't work"));
		return INDEX_NONE;
	} 
	
	const btCollisionShape* CollisionShape = nullptr;

	if (Shape.IsBox())
	{
		CollisionShape = GetBoxCollisionShape(Shape.GetBox());
		
		if (DrawDebugShapes > 0)
		{
			DrawDebugBox(GetWorld(), Start, Shape.GetBox(), Rotation, FColor::Magenta, false, 10.f);
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow);
			DrawDebugBox(GetWorld(), End, Shape.GetBox(), Rotation, FColor::Green, false, 10.f);
		}
	}
	else if (Shape.IsSphere())
	{
		CollisionShape = GetSphereCollisionShape(Shape.GetSphereRadius());
		
		if (DrawDebugShapes > 0)
		{
			DrawDebugSphere(GetWorld(), Start, Shape.GetCapsuleRadius(), 12, FColor::Magenta, false, 10.f);
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow);
			DrawDebugSphere(GetWorld(), End, Shape.GetCapsuleRadius(), 12, FColor::Magenta, false, 10.f);
			
		}
	}
	else if (Shape.IsCapsule())
	{
		CollisionShape = GetCapsuleCollisionShape(Shape.GetCapsuleRadius(), Shape.GetCapsuleHalfHeight());


		if (DrawDebugShapes > 0)
		{
			DrawDebugCapsule(GetWorld(), Start, Shape.GetCapsuleHalfHeight(), Shape.GetCapsuleRadius(), Rotation, FColor::Magenta, false, 10.f);
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow);
			DrawDebugCapsule(GetWorld(), End, Shape.GetCapsuleHalfHeight(), Shape.GetCapsuleRadius(), Rotation, FColor::Green, false, 10.f);
		}
	}
	
	
	FVector FinalEnd = End;
	if (Start.Equals(End))
	{
		FinalEnd.X += SMALL_NUMBER;
		FinalEnd.Y += SMALL_NUMBER;
		FinalEnd.Z += SMALL_NUMBER;
	}
	
	FRotator FinalRot = Rotation.Rotator();
	if (Shape.IsCapsule())
	{
		FinalRot += FRotator(0, 0, -90.f);
	}
	
	btTransform FromTransform = BulletHelpers::ToBulletTransform(FTransform(FinalRot, Start), UE_WORLD_ORIGIN);
	btTransform ToTransform = BulletHelpers::ToBulletTransform(FTransform(FinalRot, FinalEnd), UE_WORLD_ORIGIN);

	btCollisionObjectArray CollisionArray;
	if (!ActorsToIgnore.IsEmpty())
	{
		for (AActor* IgnoredActor : ActorsToIgnore)
		{
			if (!GlobalShapeDescriptorDataCache.Contains(IgnoredActor)) continue;
			const FUnrealShapeDescriptor& Ref = GlobalShapeDescriptorDataCache[IgnoredActor];
			for (const FUnrealShape& S : Ref.Shapes)
			{
				if (S.BlockingCollider)
				{
					CollisionArray.push_back(S.BlockingCollider);
				}

				if (S.OverlappingCollider)
				{
					CollisionArray.push_back(S.OverlappingCollider);
				}
			}
		}
	}
		
	btClosestNotMeConvexResultCallback RayCallback(&CollisionArray, FromTransform.getOrigin(), ToTransform.getOrigin(), Channel);
	
	return SweepTraceInternal(FromTransform, ToTransform, CollisionShape, RayCallback, OutHit);
}

TArray<int32> UBulletPhysicsWorldSubsystem::SweepTraceMulti(const FCollisionShape& Shape, const FVector& Start,
	const FVector& End, const FQuat& Rotation, const TEnumAsByte<ECollisionChannel>& Channel,
	const TArray<AActor*>& ActorsToIgnore, TArray<FHitResult>& OutHits)
{
	TArray<int32> Results;
	if (!BtWorld) {
		UE_LOG(LogTemp, Warning, TEXT("UBulletSubsystem::RayTestSingle: loaded without a bullet wouldn't work"));
		return Results;
	} 
	
	const btCollisionShape* CollisionShape = nullptr;

	if (Shape.IsBox())
	{
		CollisionShape = GetBoxCollisionShape(Shape.GetBox());
		
		if (DrawDebugShapes > 0)
		{
			DrawDebugBox(GetWorld(), Start, Shape.GetBox(), Rotation, FColor::Magenta, false, 10.f);
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow);
			DrawDebugBox(GetWorld(), End, Shape.GetBox(), Rotation, FColor::Green, false, 10.f);
		}
	}
	else if (Shape.IsSphere())
	{
		CollisionShape = GetSphereCollisionShape(Shape.GetSphereRadius());
		
		if (DrawDebugShapes > 0)
		{
			DrawDebugSphere(GetWorld(), Start, Shape.GetCapsuleRadius(), 12, FColor::Magenta, false, 10.f);
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow);
			DrawDebugSphere(GetWorld(), End, Shape.GetCapsuleRadius(), 12, FColor::Magenta, false, 10.f);
			
		}
	}
	else if (Shape.IsCapsule())
	{
		CollisionShape = GetCapsuleCollisionShape(Shape.GetCapsuleRadius(), Shape.GetCapsuleHalfHeight());


		if (DrawDebugShapes > 0)
		{
			DrawDebugCapsule(GetWorld(), Start, Shape.GetCapsuleHalfHeight(), Shape.GetCapsuleRadius(), Rotation, FColor::Magenta, false, 10.f);
			DrawDebugLine(GetWorld(), Start, End, FColor::Yellow);
			DrawDebugCapsule(GetWorld(), End, Shape.GetCapsuleHalfHeight(), Shape.GetCapsuleRadius(), Rotation, FColor::Green, false, 10.f);
		}
	}
	
	
	FVector FinalEnd = End;
	if (Start.Equals(End))
	{
		FinalEnd.X += SMALL_NUMBER;
		FinalEnd.Y += SMALL_NUMBER;
		FinalEnd.Z += SMALL_NUMBER;
	}
	
	FRotator FinalRot = Rotation.Rotator();
	if (Shape.IsCapsule())
	{
		FinalRot += FRotator(0, 0, -90.f);
	}
	
	btTransform FromTransform = BulletHelpers::ToBulletTransform(FTransform(FinalRot, Start), UE_WORLD_ORIGIN);
	btTransform ToTransform = BulletHelpers::ToBulletTransform(FTransform(FinalRot, FinalEnd), UE_WORLD_ORIGIN);

	btCollisionObjectArray CollisionArray;
	if (!ActorsToIgnore.IsEmpty())
	{
		for (AActor* IgnoredActor : ActorsToIgnore)
		{
			if (!GlobalShapeDescriptorDataCache.Contains(IgnoredActor)) continue;
			const FUnrealShapeDescriptor& Ref = GlobalShapeDescriptorDataCache[IgnoredActor];
			for (const FUnrealShape& S : Ref.Shapes)
			{
				if (S.BlockingCollider)
				{
					CollisionArray.push_back(S.BlockingCollider);
				}

				if (S.OverlappingCollider)
				{
					CollisionArray.push_back(S.OverlappingCollider);
				}
			}
		}
	}
		
	btAllNotMeConvexResultCallback RayCallback(&CollisionArray, FromTransform.getOrigin(), ToTransform.getOrigin(), Channel);
	
	return SweepTraceInternal(FromTransform, ToTransform, CollisionShape, RayCallback, OutHits);
}

void UBulletPhysicsWorldSubsystem::ConstructHitResult(const btCollisionWorld::ClosestRayResultCallback& Result, FHitResult& OutHit) const
{
	UPhysicalMaterial* UEMat = nullptr;

	const FVector HitLocation = BulletHelpers::ToUnrealPosition(Result.m_hitPointWorld, UE_WORLD_ORIGIN);
	const FVector ImpactNormal = BulletHelpers::ToUnrealDirection(Result.m_hitNormalWorld);
	const FVector From = BulletHelpers::ToUnrealPosition(Result.m_rayFromWorld, UE_WORLD_ORIGIN);
	
	
	AActor* HitActor = nullptr;
	if (Result.hasHit())
	{
		if (const USceneComponent* SceneComponent = static_cast<USceneComponent*>(Result.m_collisionObject->getUserPointer()))
		{
			HitActor = SceneComponent->GetOwner();
		}
	}
	
	OutHit.bBlockingHit = Result.hasHit();
	OutHit.Location = HitLocation;
	OutHit.ImpactPoint = HitLocation;
	OutHit.ImpactNormal = ImpactNormal;
	OutHit.Normal = ImpactNormal;
	OutHit.Distance = FVector::Distance(HitLocation, From);
	
	if (!HitActor) return;
	
	if (GlobalShapeDescriptorDataCache.Contains(HitActor))
	{
		const FUnrealShapeDescriptor& Data = GlobalShapeDescriptorDataCache[HitActor];
		UPrimitiveComponent* HitComp = Data.FindClosestPrimitive(HitLocation);
		OutHit.Component = HitComp;
		OutHit.HitObjectHandle = FActorInstanceHandle(Data.ShapeOwner.Get());

		if (const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(HitComp))
		{
			if (S->GetMaterials().Num() == 0) return;
			OutHit.PhysMaterial = S->GetMaterial(0)->GetPhysicalMaterial();
		}
	}

}

void UBulletPhysicsWorldSubsystem::ConstructHitResult(const btCollisionWorld::ClosestConvexResultCallback& Result, FHitResult& OutHit) const
{
	UPhysicalMaterial* UEMat = nullptr;

	const FVector HitLocation = BulletHelpers::ToUnrealPosition(Result.m_hitPointWorld, UE_WORLD_ORIGIN);
	const FVector ImpactNormal = BulletHelpers::ToUnrealDirection(Result.m_hitNormalWorld);
	const FVector From = BulletHelpers::ToUnrealPosition(Result.m_convexFromWorld, UE_WORLD_ORIGIN);
	
	AActor* HitActor = nullptr;
	if (Result.hasHit())
	{
		if (const USceneComponent* SceneComponent = static_cast<USceneComponent*>(Result.m_hitCollisionObject->getUserPointer()))
		{
			HitActor = SceneComponent->GetOwner();
		}
	}
	
	OutHit.bBlockingHit = Result.hasHit();
	OutHit.Location = HitLocation;
	OutHit.ImpactPoint = HitLocation;
	OutHit.ImpactNormal = ImpactNormal;
	OutHit.Normal = ImpactNormal;
	OutHit.Distance = FVector::Distance(HitLocation, From);
	
	if (!HitActor) return;
	if (GlobalShapeDescriptorDataCache.Contains(HitActor))
	{
		const FUnrealShapeDescriptor& Data = GlobalShapeDescriptorDataCache[HitActor];
		UPrimitiveComponent* HitComp = Data.FindClosestPrimitive(HitLocation);
		OutHit.Component = HitComp;
		OutHit.HitObjectHandle = FActorInstanceHandle(Data.ShapeOwner.Get());

		if (const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(HitComp))
		{
			if (S->GetMaterials().Num() == 0) return;
			OutHit.PhysMaterial = S->GetMaterial(0)->GetPhysicalMaterial();
		}
	}
}

void UBulletPhysicsWorldSubsystem::ConstructHitResult(const btAllNotMeRaycastResultCallback& Result, TArray<FHitResult>& OutHits) const
{

	const FVector From = BulletHelpers::ToUnrealPosition(Result.m_rayFromWorld, UE_WORLD_ORIGIN);
	for (int i = 0; i < Result.m_collisionObjects.size(); ++i)
	{
		FHitResult OutHit;
		
		const btCollisionObject* HitObject = Result.m_collisionObjects.at(i);
		const btVector3& HitPoint = Result.m_hitPointWorld.at(i);
		const btVector3& HitNormal = Result.m_hitNormalWorld.at(i);
		
		UPhysicalMaterial* UEMat = nullptr;

		const FVector HitLocation = BulletHelpers::ToUnrealPosition(HitPoint, UE_WORLD_ORIGIN);
		const FVector ImpactNormal = BulletHelpers::ToUnrealDirection(HitNormal);
	
	
		AActor* HitActor = nullptr;
		if (Result.hasHit())
		{
			if (const USceneComponent* SceneComponent = static_cast<USceneComponent*>(HitObject->getUserPointer()))
			{
				HitActor = SceneComponent->GetOwner();
			}
		}
	
		OutHit.bBlockingHit = Result.hasHit();
		OutHit.Location = HitLocation;
		OutHit.ImpactPoint = HitLocation;
		OutHit.ImpactNormal = ImpactNormal;
		OutHit.Normal = ImpactNormal;
		OutHit.Distance = FVector::Distance(HitLocation, From);
		
	
		if (!HitActor) return;
	
		if (GlobalShapeDescriptorDataCache.Contains(HitActor))
		{
			const FUnrealShapeDescriptor& Data = GlobalShapeDescriptorDataCache[HitActor];
			UPrimitiveComponent* HitComp = Data.FindClosestPrimitive(HitLocation);
			OutHit.Component = HitComp;
			OutHit.HitObjectHandle = FActorInstanceHandle(Data.ShapeOwner.Get());

			if (const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(HitComp))
			{
				if (S->GetMaterials().Num() == 0) return;
				OutHit.PhysMaterial = S->GetMaterial(0)->GetPhysicalMaterial();
			}
		}
		
		OutHits.Add(OutHit);
	}
}

void UBulletPhysicsWorldSubsystem::ConstructHitResult(const btAllNotMeConvexResultCallback& Result,
	TArray<FHitResult>& OutHits) const
{
	const FVector From = BulletHelpers::ToUnrealPosition(Result.m_rayFromWorld, UE_WORLD_ORIGIN);
	for (int i = 0; i < Result.m_collisionObjects.size(); ++i)
	{
		FHitResult OutHit;
		
		const btCollisionObject* HitObject = Result.m_collisionObjects.at(i);
		const btVector3& HitPoint = Result.m_hitPointWorld.at(i);
		const btVector3& HitNormal = Result.m_hitNormalWorld.at(i);
		
		UPhysicalMaterial* UEMat = nullptr;

		const FVector HitLocation = BulletHelpers::ToUnrealPosition(HitPoint, UE_WORLD_ORIGIN);
		const FVector ImpactNormal = BulletHelpers::ToUnrealDirection(HitNormal);
	
	
		AActor* HitActor = nullptr;
		if (Result.hasHit())
		{
			if (const USceneComponent* SceneComponent = static_cast<USceneComponent*>(HitObject->getUserPointer()))
			{
				HitActor = SceneComponent->GetOwner();
			}
		}
	
		OutHit.bBlockingHit = Result.hasHit();
		OutHit.Location = HitLocation;
		OutHit.ImpactPoint = HitLocation;
		OutHit.ImpactNormal = ImpactNormal;
		OutHit.Normal = ImpactNormal;
		OutHit.Distance = FVector::Distance(HitLocation, From);
		
	
		if (!HitActor) return;
	
		if (GlobalShapeDescriptorDataCache.Contains(HitActor))
		{
			const FUnrealShapeDescriptor& Data = GlobalShapeDescriptorDataCache[HitActor];
			UPrimitiveComponent* HitComp = Data.FindClosestPrimitive(HitLocation);
			OutHit.Component = HitComp;
			OutHit.HitObjectHandle = FActorInstanceHandle(Data.ShapeOwner.Get());

			if (const UStaticMeshComponent* S = Cast<UStaticMeshComponent>(HitComp))
			{
				if (S->GetMaterials().Num() == 0) return;
				OutHit.PhysMaterial = S->GetMaterial(0)->GetPhysicalMaterial();
			}
		}
		
		OutHits.Add(OutHit);
	}
}

int32 UBulletPhysicsWorldSubsystem::SweepTraceInternal(const btTransform& From, const btTransform& To, 
	const btCollisionShape* Collider, btClosestNotMeConvexResultCallback& Result, FHitResult& OutHit) const
{
	BtWorld->convexSweepTest
	(
		static_cast<const btConvexShape*>(Collider),
		From,
		To,
		Result
	);
	
	ConstructHitResult(Result, OutHit);
	
	return Result.hasHit() ? Result.m_hitCollisionObject->getWorldArrayIndex() : INDEX_NONE;
}

TArray<int32> UBulletPhysicsWorldSubsystem::SweepTraceInternal(const btTransform& From, const btTransform& To, const btCollisionShape* Collider, btAllNotMeConvexResultCallback& Result, TArray<FHitResult>& OutHits) const
{
	BtWorld->convexSweepTest
	(
		static_cast<const btConvexShape*>(Collider),
		From,
		To,
		Result
	);
	
	ConstructHitResult(Result, OutHits);
	
	TArray<int32> HitIds;
	for (int i = 0; i < Result.m_collisionObjects.size(); ++i)
	{
		const btCollisionObject* Hit = Result.m_collisionObjects.at(i);
		if (!Hit) continue;
		HitIds.Add(Hit->getWorldArrayIndex());
	}
	
	return HitIds;
}


btCollisionObject* UBulletPhysicsWorldSubsystem::GetStaticObject(const int ID) const
{
	return BtWorld->getCollisionObjectArray().at(ID);
}


void UBulletPhysicsWorldSubsystem::ExtractPhysicsGeometry(const AActor* Actor, PhysicsGeometryCallback CB, FUnrealShapeDescriptor& ShapeDescriptor)
{
	TInlineComponentArray<UPrimitiveComponent*, 20> Components;
	// Used to easily get a component's transform relative to actor, not parent component
	const FTransform InvActorTransform = Actor->GetActorTransform();

	// Collisions from meshes
	Actor->GetComponents(UPrimitiveComponent::StaticClass(), Components);
	for (UPrimitiveComponent* Comp : Components)
	{
		IBulletPrimitiveComponentInterface* I = Cast<IBulletPrimitiveComponentInterface>(Comp);
		if (!I) continue;

		const FBulletShapeOptions& ShapeOptions = I->GetShapeOptions();
		if (!ShapeOptions.bGenerateCollisionEventsInBullet && !ShapeOptions.bGenerateOverlapEventsInBullet)
		{
			continue;
		}
		
		const bool bIsRootComponent = Actor->GetRootComponent() == Comp;
		ShapeDescriptor.Add(Comp, bIsRootComponent);
		
		if (Cast<UStaticMeshComponent>(Comp))
		{
			ExtractPhysicsGeometry(Cast<UStaticMeshComponent>(Comp), InvActorTransform, CB, ShapeDescriptor);
		}
		else if (Cast<UShapeComponent>(Comp))
		{
			ExtractPhysicsGeometry(Cast<UShapeComponent>(Comp), InvActorTransform, CB, ShapeDescriptor);
		}
		else if (Cast<USkeletalMeshComponent>(Comp))
		{
			// Extract Shapes From Physics Asset.
		}
	}
}


void UBulletPhysicsWorldSubsystem::ExtractComplexPhysicsGeometry(const FTransform& XformSoFar, UStaticMeshComponent* Mesh,
	PhysicsGeometryCallback Callback, FUnrealShapeDescriptor& ShapeDescriptor)
{
	IBulletPrimitiveComponentInterface* I = Cast<IBulletPrimitiveComponentInterface>(Mesh);
	if (!I) return;	
	btBvhTriangleMeshShape* TriMesh = GetComplexShape(XformSoFar, Mesh->GetStaticMesh());
	Callback(TriMesh, XformSoFar, I->GetShapeOptions());
}

void UBulletPhysicsWorldSubsystem::ExtractPhysicsGeometry(UStaticMeshComponent* SMC, const FTransform& InvActorXform, PhysicsGeometryCallback CB, FUnrealShapeDescriptor& ShapeDescriptor)
{
	if (!SMC) return;
	IBulletPrimitiveComponentInterface* I = Cast<IBulletPrimitiveComponentInterface>(SMC);
	if (!I) return;	
	UStaticMesh* Mesh = SMC->GetStaticMesh();
	if (!Mesh) return;

	const FTransform CompTransform = SMC->GetComponentTransform();
	switch (Mesh->GetBodySetup()->CollisionTraceFlag)
	{

	case ECollisionTraceFlag::CTF_UseComplexAsSimple:
		{
			if (SMC->Mobility != EComponentMobility::Type::Movable)
			{
				// complex geo should not move.
				ExtractComplexPhysicsGeometry(CompTransform, SMC, CB, ShapeDescriptor);
			}
			else
			{
				ExtractPhysicsGeometry(SMC, CompTransform, Mesh->GetBodySetup(), CB, ShapeDescriptor);
			}
			break;
		}
	case ECollisionTraceFlag::CTF_UseDefault:
		{
			ExtractPhysicsGeometry(SMC, CompTransform, Mesh->GetBodySetup(), CB, ShapeDescriptor);
			break;
		}
	default:
		
		break;
	}

}


void UBulletPhysicsWorldSubsystem::ExtractPhysicsGeometry(UShapeComponent* Sc, const FTransform& InvActorXform, PhysicsGeometryCallback CB, FUnrealShapeDescriptor& ShapeDescriptor)
{
	// We want the complete transform from actor to this component, not just relative to parent
	FTransform CompFullRelXForm = Sc->GetComponentTransform();
	ExtractPhysicsGeometry(Sc, CompFullRelXForm, Sc->ShapeBodySetup, CB, ShapeDescriptor);
}


void UBulletPhysicsWorldSubsystem::ExtractPhysicsGeometry(UPrimitiveComponent* PrimitiveComponent, const FTransform& XformSoFar, UBodySetup* BodySetup, PhysicsGeometryCallback CB, FUnrealShapeDescriptor& ShapeDescriptor)
{
	if (!ensure(BodySetup != nullptr))
	{
		return;
	}
	
	IBulletPrimitiveComponentInterface* I = Cast<IBulletPrimitiveComponentInterface>(PrimitiveComponent);
	if (!I) return;	
	
	FVector Scale = XformSoFar.GetScale3D();
	btCollisionShape* Shape = nullptr;
	btCompoundShape* CompoundShape = nullptr;
	
	//  if the total makes up more than 1, we have a compound shape configured in USkeletalMeshComponent
	if (BodySetup->AggGeom.BoxElems.Num() + BodySetup->AggGeom.SphereElems.Num() + BodySetup->AggGeom.SphylElems.Num() > 1)
	{	
		const int TotalShapes = BodySetup->AggGeom.BoxElems.Num() + BodySetup->AggGeom.SphereElems.Num() + BodySetup->AggGeom.SphylElems.Num();
		CompoundShape = new btCompoundShape(true, TotalShapes);
	}
	
	

	// Iterate over the simple collision shapes
	for (auto&& Box : BodySetup->AggGeom.BoxElems)
	{
		FVector Dimensions = FVector(Box.X, Box.Y, Box.Z) * Scale;
		// We'll re-use based on just the LxWxH, including actor scale
		// Rotation and center will be baked in world space
		UE_LOG(LogTemp, Warning, TEXT("UBulletPhysicsWorldSubsystem:: creating box"));
        Shape = GetBoxCollisionShape(Dimensions);
		FTransform ShapeXform(Box.Rotation, Box.Center);
		// Shape transform adds to any relative transform already here
		FTransform XForm = ShapeXform * XformSoFar;
		if (CompoundShape)
		{
			CompoundShape->addChildShape(BulletHelpers::ToBulletTransform(XForm, UE_WORLD_ORIGIN), Shape);
			continue;
		}
		
		
		CB(Shape, XForm, I->GetShapeOptions());
	}
	for (auto&& Sphere : BodySetup->AggGeom.SphereElems)
	{
		// Only support uniform scale so use X
		Shape = GetSphereCollisionShape(Sphere.Radius * Scale.X);
		FTransform ShapeXform(FRotator::ZeroRotator, Sphere.Center);
		// Shape transform adds to any relative transform already here
		FTransform XForm = ShapeXform * XformSoFar;
		if (CompoundShape)
		{
			CompoundShape->addChildShape(BulletHelpers::ToBulletTransform(XForm, UE_WORLD_ORIGIN), Shape);
			continue;
		}
		CB(Shape, XForm, I->GetShapeOptions());
	}
	// Sphyl == Capsule (??)
	for (auto&& Capsule : BodySetup->AggGeom.SphylElems)
	{
		// X scales radius, Z scales height
		Shape = GetCapsuleCollisionShape(Capsule.Radius * Scale.X, Capsule.Length * Scale.Z);
		// Capsules are in Z in UE, in Y in Bullet, so roll -90
		FRotator Rot(0, 0, 0);
		// Also apply any local rotation
		Rot += Capsule.Rotation;
		FTransform ShapeXform(Rot, Capsule.Center);
		// Shape transform adds to any relative transform already here
		FTransform XForm = ShapeXform * XformSoFar;
		
		if (CompoundShape)
		{
			CompoundShape->addChildShape(BulletHelpers::ToBulletTransform(XForm, UE_WORLD_ORIGIN), Shape);
			continue;
		}
		
		CB(Shape, XForm, I->GetShapeOptions());
	}
	for (uint16 i = 0; const FKConvexElem& ConVexElem : BodySetup->AggGeom.ConvexElems)
	{
		Shape = GetConvexHullCollisionShape(BodySetup, i, Scale);
		i++;
		if (CompoundShape)
		{
			FTransform XForm(ConVexElem.GetTransform());
			CompoundShape->addChildShape(BulletHelpers::ToBulletTransform(XForm, UE_WORLD_ORIGIN), Shape);
			continue;
		}
		
		CB(Shape, XformSoFar, I->GetShapeOptions());
	}

	if (CompoundShape)
	{
		Shape = CompoundShape;
		CB(Shape, XformSoFar, I->GetShapeOptions());
		delete CompoundShape;
	}

}




