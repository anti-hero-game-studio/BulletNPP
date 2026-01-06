// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletSimpleSpringState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletSimpleSpringState)

UScriptStruct* FBulletSimpleSpringState::GetScriptStruct() const
{ 
	return StaticStruct(); 
}

FBulletMoverDataStructBase* FBulletSimpleSpringState::Clone() const
{ 
	return new FBulletSimpleSpringState(*this); 
}

bool FBulletSimpleSpringState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuccess = Super::NetSerialize(Ar, Map, bOutSuccess);

	// Could be quantized to save bandwidth
	Ar << CurrentAccel;

	return bSuccess;
}

void FBulletSimpleSpringState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("Accel=%s\n", *CurrentAccel.ToCompactString());
}

bool FBulletSimpleSpringState::ShouldReconcile(const FBulletMoverDataStructBase& AuthorityState) const
{
	const FBulletSimpleSpringState* AuthoritySpringState = static_cast<const FBulletSimpleSpringState*>(&AuthorityState);

	return !(CurrentAccel - AuthoritySpringState->CurrentAccel).IsNearlyZero();
		
}


void FBulletSimpleSpringState::Interpolate(const FBulletMoverDataStructBase& From, const FBulletMoverDataStructBase& To, float Pct)
{
	const FBulletSimpleSpringState* FromState = static_cast<const FBulletSimpleSpringState*>(&From);
	const FBulletSimpleSpringState* ToState   = static_cast<const FBulletSimpleSpringState*>(&To);

	CurrentAccel           = FMath::Lerp(FromState->CurrentAccel, ToState->CurrentAccel, Pct);
}
