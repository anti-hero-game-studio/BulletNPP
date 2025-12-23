// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/DataTypes/BulletPhysicsTypes.h"

#include "BulletLogChannels.h"
#include "BulletNPP.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "Core/DataTypes/BulletUserDefinedStruct.h"
#include "StructUtils/UserDefinedStruct.h"

#define LOCTEXT_NAMESPACE "BulletData"

FBulletOnImpactParams::FBulletOnImpactParams() 
	: AttemptedMoveDelta(0) 
{
}

FBulletOnImpactParams::FBulletOnImpactParams(const FName& ModeName, const FHitResult& Hit, const FVector& Delta)
	: MovementModeName(ModeName)
	, HitResult(Hit)
	, AttemptedMoveDelta(Delta)
{
}

FBulletDataStructBase::FBulletDataStructBase()
{
}

FBulletDataStructBase* FBulletDataStructBase::Clone() const
{
	// If child classes don't override this, collections will not work
	checkf(false, TEXT("%hs is being called erroneously on [%s]. This must be overridden in derived types!"), __FUNCTION__, *GetScriptStruct()->GetName());
	return nullptr;
}

UScriptStruct* FBulletDataStructBase::GetScriptStruct() const
{
	checkf(false, TEXT("%hs is being called erroneously. This must be overridden in derived types!"), __FUNCTION__);
	return FBulletDataStructBase::StaticStruct();
}

bool FBulletDataStructBase::ShouldReconcile(const FBulletDataStructBase& AuthorityState) const
{
	checkf(false, TEXT("%hs is being called erroneously on [%s]. This must be overridden in derived types that comprise STATE data (sync/aux) "
					"or INPUT data for use with physics-based movement"), __FUNCTION__, *GetScriptStruct()->GetName());
	return false;
}

void FBulletDataStructBase::Interpolate(const FBulletDataStructBase& From, const FBulletDataStructBase& To, float Pct)
{
	checkf(false, TEXT("%hs is being called erroneously on [%s]. This must be overridden in derived types that comprise STATE data (sync/aux) "
					"or INPUT data for use with physics-based movement"), __FUNCTION__, *GetScriptStruct()->GetName());
}

void FBulletDataStructBase::Merge(const FBulletDataStructBase& From)
{
	checkf(false, TEXT("%hs is being called erroneously on [%s]. This must be overridden in derived types that comprise INPUT data for use with physics-based movement"),
		__FUNCTION__, *GetScriptStruct()->GetName());
}

const UScriptStruct* FBulletDataStructBase::GetDataScriptStruct() const
{ 
	return GetScriptStruct(); 
}


FBulletDataCollection::FBulletDataCollection()
{
}

bool FBulletDataCollection::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	NetSerializeDataArray(Ar, Map, DataArray);

	if (Ar.IsError())
	{
		bOutSuccess = false;
		return false;
	}

	bOutSuccess = true;
	return true;
}

struct FBulletDataDeleter
{
	FORCEINLINE void operator()(FBulletDataStructBase* Object) const
	{
		check(Object);
		UScriptStruct* ScriptStruct = Object->GetScriptStruct();
		check(ScriptStruct);
		ScriptStruct->DestroyStruct(Object);
		FMemory::Free(Object);
	}
};

bool FBulletDataCollection::SerializeDebugData(FArchive& Ar)
{
	// DISCLAIMER: This serialization is not version independent, so it might not be good enough to be used for the Chaos Visual Debugger in the long run

	// First serialize the number of structs in the collection
	int32 NumDataStructs;
	if (Ar.IsLoading())
	{
		Ar << NumDataStructs;
		DataArray.SetNumZeroed(NumDataStructs);
	}
	else
	{
		NumDataStructs = DataArray.Num();
		Ar << NumDataStructs;
	}

	if (Ar.IsLoading())
	{
		DataArray.Empty();
		for (int32 i = 0; i < NumDataStructs && !Ar.IsError(); ++i)
		{
			FString StructName;
			Ar << StructName;
			if (UScriptStruct* MoveDataStruct = Cast<UScriptStruct>(FindObject<UStruct>(nullptr, *StructName)))
			{
				FBulletDataStructBase* NewBulletData = AddDataByType(MoveDataStruct);
				MoveDataStruct->SerializeBin(Ar, NewBulletData);
			}
		}
	}
	else
	{
		for (int32 i = 0; i < DataArray.Num() && !Ar.IsError(); ++i)
		{
			FBulletDataStructBase* MoveDataStruct = DataArray[i].Get();
			if (MoveDataStruct)
			{
				// The FullName of the script struct will be something like "ScriptStruct /Script/Bullet.FCharacterDefaultInputs"
				FString FullStructName = MoveDataStruct->GetScriptStruct()->GetFullName(nullptr);
				// We don't need to save the first part since we only ever save UScriptStructs (C++ structs)
				FString StructName = FullStructName.RightChop(13); // So we chop the "ScriptStruct " part (hence 13 characters)
				Ar << StructName;
				MoveDataStruct->GetScriptStruct()->SerializeBin(Ar, MoveDataStruct);
			}
		}
	}

	return true;
}

FBulletDataCollection& FBulletDataCollection::operator=(const FBulletDataCollection& Other)
{
	// Perform deep copy of this Group
	if (this != &Other)
	{
		bool bCanCopyInPlace = (BulletPhysicsEngine::DisableDataCopyInPlace == 0 && DataArray.Num() == Other.DataArray.Num());
		if (bCanCopyInPlace)
		{
			// If copy in place is enabled and the arrays are the same size, copy by index
			for (int32 i = 0; i < DataArray.Num(); ++i)
			{
				if (FBulletDataStructBase* SrcData = Other.DataArray[i].Get())
				{
					FBulletDataStructBase* DestData = DataArray[i].Get();
					UScriptStruct* SourceStruct = SrcData->GetScriptStruct();

					if (DestData && SourceStruct == DestData->GetScriptStruct())
					{
						// Same type so copy in place
						SourceStruct->CopyScriptStruct(DestData, SrcData, 1);
					}
					else
					{
						// Different type so replace the shared ptr with a clone
						DataArray[i] = TSharedPtr<FBulletDataStructBase>(SrcData->Clone());
					}
				}
				else
				{
					// Found invalid source, fall back to full copy
					bCanCopyInPlace = false;
					break;
				}
			}
		}
		
		if (!bCanCopyInPlace)
		{
			// Deep copy active data blocks
			DataArray.Empty(Other.DataArray.Num());
			for (int i = 0; i < Other.DataArray.Num(); ++i)
			{
				if (Other.DataArray[i].IsValid())
				{
					FBulletDataStructBase* CopyOfSourcePtr = Other.DataArray[i]->Clone();
					DataArray.Add(TSharedPtr<FBulletDataStructBase>(CopyOfSourcePtr));
				}
				else
				{
					UE_LOG(LogBullet, Warning, TEXT("FBulletDataCollection::operator= trying to copy invalid Other DataArray element"));
				}
			}
		}
	}

	return *this;
}

bool FBulletDataCollection::operator==(const FBulletDataCollection& Other) const
{
	// Deep move-by-move comparison
	if (DataArray.Num() != Other.DataArray.Num())
	{
		return false;
	}

	for (int32 i = 0; i < DataArray.Num(); ++i)
	{
		if (DataArray[i].IsValid() == Other.DataArray[i].IsValid())
		{
			if (DataArray[i].IsValid())
			{
				// TODO: Implement deep equality checks
				// 				if (!DataArray[i]->MatchesAndHasSameState(Other.DataArray[i].Get()))
				// 				{
				// 					return false; // They're valid and don't match/have same state
				// 				}
			}
		}
		else
		{
			return false; // Mismatch in validity
		}
	}

	return true;
}

bool FBulletDataCollection::operator!=(const FBulletDataCollection& Other) const
{
	return !(FBulletDataCollection::operator==(Other));
}


bool FBulletDataCollection::ShouldReconcile(const FBulletDataCollection& Other) const
{
	// Collections must have matching elements, and those elements are piece-wise tested for needing reconciliation
	if (DataArray.Num() != Other.DataArray.Num())
	{
		return true;
	}

	for (int32 i = 0; i < DataArray.Num(); ++i)
	{
		const FBulletDataStructBase* DataElement = DataArray[i].Get();
		const FBulletDataStructBase* OtherDataElement = Other.FindDataByType(DataElement->GetDataScriptStruct());

		// Reconciliation is needed if there's no matching types, or if the element pair needs reconciliation
		if (OtherDataElement == nullptr ||
			DataElement->ShouldReconcile(*OtherDataElement))
		{
			return true;
		}
	}

	return false;
}

void FBulletDataCollection::Interpolate(const FBulletDataCollection& From, const FBulletDataCollection& To, float Pct)
{
	// TODO: Consider an inline allocator to avoid dynamic memory allocations
	TSet<TObjectKey<UScriptStruct>> AddedDataTypes;

	// Piece-wise interpolation of matching data blocks
	for (const TSharedPtr<FBulletDataStructBase>& FromElement : From.DataArray)
	{
		AddedDataTypes.Add(FromElement->GetDataScriptStruct());

		if (const FBulletDataStructBase* ToElement = To.FindDataByType(FromElement->GetDataScriptStruct()))
		{
			FBulletDataStructBase* InterpElement = FindOrAddDataByType(FromElement->GetDataScriptStruct());
			InterpElement->Interpolate(*FromElement, *ToElement, Pct);
		}
		else
		{
			// If only present in From, add the block directly to this collection
			AddDataByCopy(FromElement.Get());
		}
	}

	// Add any types present only in To as well
	for (const TSharedPtr<FBulletDataStructBase>& ToElement : To.DataArray)
	{
		if (!AddedDataTypes.Contains(ToElement->GetDataScriptStruct()))
		{
			AddDataByCopy(ToElement.Get());
		}
	}
}

void FBulletDataCollection::Merge(const FBulletDataCollection& From)
{
	for (const TSharedPtr<FBulletDataStructBase>& FromElement : From.DataArray)
	{
		if (FBulletDataStructBase* ExistingElement = FindDataByType(FromElement->GetDataScriptStruct()))
		{
			ExistingElement->Merge(*FromElement);
		}
		else
		{
			// If only present in the previous block, copy it into this block
			AddDataByCopy(FromElement.Get());
		}
	}
}

void FBulletDataCollection::Decay(float DecayAmount)
{
	for (const TSharedPtr<FBulletDataStructBase>& Element : DataArray)
	{
		Element->Decay(DecayAmount);
	}
}


bool FBulletDataCollection::HasSameContents(const FBulletDataCollection& Other) const
{
	if (DataArray.Num() != Other.DataArray.Num())
	{
		return false;
	}

	for (int32 i = 0; i < DataArray.Num(); ++i)
	{
		if (DataArray[i]->GetDataScriptStruct() != Other.DataArray[i]->GetDataScriptStruct())
		{
			return false;
		}
	}

	return true;
}

void FBulletDataCollection::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (const TSharedPtr<FBulletDataStructBase>& Data : DataArray)
	{
		if (Data.IsValid())
		{
			Data->AddReferencedObjects(Collector);
		}
	}
}

void FBulletDataCollection::ToString(FAnsiStringBuilderBase& Out) const
{
	for (const TSharedPtr<FBulletDataStructBase>& Data : DataArray)
	{
		if (Data.IsValid())
		{
			UScriptStruct* Struct = Data->GetScriptStruct();
			Out.Appendf("\n[%s]\n", TCHAR_TO_ANSI(*Struct->GetName()));
			Data->ToString(Out);
		}
	}
}

TArray<TSharedPtr<FBulletDataStructBase>>::TConstIterator FBulletDataCollection::GetCollectionDataIterator() const
{
	return DataArray.CreateConstIterator();
}

//static 
TSharedPtr<FBulletDataStructBase> FBulletDataCollection::CreateDataByType(const UScriptStruct* DataStructType)
{
	check(DataStructType->IsChildOf(FBulletDataStructBase::StaticStruct()));

	FBulletDataStructBase* NewDataBlock = (FBulletDataStructBase*)FMemory::Malloc(DataStructType->GetCppStructOps()->GetSize());
	DataStructType->InitializeStruct(NewDataBlock);

	return TSharedPtr<FBulletDataStructBase>(NewDataBlock, FBulletDataDeleter());
}


FBulletDataStructBase* FBulletDataCollection::AddDataByType(const UScriptStruct* DataStructType)
{
	if (ensure(!FindDataByType(DataStructType)))
	{
		TSharedPtr<FBulletDataStructBase> NewDataInstance;

		if (DataStructType->IsA<UUserDefinedStruct>())
		{
			NewDataInstance = CreateDataByType(FBulletUserDefinedDataStruct::StaticStruct());
			static_cast<FBulletUserDefinedDataStruct*>(NewDataInstance.Get())->StructInstance.InitializeAs(DataStructType);
		}
		else
		{
			NewDataInstance = CreateDataByType(DataStructType);
		}

		DataArray.Add(NewDataInstance);
		return NewDataInstance.Get();
	}
	
	return nullptr;
}


void FBulletDataCollection::AddOrOverwriteData(const TSharedPtr<FBulletDataStructBase> DataInstance)
{
	RemoveDataByType(DataInstance->GetDataScriptStruct());
	DataArray.Add(DataInstance);
}


void FBulletDataCollection::AddDataByCopy(const FBulletDataStructBase* DataInstanceToCopy)
{
	check(DataInstanceToCopy);

	const UScriptStruct* TypeToMatch = DataInstanceToCopy->GetDataScriptStruct();

	if (FBulletDataStructBase* ExistingMatchingData = FindDataByType(TypeToMatch))
	{
		// Note that we've matched based on the "data" type but we're copying the top-level type (a FBulletDataStructBase subtype)
		const UScriptStruct* BulletDataTypeToCopy = DataInstanceToCopy->GetScriptStruct();
		BulletDataTypeToCopy->CopyScriptStruct(ExistingMatchingData, DataInstanceToCopy, 1);
	}
	else
	{
		DataArray.Add(TSharedPtr<FBulletDataStructBase>(DataInstanceToCopy->Clone()));
	}
}


FBulletDataStructBase* FBulletDataCollection::FindDataByType(const UScriptStruct* DataStructType) const
{
	for (const TSharedPtr<FBulletDataStructBase>& Data : DataArray)
	{
		const UStruct* CandidateStruct = Data->GetDataScriptStruct();
		while (CandidateStruct)
		{
			if (DataStructType == CandidateStruct)
			{
				return Data.Get();
			}

			CandidateStruct = CandidateStruct->GetSuperStruct();
		}
	}

	return nullptr;
}


FBulletDataStructBase* FBulletDataCollection::FindOrAddDataByType(const UScriptStruct* DataStructType)
{
	if (FBulletDataStructBase* ExistingData = FindDataByType(DataStructType))
	{
		return ExistingData;
	}

	return AddDataByType(DataStructType);
}


bool FBulletDataCollection::RemoveDataByType(const UScriptStruct* DataStructType)
{
	int32 IndexToRemove = -1;

	for (int32 i=0; i < DataArray.Num() && IndexToRemove < 0; ++i)
	{
		const UStruct* CandidateStruct = DataArray[i]->GetDataScriptStruct();
		while (CandidateStruct)
		{
			if (DataStructType == CandidateStruct)
			{
				IndexToRemove = i;
				break;
			}

			CandidateStruct = CandidateStruct->GetSuperStruct();
		}
	}

	if (IndexToRemove >= 0)
	{
		DataArray.RemoveAt(IndexToRemove);
		return true;
	}

	return false;
}

/*static*/
void FBulletDataCollection::NetSerializeDataArray(FArchive& Ar, UPackageMap* Map, TArray<TSharedPtr<FBulletDataStructBase>>& DataArray)
{
	uint8 NumDataStructsToSerialize;
	if (Ar.IsSaving())
	{
		NumDataStructsToSerialize = DataArray.Num();
	}

	Ar << NumDataStructsToSerialize;

	if (Ar.IsLoading())
	{
		DataArray.SetNumZeroed(NumDataStructsToSerialize);
	}

	for (int32 i = 0; i < NumDataStructsToSerialize && !Ar.IsError(); ++i)
	{
		TCheckedObjPtr<UScriptStruct> ScriptStruct = DataArray[i].IsValid() ? DataArray[i]->GetScriptStruct() : nullptr;
		UScriptStruct* ScriptStructLocal = ScriptStruct.Get();

		Ar << ScriptStruct;

		if (ScriptStruct.IsValid())
		{
			// Restrict replication to derived classes of FBulletDataStructBase for security reasons:
			// If FBulletDataCollection is replicated through a Server RPC, we need to prevent clients from sending us
			// arbitrary ScriptStructs due to the allocation/reliance on GetCppStructOps below which could trigger a server crash
			// for invalid structs. All provided sources are direct children of FBulletDataStructBase and we never expect to have deep hierarchies
			// so this should not be too costly
			bool bIsDerivedFromBase = false;
			UStruct* CurrentSuperStruct = ScriptStruct->GetSuperStruct();
			while (CurrentSuperStruct)
			{
				if (CurrentSuperStruct == FBulletDataStructBase::StaticStruct())
				{
					bIsDerivedFromBase = true;
					break;
				}
				CurrentSuperStruct = CurrentSuperStruct->GetSuperStruct();
			}

			if (bIsDerivedFromBase)
			{
				if (Ar.IsLoading())
				{
					if (DataArray[i].IsValid() && ScriptStructLocal == ScriptStruct.Get())
					{
						// What we have locally is the same type as we're being serialized into, so we don't need to
						// reallocate - just use existing structure
					}
					else
					{
						// For now, just reset/reallocate the data when loading.
						// Longer term if we want to generalize this and use it for property replication, we should support
						// only reallocating when necessary
						FBulletDataStructBase* NewDataBlock = (FBulletDataStructBase*)FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
						ScriptStruct->InitializeStruct(NewDataBlock);

						DataArray[i] = TSharedPtr<FBulletDataStructBase>(NewDataBlock, FBulletDataDeleter());
					}
				}

				bool bArrayElementSuccess = false;
				DataArray[i]->NetSerialize(Ar, Map, bArrayElementSuccess);

				if (!bArrayElementSuccess)
				{
					UE_LOG(LogBullet, Error, TEXT("FBulletDataCollection::NetSerialize: Failed to serialize ScriptStruct %s"), *ScriptStruct->GetName());
					Ar.SetError();
					break;
				}
			}
			else
			{
				UE_LOG(LogBullet, Error, TEXT("FBulletDataCollection::NetSerialize: ScriptStruct not derived from FBulletDataStructBase attempted to serialize."));
				Ar.SetError();
				break;
			}
		}
		else if (ScriptStruct.IsError())
		{
			UE_LOG(LogBullet, Error, TEXT("FBulletDataCollection::NetSerialize: Invalid ScriptStruct serialized."));
			Ar.SetError();
			break;
		}
	}

}



void UBulletDataCollectionLibrary::K2_AddDataToCollection(FBulletDataCollection& Collection, const int32& SourceAsRawBytes)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();
}

// static
DEFINE_FUNCTION(UBulletDataCollectionLibrary::execK2_AddDataToCollection)
{
	P_GET_STRUCT_REF(FBulletDataCollection, TargetCollection);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* SourceDataAsRawPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* SourceStructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	if (!SourceDataAsRawPtr || !SourceStructProp)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("BulletDataCollection_AddDataToCollection", "Failed to resolve the SourceAsRawBytes for AddDataToCollection")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;

		if (ensure(SourceStructProp->Struct))
		{
			// User-defined struct type support: we wrap an instance inside a FBulletUserDefinedDataStruct
			if (SourceStructProp->Struct->IsA<UUserDefinedStruct>())
			{
				FBulletUserDefinedDataStruct UserDefinedDataWrapper;
				UserDefinedDataWrapper.StructInstance.InitializeAs(SourceStructProp->Struct, (uint8*)SourceDataAsRawPtr);

				TargetCollection.AddDataByCopy(&UserDefinedDataWrapper);
			}
			else if (SourceStructProp->Struct->IsChildOf(FBulletDataStructBase::StaticStruct()))
			{
				FBulletDataStructBase* SourceDataAsBasePtr = reinterpret_cast<FBulletDataStructBase*>(SourceDataAsRawPtr);
				TargetCollection.AddDataByCopy(SourceDataAsBasePtr);
			}
			else
			{
				UE_LOG(LogBullet, Warning, TEXT("AddDataToCollection: invalid struct type submitted: %s"), *SourceStructProp->Struct->GetName());
			}
		}

		P_NATIVE_END;
	}
}


void UBulletDataCollectionLibrary::K2_GetDataFromCollection(bool& DidSucceed, const FBulletDataCollection& Collection, int32& TargetAsRawBytes)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();
}

// static
DEFINE_FUNCTION(UBulletDataCollectionLibrary::execK2_GetDataFromCollection)
{
	P_GET_UBOOL_REF(DidSucceed);
	P_GET_STRUCT_REF(FBulletDataCollection, TargetCollection);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* TargetDataAsRawPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* TargetStructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	DidSucceed = false;

	if (!TargetDataAsRawPtr || !TargetStructProp)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("BulletDataCollection_GetDataFromCollection_UnresolvedTarget", "Failed to resolve the TargetAsRawBytes for GetDataFromCollection")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else if (!TargetStructProp->Struct || 
				(!TargetStructProp->Struct->IsChildOf(FBulletDataStructBase::StaticStruct()) && !TargetStructProp->Struct->IsA<UUserDefinedStruct>()))
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("BulletDataCollection_GetDataFromCollection_BadType", "TargetAsRawBytes is not a valid type. Must be a child of FBulletDataStructBase or a User-Defined Struct type.")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;

		if (TargetStructProp->Struct->IsA<UUserDefinedStruct>())
		{
			if (FBulletDataStructBase* FoundDataInstance = TargetCollection.FindDataByType(TargetStructProp->Struct))
			{
				// User-defined struct instances are wrapped in a FBulletUserDefinedDataStruct, so we need to extract the instance memory from inside it
				FBulletUserDefinedDataStruct* FoundBPDataInstance = static_cast<FBulletUserDefinedDataStruct*>(FoundDataInstance);
				TargetStructProp->Struct->CopyScriptStruct(TargetDataAsRawPtr, FoundBPDataInstance->StructInstance.GetMemory());
				DidSucceed = true;
			}
		}
		else
		{
			if (FBulletDataStructBase* FoundDataInstance = TargetCollection.FindDataByType(TargetStructProp->Struct))
			{
				TargetStructProp->Struct->CopyScriptStruct(TargetDataAsRawPtr, FoundDataInstance);
				DidSucceed = true;
			}
		}

		P_NATIVE_END;
	}
}


void UBulletDataCollectionLibrary::ClearDataFromCollection(FBulletDataCollection& Collection)
{
	Collection.Empty();
}

#undef LOCTEXT_NAMESPACE
