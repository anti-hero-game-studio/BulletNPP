// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/DataTypes/BulletUserDefinedStruct.h"

#define LOCTEXT_NAMESPACE "BulletUDSInstances"


// TODO: Consider different rules for interpolation/merging/reconciliation checks. 
// This could be accomplished via cvars / Bullet settings / per-type metadata , etc.

bool FBulletUserDefinedDataStruct::ShouldReconcile(const FBulletDataStructBase& AuthorityState) const
{
	const FBulletUserDefinedDataStruct& TypedAuthority = static_cast<const FBulletUserDefinedDataStruct&>(AuthorityState);

	check(TypedAuthority.StructInstance.GetScriptStruct() == this->StructInstance.GetScriptStruct());

	return !StructInstance.Identical(&TypedAuthority.StructInstance, EPropertyPortFlags::PPF_DeepComparison);
}

void FBulletUserDefinedDataStruct::Interpolate(const FBulletDataStructBase& From, const FBulletDataStructBase& To, float LerpFactor)
{
	const FBulletUserDefinedDataStruct& PrimarySource = static_cast<const FBulletUserDefinedDataStruct&>((LerpFactor < 0.5f) ? From : To);

	// copy all properties from the heaviest-weighted source rather than interpolate
	StructInstance = PrimarySource.StructInstance;
}

void FBulletUserDefinedDataStruct::Merge(const FBulletDataStructBase& From)
{
	const FBulletUserDefinedDataStruct& TypedFrom = static_cast<const FBulletUserDefinedDataStruct&>(From);

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

FBulletDataStructBase* FBulletUserDefinedDataStruct::Clone() const
{
	FBulletUserDefinedDataStruct* CopyPtr = new FBulletUserDefinedDataStruct(*this);
	return CopyPtr;
}

bool FBulletUserDefinedDataStruct::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuperSuccess, bStructSuccess;

	Super::NetSerialize(Ar, Map, bSuperSuccess);
	StructInstance.NetSerialize(Ar, Map, bStructSuccess);

	bOutSuccess = bSuperSuccess && bStructSuccess;

	return true;
}


void FBulletUserDefinedDataStruct::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	// TODO: add property-wise concatenated string output
}

const UScriptStruct* FBulletUserDefinedDataStruct::GetDataScriptStruct() const
{
	return StructInstance.GetScriptStruct();
}


#undef LOCTEXT_NAMESPACE