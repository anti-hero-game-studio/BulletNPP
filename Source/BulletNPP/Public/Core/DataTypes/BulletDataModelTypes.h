// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletPhysicsTypes.h"
#include "UObject/Object.h"
#include "BulletDataModelTypes.generated.h"

#define UE_API BULLETNPP_API



// Used to identify how to interpret a movement input vector's values
UENUM(BlueprintType)
enum class EBulletMoveInputType : uint8
{
	Invalid,

	/** Move with intent, as a per-axis magnitude [-1,1] (E.g., "move straight forward as fast as possible" would be (1, 0, 0) and "move straight left at half speed" would be (0, -0.5, 0) regardless of frame time). Zero vector indicates intent to stop. */
	DirectionalIntent,

	/** Move with a given velocity (units per second) */
	Velocity,

	/** No move input of any type */
	None,
};


// Data block containing all inputs that need to be authored and consumed for the default Bullet character simulation
USTRUCT(BlueprintType)
struct FBulletDefaultInputs : public FBulletDataStructBase
{
	GENERATED_BODY()

	// Sets the directional move inputs for a simulation frame
	UE_API void SetMoveInput(EBulletMoveInputType InMoveInputType, const FVector& InMoveInput);

	const FVector& GetMoveInput() const { return MoveInput; }
	EBulletMoveInputType GetMoveInputType() const { return MoveInputType; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Bullet)
	EBulletMoveInputType MoveInputType;

	/**
	 * Representing the directional move input for this frame. Must be interpreted according to MoveInputType. Relative to MovementBase if set, world space otherwise. Will be truncated to match network serialization precision.
	 * Note: Use SetDirectionalInput or SetVelocityInput to set MoveInput and MoveInputType
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Bullet)
	FVector MoveInput;

public:
	// Facing direction intent, as a normalized forward-facing direction. A zero vector indicates no intent to change facing direction. Relative to MovementBase if set, world space otherwise.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Bullet)
	FVector OrientationIntent;

	// World space orientation that the controls were based on. This is commonly a player's camera rotation.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Bullet)
	FRotator ControlRotation;

	// Used to force the Bullet actor into a different movement mode
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Bullet)
	FName SuggestedMovementMode;

	// Specifies whether we are using a movement base, which will affect how move inputs are interpreted
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Bullet)
	bool bUsingMovementBase;

	// Optional: when moving on a base, input may be relative to this object
	UPROPERTY(BlueprintReadWrite, Category = Bullet)
	TObjectPtr<UPrimitiveComponent> MovementBase;

	// Optional: for movement bases that are skeletal meshes, this is the bone we're based on. Only valid if MovementBase is set.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Bullet)
	FName MovementBaseBoneName;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Bullet)
	bool bIsJumpJustPressed;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Bullet)
	bool bIsJumpPressed;

	UE_API FVector GetMoveInput_WorldSpace() const;
	UE_API FVector GetOrientationIntentDir_WorldSpace() const;


	FBulletDefaultInputs()
		: MoveInputType(EBulletMoveInputType::None)
		, MoveInput(ForceInitToZero)
		, OrientationIntent(ForceInitToZero)
		, ControlRotation(ForceInitToZero)
		, SuggestedMovementMode(NAME_None)
		, bUsingMovementBase(false)
		, MovementBase(nullptr)
		, MovementBaseBoneName(NAME_None)
		, bIsJumpJustPressed(false)
		, bIsJumpPressed(false)
	{
	}

	virtual ~FBulletDefaultInputs() {}

	bool operator==(const FBulletDefaultInputs& Other) const
	{
		return MoveInputType == Other.MoveInputType
			&& MoveInput == Other.MoveInput
			&& OrientationIntent == Other.OrientationIntent
			&& ControlRotation == Other.ControlRotation
			&& SuggestedMovementMode == Other.SuggestedMovementMode
			&& bUsingMovementBase == Other.bUsingMovementBase
			&& MovementBase == Other.MovementBase
			&& MovementBaseBoneName == Other.MovementBaseBoneName
			&& bIsJumpJustPressed == Other.bIsJumpJustPressed
			&& bIsJumpPressed == Other.bIsJumpPressed;
	}
	
	bool operator!=(const FBulletDefaultInputs& Other) const { return !operator==(Other); }
	
	// @return newly allocated copy of this FBulletDefaultInputs. Must be overridden by child classes
	UE_API virtual FBulletDataStructBase* Clone() const override;
	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	UE_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
	UE_API virtual bool ShouldReconcile(const FBulletDataStructBase& AuthorityState) const override;
	UE_API virtual void Interpolate(const FBulletDataStructBase& From, const FBulletDataStructBase& To, float Pct) override;
	UE_API virtual void Merge(const FBulletDataStructBase& From) override;
	UE_API virtual void Decay(float DecayAmount) override;
};

template<>
struct TStructOpsTypeTraits< FBulletDefaultInputs > : public TStructOpsTypeTraitsBase2< FBulletDefaultInputs >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};




// Data block containing basic sync state information
USTRUCT(BlueprintType)
struct FBulletDefaultSyncState : public FBulletDataStructBase
{
	GENERATED_BODY()
protected:
	// Position relative to MovementBase if set, world space otherwise
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Bullet)
	FVector Location;

	// Forward-facing rotation relative to MovementBase if set, world space otherwise.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Bullet)
	FRotator Orientation;

	// Linear velocity, units per second, relative to MovementBase if set, world space otherwise.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Bullet)
	FVector Velocity;

	// Angular velocity, degrees per second, relative to MovementBase if set, world space otherwise.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Bullet)
	FVector AngularVelocityDegrees;

public:
	// Movement intent direction relative to MovementBase if set, world space otherwise. Magnitude of range (0-1)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Bullet)
	FVector MoveDirectionIntent;

protected:
	// Optional: when moving on a base, input may be relative to this object
	UPROPERTY(BlueprintReadOnly, Category = Bullet)
	TWeakObjectPtr<UPrimitiveComponent> MovementBase;

	// Optional: for movement bases that are skeletal meshes, this is the bone we're based on. Only valid if MovementBase is set.
	UPROPERTY(BlueprintReadOnly, Category = Bullet)
	FName MovementBaseBoneName;

	UPROPERTY(BlueprintReadOnly, Category = Bullet)
	FVector MovementBasePos;

	UPROPERTY(BlueprintReadOnly, Category = Bullet)
	FQuat MovementBaseQuat;

public:

	FBulletDefaultSyncState()
		: Location(ForceInitToZero)
		, Orientation(ForceInitToZero)
		, Velocity(ForceInitToZero)
		, AngularVelocityDegrees(ForceInitToZero)
		, MoveDirectionIntent(ForceInitToZero)
		, MovementBase(nullptr)
		, MovementBaseBoneName(NAME_None)
		, MovementBasePos(ForceInitToZero)
		, MovementBaseQuat(FQuat::Identity)
	{
	}

	virtual ~FBulletDefaultSyncState() {}

	// @return newly allocated copy of this FBulletDefaultSyncState. Must be overridden by child classes
	UE_API virtual FBulletDataStructBase* Clone() const override;

	UE_API virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	UE_API virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	UE_API virtual bool ShouldReconcile(const FBulletDataStructBase& AuthorityState) const override;

	UE_API virtual void Interpolate(const FBulletDataStructBase& From, const FBulletDataStructBase& To, float Pct) override;

	UE_DEPRECATED(5.7, "Use the SetTransforms_WorldSpace with an angular velocity")
	UE_API void SetTransforms_WorldSpace(FVector WorldLocation, FRotator WorldOrient, FVector WorldVelocity, UPrimitiveComponent* Base=nullptr, FName BaseBone = NAME_None);
	
	UE_API void SetTransforms_WorldSpace(FVector WorldLocation, FRotator WorldOrient, FVector WorldVelocity, FVector WorldAngularVelocityDegrees, UPrimitiveComponent* Base=nullptr, FName BaseBone = NAME_None);

	// Returns whether the base setting succeeded
	UE_API bool SetMovementBase(UPrimitiveComponent* Base, FName BaseBone=NAME_None);

	// Refreshes captured movement base transform based on its current state, while maintaining the same base-relative transforms
	UE_API bool UpdateCurrentMovementBase();

	// Queries
	bool IsNearlyEqual(const FBulletDefaultSyncState& Other) const;

	UPrimitiveComponent* GetMovementBase() const { return MovementBase.Get(); }
	FName GetMovementBaseBoneName() const { return MovementBaseBoneName; }
	FVector GetCapturedMovementBasePos() const { return MovementBasePos; }
	FQuat GetCapturedMovementBaseQuat() const { return MovementBaseQuat; }

	UE_API FVector GetLocation_WorldSpace() const;
	UE_API FVector GetLocation_BaseSpace() const;	// If there is no movement base set, these will be the same as world space

	UE_API FVector GetIntent_WorldSpace() const;
	UE_API FVector GetIntent_BaseSpace() const;

	UE_API FVector GetVelocity_WorldSpace() const;
	UE_API FVector GetVelocity_BaseSpace() const;

	UE_API FRotator GetOrientation_WorldSpace() const;
	UE_API FRotator GetOrientation_BaseSpace() const;

	UE_API FTransform GetTransform_WorldSpace() const;
	UE_API FTransform GetTransform_BaseSpace() const;

	UE_API FVector GetAngularVelocityDegrees_WorldSpace() const;
	UE_API FVector GetAngularVelocityDegrees_BaseSpace() const;
};

template<>
struct TStructOpsTypeTraits< FBulletDefaultSyncState > : public TStructOpsTypeTraitsBase2< FBulletDefaultSyncState >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};


/**
 * Blueprint function library to make it easier to work with Bullet data structs, since we can't add UFUNCTIONs to structs
 */
UCLASS(MinimalAPI)
class UBulletDataModelBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:	// FBulletDefaultInputs

	/** Sets move input from a unit length vector representing directional intent */
	UFUNCTION(BlueprintCallable, Category = Bullet)
	static UE_API void SetDirectionalInput(UPARAM(Ref) FBulletDefaultInputs& Inputs, const FVector& DirectionInput);
	
	/** Sets move input from a vector representing desired velocity c/m^2 */
	UFUNCTION(BlueprintCallable, Category = Bullet)
	static UE_API void SetVelocityInput(UPARAM(Ref) FBulletDefaultInputs& Inputs, const FVector& VelocityInput = FVector::ZeroVector);

	/** Returns the move direction intent, if any, in world space */
	UFUNCTION(BlueprintCallable, Category = Bullet)
	static UE_API FVector GetMoveDirectionIntentFromInputs(const FBulletDefaultInputs& Inputs);


public:	// FBulletDefaultSyncState

	/** Returns the location in world space */
	UFUNCTION(BlueprintCallable, Category = Bullet)
	static UE_API FVector GetLocationFromSyncState(const FBulletDefaultSyncState& SyncState);

	/** Returns the move direction intent, if any, in world space */
	UFUNCTION(BlueprintCallable, Category = Bullet)
	static UE_API FVector GetMoveDirectionIntentFromSyncState(const FBulletDefaultSyncState& SyncState);

	/** Returns the velocity in world space */
	UFUNCTION(BlueprintCallable, Category = Bullet)
	static UE_API FVector GetVelocityFromSyncState(const FBulletDefaultSyncState& SyncState);

	/** Returns the angular velocity in world space, in degrees per second */
	UFUNCTION(BlueprintCallable, Category = Bullet)
	static UE_API FVector GetAngularVelocityDegreesFromSyncState(const FBulletDefaultSyncState& SyncState);

	/** Returns the orientation in world space */
	UFUNCTION(BlueprintCallable, Category = Bullet)
	static UE_API FRotator GetOrientationFromSyncState(const FBulletDefaultSyncState& SyncState);
};

#undef UE_API
