// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletInstantMovementEffect.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletInstantMovementEffect)

FBulletInstantMovementEffect* FBulletInstantMovementEffect::Clone() const
{
	// If child classes don't override this, saved moves will not work
	checkf(false, TEXT("FBulletInstantMovementEffect::Clone() being called erroneously from %s. A FBulletInstantMovementEffect should never be queued directly and Clone should always be overridden in child structs!"), *GetNameSafe(GetScriptStruct()));
	return nullptr;
}

void FBulletInstantMovementEffect::NetSerialize(FArchive& Ar)
{
	
}

UScriptStruct* FBulletInstantMovementEffect::GetScriptStruct() const
{
	return FBulletInstantMovementEffect::StaticStruct();
}

FString FBulletInstantMovementEffect::ToSimpleString() const
{
	return GetScriptStruct()->GetName();
}
