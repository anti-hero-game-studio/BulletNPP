// Copyright Epic Games, Inc. All Rights Reserved.


#include "BulletUserDefinedStructSupport.h"
#include "StructUtils/UserDefinedStruct.h"
#include "BulletMoverLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletUserDefinedStructSupport)

#define LOCTEXT_NAMESPACE "BulletMoverUDSInstances"


// TODO: Consider different rules for interpolation/merging/reconciliation checks. 
// This could be accomplished via cvars / Mover settings / per-type metadata , etc.

bool FBulletMoverUserDefinedDataStruct::ShouldReconcile(const FBulletMoverDataStructBase& AuthorityState) const
{
	const FBulletMoverUserDefinedDataStruct& TypedAuthority = static_cast<const FBulletMoverUserDefinedDataStruct&>(AuthorityState);

	check(TypedAuthority.StructInstance.GetScriptStruct() == this->StructInstance.GetScriptStruct());

	return !StructInstance.Identical(&TypedAuthority.StructInstance, EPropertyPortFlags::PPF_DeepComparison);
}

void FBulletMoverUserDefinedDataStruct::Interpolate(const FBulletMoverDataStructBase& From, const FBulletMoverDataStructBase& To, float LerpFactor)
{
	const FBulletMoverUserDefinedDataStruct& PrimarySource = static_cast<const FBulletMoverUserDefinedDataStruct&>((LerpFactor < 0.5f) ? From : To);

	// copy all properties from the heaviest-weighted source rather than interpolate
	StructInstance = PrimarySource.StructInstance;
}

void FBulletMoverUserDefinedDataStruct::Merge(const FBulletMoverDataStructBase& From)
{
	const FBulletMoverUserDefinedDataStruct& TypedFrom = static_cast<const FBulletMoverUserDefinedDataStruct&>(From);

	check(TypedFrom.StructInstance.GetScriptStruct() == this->StructInstance.GetScriptStruct());

	// Merging is typically only done for inputs. Let's make the assumption that boolean inputs should be OR'd so we never miss any digital inputs.

	if (const UScriptStruct* UdsScriptStruct = TypedFrom.StructInstance.GetScriptStruct())
	{
		uint8* ThisInstanceMemory = StructInstance.GetMutableMemory();
		const uint8* FromInstanceMemory = TypedFrom.StructInstance.GetMemory();

		for (TFieldIterator<FBoolProperty> BoolProperty(UdsScriptStruct); BoolProperty; ++BoolProperty)
		{
			bool bMergedBool = BoolProperty->GetPropertyValue(ThisInstanceMemory);

			if (!bMergedBool)
			{
				bMergedBool |= BoolProperty->GetPropertyValue(FromInstanceMemory);

				if (bMergedBool)
				{
					BoolProperty->SetPropertyValue(ThisInstanceMemory, bMergedBool);
				}
			}
		}
	}
}

FBulletMoverDataStructBase* FBulletMoverUserDefinedDataStruct::Clone() const
{
	FBulletMoverUserDefinedDataStruct* CopyPtr = new FBulletMoverUserDefinedDataStruct(*this);
	return CopyPtr;
}

bool FBulletMoverUserDefinedDataStruct::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuperSuccess, bStructSuccess;

	Super::NetSerialize(Ar, Map, bSuperSuccess);
	StructInstance.NetSerialize(Ar, Map, bStructSuccess);

	bOutSuccess = bSuperSuccess && bStructSuccess;

	return true;
}


void FBulletMoverUserDefinedDataStruct::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	// TODO: add property-wise concatenated string output
}

const UScriptStruct* FBulletMoverUserDefinedDataStruct::GetDataScriptStruct() const
{
	return StructInstance.GetScriptStruct();
}


#undef LOCTEXT_NAMESPACE
