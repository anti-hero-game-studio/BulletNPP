// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BulletPhysicsCharacterMovementMode.h"
#include "Core/DataTypes/BulletTypes.h"
#include "DefaultMovementSet/Modes/BulletKinematicWalkingMode.h"
#include "BulletPhysicsWalkingMode.generated.h"

#define UE_API BULLETMOVER_API

class UBulletCommonLegacyMovementSettings;
struct FBulletFloorCheckResult;
struct FBulletRelativeBaseInfo;
struct FBulletMovementRecord;

/**
 * 
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, Experimental)
class UBulletPhysicsWalkingMode : public UBulletPhysicsCharacterMovementMode
{
		GENERATED_BODY()

public:
	UE_API UBulletPhysicsWalkingMode(const FObjectInitializer& ObjectInitializer);
	
	UE_API virtual void GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const override;

	UE_API virtual void SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState) override;

	// Returns the active turn generator. Note: you will need to cast the return value to the generator you expect to get, it can also be none
	UFUNCTION(BlueprintPure, Category=Mover)
	UE_API UObject* GetTurnGenerator();

	// Sets the active turn generator to use the class provided. Note: To set it back to the default implementation pass in none
	UFUNCTION(BlueprintCallable, Category=Mover)
	UE_API void SetTurnGeneratorClass(UPARAM(meta=(MustImplement="/Script/BulletMover.TurnGeneratorInterface", AllowAbstract="false")) TSubclassOf<UObject> TurnGeneratorClass);

protected:

	/** Choice of behavior for floor checks while not moving.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	EBulletStaticFloorCheckPolicy FloorCheckPolicy = EBulletStaticFloorCheckPolicy::OnDynamicBaseOnly;

	/** Optional modular object for generating rotation towards desired orientation. If not specified, linear interpolation will be used. */
	UPROPERTY(EditAnywhere, Instanced, Category=Mover, meta=(ObjectMustImplement="/Script/BulletMover.TurnGeneratorInterface"))
	TObjectPtr<UObject> TurnGenerator;

	UE_API virtual void OnRegistered(const FName ModeName) override; 
	UE_API virtual void OnUnregistered() override;

	UE_API void CaptureFinalState(const FVector FinalLocation, const FRotator FinalRotation, bool bDidAttemptMovement, const FBulletFloorCheckResult& FloorResult, const FBulletMovementRecord& Record, const FVector& AngularVelocityDegrees, FBulletUpdatedMotionState& OutputSyncState) const;

	UE_API FBulletRelativeBaseInfo UpdateFloorAndBaseInfo(const FBulletFloorCheckResult& FloorResult) const;

	TWeakObjectPtr<const UBulletCommonLegacyMovementSettings> CommonLegacySettings;
};

#undef UE_API