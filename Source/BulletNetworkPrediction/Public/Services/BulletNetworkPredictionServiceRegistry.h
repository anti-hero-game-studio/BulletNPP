// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "BulletNetworkPredictionService_Ticking.inl"
#include "BulletNetworkPredictionService_Rollback.inl"
#include "BulletNetworkPredictionService_Interpolate.inl"
#include "BulletNetworkPredictionService_Input.inl"
#include "BulletNetworkPredictionService_Finalize.inl"
#include "BulletNetworkPredictionService_ServerRPC.inl"
#include "BulletNetworkPredictionService_Smooth.inl"

// Services do the actual system work on batches of registered instances. UBulletNetworkPredictionWorldManager orchestrates them.
//
// Services should treat all registered instances the same in most cases. Instead of 1 services that has different behavior based 
// on Role/NetConnection/etc, make N services to cover each unique case. There will be exceptions to this where a role branch on a minor 
// aspect of the service is better than creating a brand new service.
//
// Services are defined by an interface (E.g, IBulletFixedRollbackService) and a ModelDef based template implementation (E.g, TBulletFixedRollbackService).
//
// Services operate on the data stored in TBulletModelDataStore, which is owned by the FBulletNetworkPredictionServiceRegistry. 
// All services get a pointer to the data store when created.
// Services are free to keep internal acceleration structures but should mainly operate on the per-instance data in the data store.
// 
// FBulletNetworkPredictionServiceRegistry maintains lists of all active services. Services are bound to the EBulletNetworkPredictionService enum for identification.
// The template implementations are instantiated on demand during registration where ModelDef::ID is the index into the TServiceStorage array.
//
// The big picture looks like this, where 1=templated class has been instantiated and is managing X registered instances:
//
//                              [ModelDef::ID]
//	  [ServiceType]			0  1  2  3  4  5  ... N
//	FixedRollback           1  0  1  1  1  0
//	FixedExtrapolated       0  1  0  0  1  0
//	...
//	IndependentFinalize     0  0  0  0  0  1
//
//
// NPs instance are registered to X services based on their config and network role/connection. Subscribed services are stored in TInstanceData<ModelDef>::ServiceMask.
// As runtime settings change, E.g, NetworkLOD, the set of subscribed services will change. This is done efficiently with the ServiceMask.
//
// Services are an implementation detail of the UBulletNetworkPredictionWorldManager and should not be exposed directly to outside runtime code. 
// E.g, don't pass pointers to services back to user code.
//
// Services can be specialized on ModelDef types. This could facilitate more efficient batch processing or further per-simulation/modeldef customization.
// Services should have RegisterInstance/UnregisterInstance functions that take only the FBulletNetworkPredictionID as parameter.
//
// No direct cross-service communication. Services can have their own internal services (E.g, rollback has an internal tick service) but the "top level" services do not communicate. 
// UBulletNetworkPredictionWorldManager should coordinate things.
//
//	
// Adding new services:
//	1. Add entry to EBulletNetworkPredictionService
//	2. Add JNP_DECLARE_SERVICE 
//	3. Add JNP_DEFINE_SERVICE_CALL
//	4. Add logic to UBulletNetworkPredictionWorldManager::ConfigureInstance to determine the conditions for subscribing to the service
//	5. Add logic in UBulletNetworkPredictionWorldManager to actually invoke the service. This will obviously be service dependent.
//
// New services types are not meant to be arbitrarily added by projects. Adding a brand new service requires modification of UBulletNetworkPredictionWorldManager.
// If you want to tack on 'something special', look at specializing an existing service (E.g the tick service could do 'extra stuff' per tick for example).
//
// Future Improvements:
//	-All services template classes are currently instantiated for all ModelDefs (the templated code is generated for each ModelDef)
//	-Even if ModelDefs are incompatible for a given service.
//	-To be clear: it does not instantiate an instance of TBulletLocalInputService at runtime, but the code is generated and almost certainly cannot be culled by the compiler.
//	-Concepts could be used to determine if ModelDefs are compatible with Services and we could avoid the template code instantiation.
//	-This would add more template machinery, and basically requires all TService<ModelDefs> be hidden behind SFINAE walls. E.g, Factories for instantiating and CallOrNot for Register/Unregister.
//	-Its not clear if its worth the effort at this point. In practice there should be relatively few ModelDefs that would benefit from this. 
//	-But it could make a difference in some cases for build time and exe size

class FBulletNetworkPredictionServiceRegistry
{
public:

	// -----------------------------------------------------------------------------------------
	//	Register / Unregister
	// -----------------------------------------------------------------------------------------

	// Registers instance with given services. Will unregister instance with any previously-subscribed services.
	template<typename ModelDef>
	void RegisterInstance(FBulletNetworkPredictionID ID, TInstanceData<ModelDef>& InstanceData, EBulletNetworkPredictionService ServiceMask)
	{
		// Expected to register for fixed XOR independent services
		jnpEnsureSlow(EnumHasAnyFlags(ServiceMask, EBulletNetworkPredictionService::ANY_FIXED) ^ EnumHasAnyFlags(ServiceMask, EBulletNetworkPredictionService::ANY_INDEPENDENT));
		if (InstanceData.ServiceMask != EBulletNetworkPredictionService::None)
		{
			// Only unregister/register what is changing
			const EBulletNetworkPredictionService UnregisterMask = InstanceData.ServiceMask & ~(ServiceMask);
			const EBulletNetworkPredictionService RegisterMask = ServiceMask & ~(InstanceData.ServiceMask);

			UnregisterInstance_Internal<ModelDef>(ID, UnregisterMask);

			ForEachService<ModelDef>(RegisterMask, [ID](auto Ptr)
			{
				Ptr->RegisterInstance(ID);
			});
		}
		else
		{
			// Register with everything
			ForEachService<ModelDef>(ServiceMask, [ID](auto Ptr)
			{
				Ptr->RegisterInstance(ID);
			});
		}

		InstanceData.ServiceMask = ServiceMask;
	}

	template<typename ModelDef>
	void UnregisterInstance(FBulletNetworkPredictionID ID)
	{

		TBulletModelDataStore<ModelDef>* DataStore = GetDataStore<ModelDef>();
		if (TInstanceData<ModelDef>* FoundInstanceData = DataStore->Instances.Find(ID))
		{
			UnregisterInstance_Internal<ModelDef>(ID, FoundInstanceData->ServiceMask);
		}
    
		/// New Cleanup
		DataStore->Instances.Remove(ID);
		DataStore->Frames.Remove(ID);
		DataStore->ClientRecv.Remove(ID);
		DataStore->ServerRecv.Remove(ID);
		DataStore->ServerRecv_IndependentTick.Remove(ID);
	}

	// -----------------------------------------------------------------------------------------
	//	DataStore
	// -----------------------------------------------------------------------------------------

	template<typename ModelDef=FBulletNetworkPredictionModelDef>
	TBulletModelDataStore<ModelDef>* GetDataStore()
	{
		jnpEnsureMsgf(ModelDef::ID > 0, TEXT("ModelDef %s has invalid ID assigned. Could be missing JNP_MODEL_REGISTER."), ModelDef::GetName());

		struct FThisDataStore : IDataStore
		{
			TBulletModelDataStore<ModelDef> Self;
		};

		if (DataStoreArray.IsValidIndex(ModelDef::ID) == false)
		{
			DataStoreArray.SetNum(ModelDef::ID+1);
		}

		TUniquePtr<IDataStore>& Item = DataStoreArray[ModelDef::ID];
		if (Item.IsValid() == false)
		{
			Item = MakeUnique<FThisDataStore>();
		}

		return &((FThisDataStore*)Item.Get())->Self;
	};

	// -----------------------------------------------------------------------------------------
	//	Services
	// -----------------------------------------------------------------------------------------

	template<typename InServiceInterface, int32 NumInlineServices=5>
	struct TServiceStorage
	{
		using ServiceInterface = InServiceInterface;
		TSparseArray<TUniquePtr<ServiceInterface>, TInlineSparseArrayAllocator<NumInlineServices>> Array;
	};

	// Macros are mainly to enforce consistent naming and cohesion with EBulletNetworkPredictionService
#define JNP_DECLARE_SERVICE(EnumName, ServiceInterface) TServiceStorage<ServiceInterface> EnumName
#define JNP_DEFINE_SERVICE_CALL(EnumName, ServiceType) ConditionalCallFuncOnService<EBulletNetworkPredictionService::EnumName, ServiceType<ModelDef>>(EnumName, Func, Mask)
	
	// Declares generic storage for the service type: TServiceStorage<InterfaceType>
	
	
	JNP_DECLARE_SERVICE(FixedServerRPC,			    IBulletFixedServerRPCService);
	JNP_DECLARE_SERVICE(FixedRollback,				IBulletFixedRollbackService);
	JNP_DECLARE_SERVICE(FixedInterpolate,			IBulletFixedInterpolateService);
	JNP_DECLARE_SERVICE(FixedInputLocal,				IBulletInputService);
	JNP_DECLARE_SERVICE(FixedInputRemote,			IBulletInputService);
	JNP_DECLARE_SERVICE(FixedTick,					IBulletLocalTickService);
	JNP_DECLARE_SERVICE(FixedFinalize,				IBulletFinalizeService);
	JNP_DECLARE_SERVICE(FixedSmoothing,				IBulletFixedSmoothingService);

	JNP_DECLARE_SERVICE(ServerRPC,			        IBulletServerRPCService);
	JNP_DECLARE_SERVICE(IndependentRollback,			IBulletIndependentRollbackService);
	JNP_DECLARE_SERVICE(IndependentInterpolate,		IBulletIndependentInterpolateService);
	JNP_DECLARE_SERVICE(IndependentLocalInput,		IBulletInputService);
	JNP_DECLARE_SERVICE(IndependentLocalTick,		IBulletLocalTickService);
	JNP_DECLARE_SERVICE(IndependentRemoteTick,		IBulletRemoteIndependentTickService);
	JNP_DECLARE_SERVICE(IndependentLocalFinalize,	IBulletFinalizeService);
	JNP_DECLARE_SERVICE(IndependentRemoteFinalize,	IBulletRemoteFinalizeService);
	
private:

	template<typename ModelDef=FBulletNetworkPredictionModelDef, typename FuncRefType>
	void ForEachService(EBulletNetworkPredictionService Mask, const FuncRefType& Func)
	{
		// Call to ConditionalCallFuncOnService
		

		if (EnumHasAnyFlags(Mask, EBulletNetworkPredictionService::ANY_FIXED))
		{
			JNP_DEFINE_SERVICE_CALL(FixedServerRPC,			        TBulletFixedServerRPCService);
			JNP_DEFINE_SERVICE_CALL(FixedRollback,				TBulletFixedRollbackService);
			JNP_DEFINE_SERVICE_CALL(FixedInterpolate,			TBulletFixedInterpolateService);
			JNP_DEFINE_SERVICE_CALL(FixedInputLocal,				TBulletLocalInputService);
			JNP_DEFINE_SERVICE_CALL(FixedInputRemote,			TBulletRemoteInputService);
			JNP_DEFINE_SERVICE_CALL(FixedTick,					TBulletLocalTickService);
			JNP_DEFINE_SERVICE_CALL(FixedFinalize,				TBulletFinalizeService);
			JNP_DEFINE_SERVICE_CALL(FixedSmoothing,				TBulletFixedSmoothingService);
		}
		else if (EnumHasAnyFlags(Mask, EBulletNetworkPredictionService::ANY_INDEPENDENT))
		{

			JNP_DEFINE_SERVICE_CALL(ServerRPC,			        TBulletServerRPCService);
			JNP_DEFINE_SERVICE_CALL(IndependentRollback,			TBulletIndependentRollbackService);
			JNP_DEFINE_SERVICE_CALL(IndependentInterpolate,		TBulletIndependentInterpolateService);
			JNP_DEFINE_SERVICE_CALL(IndependentLocalInput,		TBulletLocalInputService);
			JNP_DEFINE_SERVICE_CALL(IndependentLocalTick,		TBulletLocalTickService);
			JNP_DEFINE_SERVICE_CALL(IndependentRemoteTick,		TBulletRemoteIndependentTickService);
			JNP_DEFINE_SERVICE_CALL(IndependentLocalFinalize,	TBulletFinalizeService);
			JNP_DEFINE_SERVICE_CALL(IndependentRemoteFinalize,	TBulletRemoteFinalizeService);
		}
	}

	template<EBulletNetworkPredictionService ServiceMask, typename ServiceType, typename FuncRefType, typename ServiceStorageType>
	void ConditionalCallFuncOnService(ServiceStorageType& ServiceStorage, const FuncRefType& Func, EBulletNetworkPredictionService Mask)
	{
		using ModelDef = typename ServiceType::ModelDef;
		if (EnumHasAllFlags(Mask, ServiceMask))
		{
			// Resize array for this ModelDef if necessary
			if (ServiceStorage.Array.IsValidIndex(ModelDef::ID) == false)
			{
				FSparseArrayAllocationInfo AllocationInfo = ServiceStorage.Array.InsertUninitialized(ModelDef::ID);
				new (AllocationInfo.Pointer) TUniquePtr<typename ServiceStorageType::ServiceInterface>();
			}

			// Allocate instance on the UniquePtr if necessary
			auto& Ptr = ServiceStorage.Array[ModelDef::ID];
			if (Ptr.IsValid() == false)
			{
				Ptr = MakeUnique<ServiceType>(GetDataStore<ModelDef>());
			}

			Func((ServiceType*)Ptr.Get());
		}
	}

	template<typename ServiceType, typename ArrayType>
	ServiceType* GetService_Internal(ArrayType& Array)
	{
		using ModelDef = typename ServiceType::ModelDef;
		if (Array.IsValidIndex(ModelDef::ID) == false)
		{
			FSparseArrayAllocationInfo AllocationInfo = Array.InsertUninitialized(ModelDef::ID);
			new (AllocationInfo.Pointer) TUniquePtr<ServiceType>(new ServiceType(GetDataStore<ModelDef>()));
		}

		auto& Item = Array[ModelDef::ID];
		jnpCheckf(Item.IsValid(), TEXT("Service not initialized"));
		return (ServiceType*)Item.Get();
	}

	template<typename ModelDef>
	void UnregisterInstance_Internal(FBulletNetworkPredictionID ID, EBulletNetworkPredictionService ServiceMask)
	{
		ForEachService<ModelDef>(ServiceMask, [ID](auto Ptr)
		{
			Ptr->UnregisterInstance(ID);
		});
	}

	struct IDataStore
	{
		virtual ~IDataStore() = default;
	};

	TArray<TUniquePtr<IDataStore>>	DataStoreArray;
};
