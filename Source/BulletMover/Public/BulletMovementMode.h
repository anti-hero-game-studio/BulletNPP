// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BulletMoverSimulationTypes.h"
#include "BulletMoverTypes.h"
#include "MoveLibrary/BulletMoverBlackboard.h"
#include "BulletMovementModeTransition.h"
#include "MoveLibrary/BulletFloorQueryUtils.h"
#include "UObject/Interface.h"
#include "Templates/SubclassOf.h"
#include "BulletMovementMode.generated.h"

#define UE_API BULLETMOVER_API


/**
 * UBulletMovementSettingsInterface: interface that must be implemented for any settings object to be shared between modes
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UBulletMovementSettingsInterface : public UInterface
{
	GENERATED_BODY()
};

class IBulletMovementSettingsInterface
{
	GENERATED_BODY()

public:
	virtual FString GetDisplayName() const = 0;
};

UENUM(BlueprintType)
enum class EBulletMoverFrictionOverrideMode : uint8
{
	DoNotOverride,
	AlwaysOverrideToZero,
	OverrideToZeroWhenMoving,
};


/**
 * Base class for all movement modes, exposing simulation update methods for both C++ and blueprint extension
 */
UCLASS(MinimalAPI, Abstract, Within = BulletMoverComponent, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class UBulletBaseMovementMode : public UObject
{
	GENERATED_BODY()

public:
	UE_API virtual UWorld* GetWorld() const override;
	
	UE_API virtual void OnRegistered(const FName ModeName);
	UE_API virtual void OnUnregistered();
	
	// These functions are called immediately when the state machine switches modes
	UE_API virtual void Activate();
	UE_API virtual void Deactivate();

	// These functions are called when the sync state is changed on the game thread
	// and a new mode is activated/deactivated
	UE_API virtual void Activate_External();
	UE_API virtual void Deactivate_External();
	
	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "Generate Move", ForceAsFunction))
	UE_API void GenerateMove(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, UPARAM(ref) FBulletProposedMove& OutProposedMove) const;

	UFUNCTION(BlueprintNativeEvent, meta = (DisplayName = "Simulation Tick", ForceAsFunction))
	UE_API void SimulationTick(const FBulletSimulationTickParams& Params, UPARAM(ref) FBulletMoverTickEndData& OutputState);
	
	/** Gets the MoverComponent that owns this movement mode */
	UFUNCTION(BlueprintCallable, Category=Mover, meta=(DisplayName="Get Mover Component", ScriptName = GetMoverComponent))
	UE_API UBulletMoverComponent* K2_GetMoverComponent() const;

	/**
	 * Gets the outer mover component of the indicated type. Does not check on the type or the presence of the MoverComp outer. Safe to call on CDOs.
	 * Note: Since UBulletBaseMovementMode is declared "Within = MoverComponent", all instances of a mode except the CDO are guaranteed to have a valid MoverComponent outer.
	 */
	template<typename MoverT = UBulletMoverComponent UE_REQUIRES(std::is_base_of_v<MoverT, UBulletMoverComponent>)>
	MoverT* GetMoverComponent() const
	{
		return Cast<MoverT>(GetOuter());
	}

	/**
	 * Gets the outer mover component of the indicated type, checked for validity.
	 * Note: Since UBulletBaseMovementMode is declared "Within = MoverComponent", all instances of a mode except the CDO are guaranteed to have a valid MoverComponent outer.
	 */
	template<typename MoverT = UBulletMoverComponent UE_REQUIRES(std::is_base_of_v<MoverT, UBulletMoverComponent>)>
	MoverT& GetMoverComponentChecked() const
	{
		return *CastChecked<MoverT>(GetOuterUBulletMoverComponent());
	}

	/**
   	 * Check Movement Mode for a gameplay tag.
   	 *
   	 * @param TagToFind			Tag to check on the Mover systems
   	 * @param bExactMatch		If true, the tag has to be exactly present, if false then TagToFind will include it's parent tags while matching
   	 * 
   	 * @return True if the TagToFind was found
   	 */
	UE_API virtual bool HasGameplayTag(FGameplayTag TagToFind, bool bExactMatch) const;

#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	/** Settings object type that this mode depends on. May be shared with other movement modes. When the mode is added to a Mover Component, it will create a shared instance of this settings class. */
	UPROPERTY(EditDefaultsOnly, Category = Mover, meta = (MustImplement = "/Script/BulletMover.MovementSettingsInterface"))
	TArray<TSubclassOf<UObject>> SharedSettingsClasses;

	/** Transition checks for the current mode. Evaluated in order, stopping at the first successful transition check */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = Mover, meta = (FullyExpand = true))
	TArray<TObjectPtr<UBulletBaseMovementModeTransition>> Transitions;

	/** A list of gameplay tags associated with this movement mode */
	UPROPERTY(EditDefaultsOnly, Category = Mover)
	FGameplayTagContainer GameplayTags;

	/** 
	 * Whether this movement mode supports being part of an asynchronous movement simulation (running concurrently with the gameplay thread) 
	 * Specifically for the GenerateMove and SimulationTick functions
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Mover)
	bool bSupportsAsync = false;

protected:
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Activated", ScriptName = "OnActivated"))
	UE_API void K2_OnActivated();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Deactivated", ScriptName = "OnDeactivated"))
	UE_API void K2_OnDeactivated();
	
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Registered", ScriptName = "OnRegistered"))
	UE_API void K2_OnRegistered(const FName ModeName);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "On Unregistered", ScriptName = "OnUnregistered"))
	UE_API void K2_OnUnregistered();
	
	
	void FloorCheck(const FVector& StartingLocation, const FVector& ProposedLinearVelocity, const float& DeltaTime, FBulletFloorCheckResult& Result) const;
	
	
};

/**
 * NullMovementMode: a default do-nothing mode used as a placeholder when no other mode is active
 */
 UCLASS(MinimalAPI, NotBlueprintable)
class UBulletNullMovementMode : public UBulletBaseMovementMode
{
	GENERATED_UCLASS_BODY()

public:
	UE_API virtual void SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState) override;

	UE_API const static FName NullModeName;
};

#undef UE_API
