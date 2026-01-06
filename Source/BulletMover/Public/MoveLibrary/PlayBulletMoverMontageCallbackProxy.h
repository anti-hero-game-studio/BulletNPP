// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Animation/AnimInstance.h"
#include "PlayMontageCallbackProxy.h"
#include "PlayBulletMoverMontageCallbackProxy.generated.h"

#define UE_API BULLETMOVER_API

class UBulletMoverComponent;
class UAnimMontage;
class USkeletalMeshComponent;

// Runtime object used as a proxy to an async blueprint task node that runs animation montages on Mover actors. This leverages 
// parent UPlayMontageCallbackProxy to perform animation, while adding an accompanying layered move to handle any root motion 
// from the montage.
UCLASS(MinimalAPI)
class UPlayBulletMoverMontageCallbackProxy : public UPlayMontageCallbackProxy
{
	GENERATED_UCLASS_BODY()

	// Called to perform the query internally
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UE_API UPlayBulletMoverMontageCallbackProxy* CreateProxyObjectForPlayMoverMontage(
		UBulletMoverComponent* InMoverComponent,
		UAnimMontage* MontageToPlay,
		float PlayRate = 1.f,
		float StartingPosition = 0.f,
		FName StartingSection = NAME_None);

public:
	//~ Begin UObject Interface
	UE_API virtual void BeginDestroy() override;
	//~ End UObject Interface

protected:
	UE_API bool PlayMoverMontage(
		UBulletMoverComponent* InMoverComponent,
		USkeletalMeshComponent* InSkeletalMeshComponent,
		UAnimMontage* MontageToPlay,
		float PlayRate,
		float StartingPosition,
		FName StartingSection);

	UFUNCTION()
	UE_API void OnMoverMontageEnded(FName IgnoredNotifyName);

	UE_API void UnbindMontageDelegates();
};

#undef UE_API
