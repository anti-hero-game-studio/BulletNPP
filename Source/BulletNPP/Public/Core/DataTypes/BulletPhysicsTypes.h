// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletPhysicsTypes.generated.h"

#define UE_API BULLETNPP_API

/** List of Bullet tick phases where different parts of work are being performed */
UENUM(BlueprintType)
enum class EBulletTickPhase : uint8
{
	Invalid = 0             UMETA(Hidden),

	/** This tick is where an input for the next movement step is authored */
	ProduceInput = 1        UMETA(DisplayName = "ProduceInput"),

	/** This tick is where movement based on {input, state} is simulated, to produce a new state */
	SimulateMovement = 2    UMETA(DisplayName = "SimulateMovement"),

	/** This tick is where the newest simulation state is applied to the actor and its components */
	ApplyState = 3          UMETA(DisplayName = "ApplyState")
};

/** List of tick dependency order of execution relative to the Bullet tick function. */
UENUM(BlueprintType)
enum class EBulletTickDependencyOrder : uint8
{
	Before = 0,
	After = 1,
};

/** Options for how to handle smoothing frame data from the backend. Typically this is for advancing the simulation at a lower or fixed rate versus the game thread/render rate. */
UENUM(BlueprintType)
enum class EBulletSmoothingMode : uint8
{
	/** Smoothed frames will be ignored */
	None,

	/** Use the smoothed state data to offset the visual root component only, without smoothing the root moving component or any other state data */
	VisualComponentOffset,
};


// Struct to hold params for when an impact happens. This contains all of the data for impacts including what gets passed to the FBullet_OnImpact delegate
USTRUCT(BlueprintType, meta = (DisplayName = "Impact Data"))
struct FBulletOnImpactParams
{
	GENERATED_BODY()
	
	UE_API FBulletOnImpactParams();
	
	UE_API FBulletOnImpactParams(const FName& ModeName, const FHitResult& Hit, const FVector& Delta);
	
	// Name of the movement mode this actor is currently in at the time of the impact
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet)
	FName MovementModeName;
	
	// The hit result of the impact
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet)
	FHitResult HitResult;

	// The original move that was being performed when the impact happened
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet)
	FVector AttemptedMoveDelta;
};


USTRUCT(BlueprintType)
struct FBulletTimeStep
{
	GENERATED_BODY()
	

	// The server simulation frame this timestep is associated with. This is the frame that's being worked on now.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Bullet)
	int32 ServerFrame=-1;

	// Starting simulation time (in server simulation timespace)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Bullet)
	double BaseSimTimeMs=-1.0;

	// The delta time step for this tick
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Bullet)
	float StepMs=1.0;

	// Indicates whether this time step is re-simulating based on prior inputs, such as during a correction
	uint8 bIsResimulating : 1 = 0;

};



// Base type for all data structs used to compose Bullet simulation model definition dynamically (input cmd, sync state, aux state)
// NOTE: for simulation state data (sync/aux), derive from FBulletStateData instead 
USTRUCT(BlueprintType)
struct FBulletDataStructBase
{
	GENERATED_BODY()

	UE_API FBulletDataStructBase();
	virtual ~FBulletDataStructBase() {}

	/** return newly allocated copy of this FBulletDataStructBase. MUST be overridden by derived types. */
	UE_API virtual FBulletDataStructBase* Clone() const;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;
		return true;
	}

	/** Gets the type info of this FBulletDataStructBase. MUST be overridden by derived types. */
	UE_API virtual UScriptStruct* GetScriptStruct() const;

	/** Get string representation of this struct instance */
	virtual void ToString(FAnsiStringBuilderBase& Out) const {}

	/** If derived classes hold any object references, override this function and add them to the collector. */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}

	/**
	 * Checks if the contained data is equal, within reason.
	 * AuthorityState is guaranteed to be the same concrete type as 'this'.
	 * Override for: All types that compose STATE data (sync or aux) and types that compose INPUT data for physics-based movement
	 */
	UE_API virtual bool ShouldReconcile(const FBulletDataStructBase& AuthorityState) const;

	/**
	 * Interpolates contained data between a starting and ending block.
	 * From and To are guaranteed to be the same concrete type as 'this'.
	 * Override for: All types that compose STATE data (sync or aux) and types that compose INPUT data for physics-based movement  
	 */
	UE_API virtual void Interpolate(const FBulletDataStructBase& From, const FBulletDataStructBase& To, float Pct);

	/**
	 * Merges contained data from a previous frame with that of the current frame. For more information, see the comments on FNetworkPhysicsData::MergeData.
	 * From is guaranteed to be the same concrete type as 'this'.
	 * Override for: Types that compose INPUT data for physics-based movement
	 */
	UE_API virtual void Merge(const FBulletDataStructBase& From);
	
	/**
	 * Decays contained data during resimulation if data is forward predicted. For more information, see the comments on FNetworkPhysicsData::DecayData.
	 * @param DecayAmount = Total amount of decay as a multiplier. 10% decay = 0.1.
	 * Override for: Types that compose INPUT data for physics-based movement
	 */
	virtual void Decay(float DecayAmount) {};


	/** Used to differentiate between this type, and a proxy data type. RARELY overridden by derived types. */
	UE_API virtual const UScriptStruct* GetDataScriptStruct() const;
};

template<>
struct TStructOpsTypeTraits< FBulletDataStructBase > : public TStructOpsTypeTraitsBase2< FBulletDataStructBase >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};




// Contains a group of different FBulletDataStructBase-derived data, and supports net serialization of them. Note that
//	each contained data must have a unique type.  This is to support dynamic composition of Bullet simulation model
//  definitions (input cmd, sync state, aux state).
USTRUCT(BlueprintType)
struct FBulletDataCollection
{
	GENERATED_BODY()

	UE_API FBulletDataCollection();

	void Empty() { DataArray.Empty(); }

	/** Serialize all data in this collection */
	UE_API bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	/** Serializes data in this collection for debug purposes.
	*   This is currently only usable in the context of sending Bullet info to the Chaos Visual Debugger
	*/
	UE_API bool SerializeDebugData(FArchive& Ar);

	/** Copy operator - deep copy so it can be used for archiving/saving off data */
	UE_API FBulletDataCollection& operator=(const FBulletDataCollection& Other);

	/** Comparison operator (deep) - needs matching struct types along with identical states in those structs. See also ShouldReconcile */
	UE_API bool operator==(const FBulletDataCollection& Other) const;

	/** Comparison operator */
	UE_API bool operator!=(const FBulletDataCollection& Other) const;

	/** Checks if the collections are significantly different enough (piece-wise) to need reconciliation. NOT an equality check. */
	UE_API bool ShouldReconcile(const FBulletDataCollection& Other) const;

	/** Make this collection a piece-wise interpolation between 2 collections */
	UE_API void Interpolate(const FBulletDataCollection& From, const FBulletDataCollection& To, float Pct);

	/** Merge a previous frame's collection with this collection */
	UE_API void Merge(const FBulletDataCollection& From);

	/** Decay input based on DecayAmount for resimulation and forward prediction */
	UE_API void Decay(float DecayAmount);

	/** Checks only whether there are matching structs inside, but NOT necessarily identical states of each one */
	UE_API bool HasSameContents(const FBulletDataCollection& Other) const;

	/** Exposes references to GC system */
	UE_API void AddStructReferencedObjects(FReferenceCollector& Collector) const;

	/** Get string representation of all elements in this collection */
	UE_API void ToString(FAnsiStringBuilderBase& Out) const;

	/** Const access to data array of collections */
	UE_API TArray<TSharedPtr<FBulletDataStructBase>>::TConstIterator GetCollectionDataIterator() const;
	
	/** Find data of a specific type in the collection (mutable version). If not found, null will be returned. */
	template <typename T>
	T* FindMutableDataByType() const
	{
		if (FBulletDataStructBase* FoundData = FindDataByType(T::StaticStruct()))
		{
			return static_cast<T*>(FoundData);
		}

		return nullptr;
	}

	/** Find data of a specific type in the collection. If not found, null will be returned. */
	template <typename T>
	const T* FindDataByType() const
	{
		if (const FBulletDataStructBase* FoundData = FindDataByType(T::StaticStruct()))
		{
			return static_cast<const T*>(FoundData);
		}

		return nullptr;
	}

	/** Find data of a specific type in the collection. If not found, a new default instance will be added. */
	template <typename T>
	const T& FindOrAddDataByType()
	{
		if (const T* ExistingData = FindDataByType<T>())
		{
			return *ExistingData;
		}

		return *(static_cast<const T*>(AddDataByType(T::StaticStruct())));
	}

	/** Find data of a specific type in the collection (mutable version). If not found, a new default instance will be added. */
	template <typename T>
	T& FindOrAddMutableDataByType()
	{
		if (T* ExistingData = FindMutableDataByType<T>())
		{
			return *ExistingData;
		}

		return *(static_cast<T*>(AddDataByType(T::StaticStruct())));
	}

	/** Find data of a specific type in the collection (mutable version). If not found, a new default instance will be added. Outputs if the state was added or not. */
	template <typename T>
	T& FindOrAddMutableDataByType(bool& bOutAdded)
	{
		if (T* ExistingData = FindMutableDataByType<T>())
		{
			bOutAdded = false;
			return *ExistingData;
		}

		bOutAdded = true;
		return *(static_cast<T*>(AddDataByType(T::StaticStruct())));
	}

	/** Adds this data instance to the collection, taking ownership of it. If an existing data struct of the same type is already there, it will be removed first. */
	UE_API void AddOrOverwriteData(const TSharedPtr<FBulletDataStructBase> DataInstance);

	/** Adds data to the collection by copying over an existing struct or cloning the provided struct if no matching struct exists. 
	 *  This is different than AddOrOverwriteData because the instance isn't touched, avoiding memory allocation & array changing if a matching struct exists.
	 */
	UE_API void AddDataByCopy(const FBulletDataStructBase* DataInstanceToCopy);


	const TArray< TSharedPtr<FBulletDataStructBase> >& GetDataArray() const
	{
		return DataArray;
	}

	/** Find data of a specific type in the collection. */
	UE_API FBulletDataStructBase* FindDataByType(const UScriptStruct* DataStructType) const;

	/** Find data of a specific type in the collection. If not found, a new default instance will be added. */
	UE_API FBulletDataStructBase* FindOrAddDataByType(const UScriptStruct* DataStructType);

	/** Removes data of a specific type in the collection. Returns true if data was removed. */
	UE_API bool RemoveDataByType(const UScriptStruct* DataStructType);

protected:
	UE_API FBulletDataStructBase* AddDataByType(const UScriptStruct* DataStructType);
	static UE_API TSharedPtr<FBulletDataStructBase> CreateDataByType(const UScriptStruct* DataStructType);
	
	/** Helper function for serializing array of data */
	static UE_API void NetSerializeDataArray(FArchive& Ar, UPackageMap* Map, TArray<TSharedPtr<FBulletDataStructBase>>& DataArray);

	/** All data in this collection */
	TArray< TSharedPtr<FBulletDataStructBase> > DataArray;


friend class UBulletDataCollectionLibrary;
friend class UBulletComponent;
};


template<>
struct TStructOpsTypeTraits<FBulletDataCollection> : public TStructOpsTypeTraitsBase2<FBulletDataCollection>
{
	enum
	{
		WithCopy = true,		// Necessary so that DataArray is copied around
		WithNetSerializer = true,
		WithIdenticalViaEquality = true,
		WithAddStructReferencedObjects = true,
	};
};

/** Info about data collection types that should always be present, and how they should propagate from one frame to the next */
USTRUCT(BlueprintType, meta = (DisplayName = "Persistent Data Settings"))
struct FBulletDataPersistence
{
	GENERATED_BODY()

	FBulletDataPersistence() {}

	FBulletDataPersistence(UScriptStruct* TypeToPersist, bool bShouldCopyBetweenFrames) 
	{
		RequiredType = TypeToPersist;
		bCopyFromPriorFrame = bShouldCopyBetweenFrames;
	}

	// The type that should propagate between frames
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet, meta=(MetaStruct="/Script/Bullet.BulletDataStructBase"))
	TObjectPtr<UScriptStruct> RequiredType = nullptr;

	// If true, values will be copied from the prior frame. Otherwise, they will be default-initialized.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Bullet)
	bool bCopyFromPriorFrame = true;
};

// Information about a change in movement mode
struct FMovementModeChangeRecord
{
	FName ModeName;
	FName PrevModeName;
	uint32 Frame;
	double SimTimeMs;
};



// Blueprint helper functions for working with a Bullet data collection
UCLASS(MinimalAPI)
class UBulletDataCollectionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	* Add a data struct to the collection, overwriting an existing one with the same type
	* @param SourceAsRawBytes		The data struct instance to add by copy, which must be a FBulletDataStructBase sub-type
	*/
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Bullet|Data", meta = (CustomStructureParam = "SourceAsRawBytes", AllowAbstract = "false", DisplayName = "Add Data To Collection"))
	static UE_API void K2_AddDataToCollection(UPARAM(Ref) FBulletDataCollection& Collection, UPARAM(DisplayName="Struct To Add") const int32& SourceAsRawBytes);

	/**
	 * Retrieves data from a collection, by writing to a target instance if it contains one of the matching type.  Changes must be written back using AddDataToCollection.
	 * @param DidSucceed			Flag indicating whether data was actually written to target struct instance
	 * @param TargetAsRawBytes		The data struct instance to write to, which must be a FBulletDataStructBase sub-type
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "Bullet|Data", meta = (CustomStructureParam = "TargetAsRawBytes", AllowAbstract = "false", DisplayName = "Get Data From Collection"))
	static UE_API void K2_GetDataFromCollection(bool& DidSucceed, UPARAM(Ref) const FBulletDataCollection& Collection, UPARAM(DisplayName = "Out Struct") int32& TargetAsRawBytes);

	/**
	* Clears all data from a collection
	*/
	UFUNCTION(BlueprintCallable, Category = "Bullet|Data", meta=(DisplayName = "Clear Data From Collection"))
	static UE_API void ClearDataFromCollection(UPARAM(Ref) FBulletDataCollection& Collection);

private:
	DECLARE_FUNCTION(execK2_AddDataToCollection);
	DECLARE_FUNCTION(execK2_GetDataFromCollection);
};

#undef UE_API