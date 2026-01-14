// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletMovementMode.h"
#include "BulletKinematicFallingMode.h"
#include "BulletAsyncFallingMode.generated.h"

#define UE_API BULLETMOVER_API

class UBulletCommonLegacyMovementSettings;
struct FBulletFloorCheckResult;

/**
 * AsyncFallingMode: a default movement mode for moving through the air and jumping, typically influenced by gravity and air control
 * This mode simulates movement without actually modifying any scene component(s).
 */
UCLASS(MinimalAPI, Blueprintable, BlueprintType, Experimental)
class UBulletAsyncFallingMode : public UBulletBaseMovementMode
{
	GENERATED_BODY()


public:
	UE_API UBulletAsyncFallingMode(const FObjectInitializer& ObjectInitializer);

	UE_API virtual void OnRegistered(const FName ModeName) override;
    UE_API virtual void OnUnregistered() override;
	UE_API virtual void GenerateMove_Implementation(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, FBulletProposedMove& OutProposedMove) const override;
	UE_API virtual void SimulationTick_Implementation(const FBulletSimulationTickParams& Params, FBulletMoverTickEndData& OutputState) override;

	// Broadcast when this actor lands on a valid surface.
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FBulletMover_OnLanded OnLanded;
	
	/**
	 * If true, actor will land and lose all speed in the vertical direction upon landing. If false, actor's vertical speed will be redirected based on the surface normal it hit.
	 * Note: Actor's horizontal speed will not be affected if true. If false, horizontal speed may be increased on landing.
	 */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite)
	bool bCancelVerticalSpeedOnLanding;
	
	/**
	 * When falling, amount of movement control available to the actor.
	 * 0 = no control, 1 = full control
	 */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1.0"))
	float AirControlPercentage;
	
	/**
 	 * Deceleration to apply to air movement when falling slower than terminal velocity.
 	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
 	 */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s^2"))
	float FallingDeceleration;

	/**
     * Deceleration to apply to air movement when falling faster than terminal velocity
	 * Note: This is NOT applied to vertical velocity, only movement plane velocity
     */
    UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s^2"))
    float OverTerminalSpeedFallingDeceleration;
	
	/**
	 * If the actor's movement plane velocity is greater than this speed falling will start applying OverTerminalSpeedFallingDeceleration instead of FallingDeceleration
	 * The expected behavior is to set OverTerminalSpeedFallingDeceleration higher than FallingDeceleration so the actor will slow down faster
	 * when going over TerminalMovementPlaneSpeed.
	 */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s"))
	float TerminalMovementPlaneSpeed;

	/** When exceeding maximum vertical speed, should it be enforced via a hard clamp? If false, VerticalFallingDeceleration will be used for a smoother transition to the terminal speed limit. */
	UPROPERTY(Category = Mover, EditAnywhere, BlueprintReadWrite)
	bool bShouldClampTerminalVerticalSpeed;

	/** Deceleration to apply to vertical velocity when it's greater than TerminalVerticalSpeed. Only used if bShouldClampTerminalVerticalSpeed is false. */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(EditCondition="!bShouldClampTerminalVerticalSpeed", ClampMin="0", ForceUnits = "cm/s^2"))
	float VerticalFallingDeceleration;
	
	/**
	 * If the actors vertical velocity is greater than this speed VerticalFallingDeceleration will be applied to vertical velocity
	 */
	UPROPERTY(Category=Mover, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ForceUnits = "cm/s"))
	float TerminalVerticalSpeed;
	
protected:
	/**
	 * Is called at the end of the tick in falling mode. Handles checking any landings that should occur and switching to specific modes
	 * (i.e. landing on a walkable surface would switch to the walking movement mode) 
	 */
	UFUNCTION(BlueprintCallable, Category=Mover)
	UE_API virtual void ProcessLanded(const FBulletFloorCheckResult& FloorResult, FVector& Velocity, FBulletRelativeBaseInfo& BaseInfo, FBulletMoverTickEndData& TickEndData) const;

	UE_API void CaptureFinalState(const FBulletUpdatedMotionState* StartSyncState, const FVector FinalLocation, const FRotator FinalRotation, const FBulletFloorCheckResult& FloorResult, float DeltaSeconds, float DeltaSecondsUsed, const FVector& AngularVelocityDegrees, FBulletUpdatedMotionState& OutputSyncState, FBulletMoverTickEndData& TickEndData, FBulletMovementRecord& Record) const;

	TWeakObjectPtr<const UBulletCommonLegacyMovementSettings> CommonLegacySettings;
};

#undef UE_API
