// Copyright Epic Games, Inc. All Rights Reserved.


#include "BulletMoverTypes.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "BulletMoverLog.h"
#include "BulletMoverModule.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UObject/ObjectKey.h"
#include "BulletUserDefinedStructSupport.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMoverTypes)

#define LOCTEXT_NAMESPACE "BulletMoverData"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(BulletMover_IsOnGround, "BulletMover.IsOnGround", "Default Mover state flag indicating character is on the ground.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(BulletMover_IsInAir, "BulletMover.IsInAir", "Default Mover state flag indicating character is in the air.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(BulletMover_IsFalling, "BulletMover.IsFalling", "Default Mover state flag indicating character is falling.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(BulletMover_IsFlying, "BulletMover.IsFlying", "Default Mover state flag indicating character is flying.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(BulletMover_IsSwimming, "BulletMover.IsSwimming", "Default Mover state flag indicating character is swimming.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(BulletMover_IsCrouching, "BulletMover.Stance.IsCrouching", "Default Mover state flag indicating character is crouching.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(BulletMover_IsNavWalking, "BulletMover.IsNavWalking", "Default Mover state flag indicating character is NavWalking.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(BulletMover_SkipAnimRootMotion, "BulletMover.SkipAnimRootMotion", "Default Mover state flag indicating Animation Root Motion proposed movement should be skipped.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(BulletMover_SkipVerticalAnimRootMotion, "BulletMover.SkipVerticalAnimRootMotion", "Default Mover state flag indicating Animation Root Motion proposed movements should not include a vertical velocity component (along the up/down axis).");

FBulletMoverOnImpactParams::FBulletMoverOnImpactParams() 
	: AttemptedMoveDelta(0) 
{
}

FBulletMoverOnImpactParams::FBulletMoverOnImpactParams(const FName& ModeName, const FHitResult& Hit, const FVector& Delta)
	: MovementModeName(ModeName)
	, HitResult(Hit)
	, AttemptedMoveDelta(Delta)
{
}

FBulletMoverDataStructBase::FBulletMoverDataStructBase()
{
}

FBulletMoverDataStructBase* FBulletMoverDataStructBase::Clone() const
{
	// If child classes don't override this, collections will not work
	checkf(false, TEXT("%hs is being called erroneously on [%s]. This must be overridden in derived types!"), __FUNCTION__, *GetScriptStruct()->GetName());
	return nullptr;
}

UScriptStruct* FBulletMoverDataStructBase::GetScriptStruct() const
{
	checkf(false, TEXT("%hs is being called erroneously. This must be overridden in derived types!"), __FUNCTION__);
	return FBulletMoverDataStructBase::StaticStruct();
}

bool FBulletMoverDataStructBase::ShouldReconcile(const FBulletMoverDataStructBase& AuthorityState) const
{
	checkf(false, TEXT("%hs is being called erroneously on [%s]. This must be overridden in derived types that comprise STATE data (sync/aux) "
					"or INPUT data for use with physics-based movement"), __FUNCTION__, *GetScriptStruct()->GetName());
	return false;
}

void FBulletMoverDataStructBase::Interpolate(const FBulletMoverDataStructBase& From, const FBulletMoverDataStructBase& To, float Pct)
{
	checkf(false, TEXT("%hs is being called erroneously on [%s]. This must be overridden in derived types that comprise STATE data (sync/aux) "
					"or INPUT data for use with physics-based movement"), __FUNCTION__, *GetScriptStruct()->GetName());
}

void FBulletMoverDataStructBase::Merge(const FBulletMoverDataStructBase& From)
{
	checkf(false, TEXT("%hs is being called erroneously on [%s]. This must be overridden in derived types that comprise INPUT data for use with physics-based movement"),
		__FUNCTION__, *GetScriptStruct()->GetName());
}

const UScriptStruct* FBulletMoverDataStructBase::GetDataScriptStruct() const
{ 
	return GetScriptStruct(); 
}


FBulletMoverDataCollection::FBulletMoverDataCollection()
{
}

bool FBulletMoverDataCollection::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
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

struct FBulletMoverDataDeleter
{
	FORCEINLINE void operator()(FBulletMoverDataStructBase* Object) const
	{
		check(Object);
		UScriptStruct* ScriptStruct = Object->GetScriptStruct();
		check(ScriptStruct);
		ScriptStruct->DestroyStruct(Object);
		FMemory::Free(Object);
	}
};

bool FBulletMoverDataCollection::SerializeDebugData(FArchive& Ar)
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
				FBulletMoverDataStructBase* NewMoverData = AddDataByType(MoveDataStruct);
				MoveDataStruct->SerializeBin(Ar, NewMoverData);
			}
		}
	}
	else
	{
		for (int32 i = 0; i < DataArray.Num() && !Ar.IsError(); ++i)
		{
			FBulletMoverDataStructBase* MoveDataStruct = DataArray[i].Get();
			if (MoveDataStruct)
			{
				// The FullName of the script struct will be something like "ScriptStruct /Script/BulletMover.FBulletCharacterDefaultInputs"
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

FBulletMoverDataCollection& FBulletMoverDataCollection::operator=(const FBulletMoverDataCollection& Other)
{
	// Perform deep copy of this Group
	if (this != &Other)
	{
		bool bCanCopyInPlace = (UE::BulletMover::DisableDataCopyInPlace == 0 && DataArray.Num() == Other.DataArray.Num());
		if (bCanCopyInPlace)
		{
			// If copy in place is enabled and the arrays are the same size, copy by index
			for (int32 i = 0; i < DataArray.Num(); ++i)
			{
				if (FBulletMoverDataStructBase* SrcData = Other.DataArray[i].Get())
				{
					FBulletMoverDataStructBase* DestData = DataArray[i].Get();
					UScriptStruct* SourceStruct = SrcData->GetScriptStruct();

					if (DestData && SourceStruct == DestData->GetScriptStruct())
					{
						// Same type so copy in place
						SourceStruct->CopyScriptStruct(DestData, SrcData, 1);
					}
					else
					{
						// Different type so replace the shared ptr with a clone
						DataArray[i] = TSharedPtr<FBulletMoverDataStructBase>(SrcData->Clone());
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
					FBulletMoverDataStructBase* CopyOfSourcePtr = Other.DataArray[i]->Clone();
					DataArray.Add(TSharedPtr<FBulletMoverDataStructBase>(CopyOfSourcePtr));
				}
				else
				{
					UE_LOG(LogBulletMover, Warning, TEXT("FBulletMoverDataCollection::operator= trying to copy invalid Other DataArray element"));
				}
			}
		}
	}

	return *this;
}

bool FBulletMoverDataCollection::operator==(const FBulletMoverDataCollection& Other) const
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

bool FBulletMoverDataCollection::operator!=(const FBulletMoverDataCollection& Other) const
{
	return !(FBulletMoverDataCollection::operator==(Other));
}


bool FBulletMoverDataCollection::ShouldReconcile(const FBulletMoverDataCollection& Other) const
{
	// Collections must have matching elements, and those elements are piece-wise tested for needing reconciliation
	if (DataArray.Num() != Other.DataArray.Num())
	{
		return true;
	}

	for (int32 i = 0; i < DataArray.Num(); ++i)
	{
		const FBulletMoverDataStructBase* DataElement = DataArray[i].Get();
		const FBulletMoverDataStructBase* OtherDataElement = Other.FindDataByType(DataElement->GetDataScriptStruct());

		// Reconciliation is needed if there's no matching types, or if the element pair needs reconciliation
		if (OtherDataElement == nullptr ||
			DataElement->ShouldReconcile(*OtherDataElement))
		{
			return true;
		}
	}

	return false;
}

void FBulletMoverDataCollection::Interpolate(const FBulletMoverDataCollection& From, const FBulletMoverDataCollection& To, float Pct)
{
	// TODO: Consider an inline allocator to avoid dynamic memory allocations
	TSet<TObjectKey<UScriptStruct>> AddedDataTypes;

	// Piece-wise interpolation of matching data blocks
	for (const TSharedPtr<FBulletMoverDataStructBase>& FromElement : From.DataArray)
	{
		AddedDataTypes.Add(FromElement->GetDataScriptStruct());

		if (const FBulletMoverDataStructBase* ToElement = To.FindDataByType(FromElement->GetDataScriptStruct()))
		{
			FBulletMoverDataStructBase* InterpElement = FindOrAddDataByType(FromElement->GetDataScriptStruct());
			InterpElement->Interpolate(*FromElement, *ToElement, Pct);
		}
		else
		{
			// If only present in From, add the block directly to this collection
			AddDataByCopy(FromElement.Get());
		}
	}

	// Add any types present only in To as well
	for (const TSharedPtr<FBulletMoverDataStructBase>& ToElement : To.DataArray)
	{
		if (!AddedDataTypes.Contains(ToElement->GetDataScriptStruct()))
		{
			AddDataByCopy(ToElement.Get());
		}
	}
}

void FBulletMoverDataCollection::Merge(const FBulletMoverDataCollection& From)
{
	for (const TSharedPtr<FBulletMoverDataStructBase>& FromElement : From.DataArray)
	{
		if (FBulletMoverDataStructBase* ExistingElement = FindDataByType(FromElement->GetDataScriptStruct()))
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

void FBulletMoverDataCollection::Decay(float DecayAmount)
{
	for (const TSharedPtr<FBulletMoverDataStructBase>& Element : DataArray)
	{
		Element->Decay(DecayAmount);
	}
}


bool FBulletMoverDataCollection::HasSameContents(const FBulletMoverDataCollection& Other) const
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

void FBulletMoverDataCollection::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (const TSharedPtr<FBulletMoverDataStructBase>& Data : DataArray)
	{
		if (Data.IsValid())
		{
			Data->AddReferencedObjects(Collector);
		}
	}
}

void FBulletMoverDataCollection::ToString(FAnsiStringBuilderBase& Out) const
{
	for (const TSharedPtr<FBulletMoverDataStructBase>& Data : DataArray)
	{
		if (Data.IsValid())
		{
			UScriptStruct* Struct = Data->GetScriptStruct();
			Out.Appendf("\n[%s]\n", TCHAR_TO_ANSI(*Struct->GetName()));
			Data->ToString(Out);
		}
	}
}

TArray<TSharedPtr<FBulletMoverDataStructBase>>::TConstIterator FBulletMoverDataCollection::GetCollectionDataIterator() const
{
	return DataArray.CreateConstIterator();
}

//static 
TSharedPtr<FBulletMoverDataStructBase> FBulletMoverDataCollection::CreateDataByType(const UScriptStruct* DataStructType)
{
	check(DataStructType->IsChildOf(FBulletMoverDataStructBase::StaticStruct()));

	FBulletMoverDataStructBase* NewDataBlock = (FBulletMoverDataStructBase*)FMemory::Malloc(DataStructType->GetCppStructOps()->GetSize());
	DataStructType->InitializeStruct(NewDataBlock);

	return TSharedPtr<FBulletMoverDataStructBase>(NewDataBlock, FBulletMoverDataDeleter());
}


FBulletMoverDataStructBase* FBulletMoverDataCollection::AddDataByType(const UScriptStruct* DataStructType)
{
	if (ensure(!FindDataByType(DataStructType)))
	{
		TSharedPtr<FBulletMoverDataStructBase> NewDataInstance;

		if (DataStructType->IsA<UUserDefinedStruct>())
		{
			NewDataInstance = CreateDataByType(FBulletMoverUserDefinedDataStruct::StaticStruct());
			static_cast<FBulletMoverUserDefinedDataStruct*>(NewDataInstance.Get())->StructInstance.InitializeAs(DataStructType);
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


void FBulletMoverDataCollection::AddOrOverwriteData(const TSharedPtr<FBulletMoverDataStructBase> DataInstance)
{
	RemoveDataByType(DataInstance->GetDataScriptStruct());
	DataArray.Add(DataInstance);
}


void FBulletMoverDataCollection::AddDataByCopy(const FBulletMoverDataStructBase* DataInstanceToCopy)
{
	check(DataInstanceToCopy);

	const UScriptStruct* TypeToMatch = DataInstanceToCopy->GetDataScriptStruct();

	if (FBulletMoverDataStructBase* ExistingMatchingData = FindDataByType(TypeToMatch))
	{
		// Note that we've matched based on the "data" type but we're copying the top-level type (a FBulletMoverDataStructBase subtype)
		const UScriptStruct* MoverDataTypeToCopy = DataInstanceToCopy->GetScriptStruct();
		MoverDataTypeToCopy->CopyScriptStruct(ExistingMatchingData, DataInstanceToCopy, 1);
	}
	else
	{
		DataArray.Add(TSharedPtr<FBulletMoverDataStructBase>(DataInstanceToCopy->Clone()));
	}
}


FBulletMoverDataStructBase* FBulletMoverDataCollection::FindDataByType(const UScriptStruct* DataStructType) const
{
	for (const TSharedPtr<FBulletMoverDataStructBase>& Data : DataArray)
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


FBulletMoverDataStructBase* FBulletMoverDataCollection::FindOrAddDataByType(const UScriptStruct* DataStructType)
{
	if (FBulletMoverDataStructBase* ExistingData = FindDataByType(DataStructType))
	{
		return ExistingData;
	}

	return AddDataByType(DataStructType);
}


bool FBulletMoverDataCollection::RemoveDataByType(const UScriptStruct* DataStructType)
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
void FBulletMoverDataCollection::NetSerializeDataArray(FArchive& Ar, UPackageMap* Map, TArray<TSharedPtr<FBulletMoverDataStructBase>>& DataArray)
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
			// Restrict replication to derived classes of FBulletMoverDataStructBase for security reasons:
			// If FBulletMoverDataCollection is replicated through a Server RPC, we need to prevent clients from sending us
			// arbitrary ScriptStructs due to the allocation/reliance on GetCppStructOps below which could trigger a server crash
			// for invalid structs. All provided sources are direct children of FBulletMoverDataStructBase and we never expect to have deep hierarchies
			// so this should not be too costly
			bool bIsDerivedFromBase = false;
			UStruct* CurrentSuperStruct = ScriptStruct->GetSuperStruct();
			while (CurrentSuperStruct)
			{
				if (CurrentSuperStruct == FBulletMoverDataStructBase::StaticStruct())
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
						FBulletMoverDataStructBase* NewDataBlock = (FBulletMoverDataStructBase*)FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
						ScriptStruct->InitializeStruct(NewDataBlock);

						DataArray[i] = TSharedPtr<FBulletMoverDataStructBase>(NewDataBlock, FBulletMoverDataDeleter());
					}
				}

				bool bArrayElementSuccess = false;
				DataArray[i]->NetSerialize(Ar, Map, bArrayElementSuccess);

				if (!bArrayElementSuccess)
				{
					UE_LOG(LogBulletMover, Error, TEXT("FBulletMoverDataCollection::NetSerialize: Failed to serialize ScriptStruct %s"), *ScriptStruct->GetName());
					Ar.SetError();
					break;
				}
			}
			else
			{
				UE_LOG(LogBulletMover, Error, TEXT("FBulletMoverDataCollection::NetSerialize: ScriptStruct not derived from FBulletMoverDataStructBase attempted to serialize."));
				Ar.SetError();
				break;
			}
		}
		else if (ScriptStruct.IsError())
		{
			UE_LOG(LogBulletMover, Error, TEXT("FBulletMoverDataCollection::NetSerialize: Invalid ScriptStruct serialized."));
			Ar.SetError();
			break;
		}
	}

}



void UBulletMoverDataCollectionLibrary::K2_AddDataToCollection(FBulletMoverDataCollection& Collection, const int32& SourceAsRawBytes)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();
}

// static
DEFINE_FUNCTION(UBulletMoverDataCollectionLibrary::execK2_AddDataToCollection)
{
	P_GET_STRUCT_REF(FBulletMoverDataCollection, TargetCollection);

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
			LOCTEXT("BulletMoverDataCollection_AddDataToCollection", "Failed to resolve the SourceAsRawBytes for AddDataToCollection")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;

		if (ensure(SourceStructProp->Struct))
		{
			// User-defined struct type support: we wrap an instance inside a FBulletMoverUserDefinedDataStruct
			if (SourceStructProp->Struct->IsA<UUserDefinedStruct>())
			{
				FBulletMoverUserDefinedDataStruct UserDefinedDataWrapper;
				UserDefinedDataWrapper.StructInstance.InitializeAs(SourceStructProp->Struct, (uint8*)SourceDataAsRawPtr);

				TargetCollection.AddDataByCopy(&UserDefinedDataWrapper);
			}
			else if (SourceStructProp->Struct->IsChildOf(FBulletMoverDataStructBase::StaticStruct()))
			{
				FBulletMoverDataStructBase* SourceDataAsBasePtr = reinterpret_cast<FBulletMoverDataStructBase*>(SourceDataAsRawPtr);
				TargetCollection.AddDataByCopy(SourceDataAsBasePtr);
			}
			else
			{
				UE_LOG(LogBulletMover, Warning, TEXT("AddDataToCollection: invalid struct type submitted: %s"), *SourceStructProp->Struct->GetName());
			}
		}

		P_NATIVE_END;
	}
}


void UBulletMoverDataCollectionLibrary::K2_GetDataFromCollection(bool& DidSucceed, const FBulletMoverDataCollection& Collection, int32& TargetAsRawBytes)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();
}

// static
DEFINE_FUNCTION(UBulletMoverDataCollectionLibrary::execK2_GetDataFromCollection)
{
	P_GET_UBOOL_REF(DidSucceed);
	P_GET_STRUCT_REF(FBulletMoverDataCollection, TargetCollection);

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
			LOCTEXT("BulletMoverDataCollection_GetDataFromCollection_UnresolvedTarget", "Failed to resolve the TargetAsRawBytes for GetDataFromCollection")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else if (!TargetStructProp->Struct || 
				(!TargetStructProp->Struct->IsChildOf(FBulletMoverDataStructBase::StaticStruct()) && !TargetStructProp->Struct->IsA<UUserDefinedStruct>()))
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			LOCTEXT("BulletMoverDataCollection_GetDataFromCollection_BadType", "TargetAsRawBytes is not a valid type. Must be a child of FBulletMoverDataStructBase or a User-Defined Struct type.")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
	}
	else
	{
		P_NATIVE_BEGIN;

		if (TargetStructProp->Struct->IsA<UUserDefinedStruct>())
		{
			if (FBulletMoverDataStructBase* FoundDataInstance = TargetCollection.FindDataByType(TargetStructProp->Struct))
			{
				// User-defined struct instances are wrapped in a FBulletMoverUserDefinedDataStruct, so we need to extract the instance memory from inside it
				FBulletMoverUserDefinedDataStruct* FoundBPDataInstance = static_cast<FBulletMoverUserDefinedDataStruct*>(FoundDataInstance);
				TargetStructProp->Struct->CopyScriptStruct(TargetDataAsRawPtr, FoundBPDataInstance->StructInstance.GetMemory());
				DidSucceed = true;
			}
		}
		else
		{
			if (FBulletMoverDataStructBase* FoundDataInstance = TargetCollection.FindDataByType(TargetStructProp->Struct))
			{
				TargetStructProp->Struct->CopyScriptStruct(TargetDataAsRawPtr, FoundDataInstance);
				DidSucceed = true;
			}
		}

		P_NATIVE_END;
	}
}


void UBulletMoverDataCollectionLibrary::ClearDataFromCollection(FBulletMoverDataCollection& Collection)
{
	Collection.Empty();
}

#undef LOCTEXT_NAMESPACE
