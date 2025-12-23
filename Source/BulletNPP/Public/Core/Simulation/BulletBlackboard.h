// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletBlackboard.generated.h"

#define UE_API BULLETNPP_API

namespace BulletPhysicsEngine
{
	enum class EInvalidationReason : uint8
	{
		FullReset
		, // All blackboard objects should be invalidated
		Rollback
		, // Invalidate any rollback-sensitive objects
	};
}


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType)
class UBulletBlackboard : public UObject
{
	GENERATED_BODY()

private:
	class BlackboardObject
	{
		// untyped base
		struct ObjectContainerBase
		{
		};

		// typed container
		template<typename T>
		struct ObjectContainer : ObjectContainerBase
		{
			ObjectContainer(const T& t) : Object(t) {}

			const T& Get() { return Object; }
			T& GetMutable() { return Object; }

		private:
			T Object;
		};


	public:
		template<typename T>
		BlackboardObject(const T& obj) : ContainerPtr(MakeShared<ObjectContainer<T>>(obj)) {}

		template<typename T>
		const T& Get() const
		{
			ObjectContainer<T>* TypedContainer = static_cast<ObjectContainer<T>*>(ContainerPtr.Get());
			return TypedContainer->Get();
		}

		template<typename T>
		T& GetMutable() const
		{
			ObjectContainer<T>* TypedContainer = static_cast<ObjectContainer<T>*>(ContainerPtr.Get());
			return TypedContainer->GetMutable();
		}

	private:
		TSharedPtr<ObjectContainerBase> ContainerPtr;
	};	// end BlackboardObject
 
public:

	/** Attempt to retrieve an object from the blackboard. If found, OutFoundValue will be set. Returns true/false to indicate whether it was found. */
 	template<typename T>
	bool TryGet(FName ObjName, T& OutFoundValue) const
	{
		UE::TReadScopeLock Lock(ObjectsMapLock);

		if (const TUniquePtr<BlackboardObject>* ExistingObject = ObjectsByName.Find(ObjName))
		{
			OutFoundValue = ExistingObject->Get()->Get<T>();
			return true;
		}

		return false;
	}

	// TODO: make GetOrAdds. One that takes a value, and one that takes a lambda that can generate the new value.
	/*
	template<typename T>
	bool GetOrAdd(FName ObjName, some kinda lambda that returns T, T& OutFoundValue)
	{
		if (const TUniquePtr<BlackboardObject>* ExistingObject = ObjectsByName.Find(ObjName))
		{
			OutFoundValue = ExistingObject->Get();
			return true;
		}

		RunTheExec lambda and store it by the key, then return that value
		return false;
	}
	template<typename T>
	T GetOrAdd(FName ObjName, T ObjToAdd)
	{
		return ObjectsByName.FindOrAdd(ObjName, ObjToAdd);
	}
	*/

	/** Returns true/false to indicate if an object is stored with that name */
	bool Contains(FName ObjName) const
	{
		UE::TReadScopeLock Lock(ObjectsMapLock);
		return ObjectsByName.Contains(ObjName);
	}

	/** Store object by a named key, overwriting any existing object */
	template<typename T>
	void Set(FName ObjName, T Obj)
	{
		UE::TWriteScopeLock Lock(ObjectsMapLock);
		ObjectsByName.Emplace(ObjName, MakeUnique<BlackboardObject>(Obj));
	}

	/** Invalidate an object by name */
	UE_API void Invalidate(FName ObjName);

	/** Invalidate all objects that can be affected by a particular circumstance (such as a rollback) */
	UE_API void Invalidate(BulletPhysicsEngine::EInvalidationReason Reason);
	
	/** Invalidate all objects */
	void InvalidateAll() { Invalidate(BulletPhysicsEngine::EInvalidationReason::FullReset); }

	// UObject interface
	UE_API virtual void BeginDestroy() override;
	// End UObject interface

private:
	mutable FTransactionallySafeRWLock ObjectsMapLock;		// used internally when reading/writing to ObjectsByName map
	TMap<FName, TUniquePtr<BlackboardObject>> ObjectsByName;
};
