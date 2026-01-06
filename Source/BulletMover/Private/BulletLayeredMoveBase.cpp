// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletLayeredMoveBase.h"
#include "BulletMoverLog.h"
#include "BulletMoverComponent.h"
#include "BulletMoverSimulationTypes.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "MoveLibrary/BulletMovementMixer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletLayeredMoveBase)

#define LOCTEXT_NAMESPACE "BulletMover"

//////////////////////////////////////////////////////////////////////////
// FBulletLayeredMoveInstancedData

bool FBulletLayeredMoveInstancedData::operator==(const FBulletLayeredMoveInstancedData& Other) const
{
	//@todo DanH: Compare logic class compatibility?
	if (GetScriptStruct() == Other.GetScriptStruct())
	{
		return Equals(Other);
	}
	return false;
}

bool FBulletLayeredMoveInstancedData::Equals(const FBulletLayeredMoveInstancedData& OtherData) const
{
	return StartSimTimeMs == OtherData.StartSimTimeMs
		&& DurationMs == OtherData.DurationMs;
}

void FBulletLayeredMoveInstancedData::ActivateFromContext(const FBulletLayeredMoveActivationParams* ActivationParams)
{
	
}

void FBulletLayeredMoveInstancedData::NetSerialize(FArchive& Ar)
{
	Ar << StartSimTimeMs;
	Ar << DurationMs;
}

//////////////////////////////////////////////////////////////////////////
// UBulletLayeredMoveLogic

UBulletLayeredMoveLogic::UBulletLayeredMoveLogic()
	: InstancedDataStructType(FBulletLayeredMoveInstancedData::StaticStruct())
{
}

bool UBulletLayeredMoveLogic::IsFinished_Implementation(const FBulletMoverTimeStep& TimeStep, const UBulletMoverBlackboard* SimBlackboard)
{
	const FBulletLayeredMoveInstancedData& ExecData = AccessExecutionMoveData();
	const double DurationMs = ExecData.DurationMs;
	if (DurationMs < 0.f)
	{
		return false;
	}
	
	const double StartSimTimeMs = ExecData.GetStartSimTimeMs();
	
	const bool bHasStarted = StartSimTimeMs >= 0.f;
	const bool bTimeExpired = StartSimTimeMs + DurationMs <= TimeStep.BaseSimTimeMs;
	return bHasStarted && bTimeExpired;	
}

bool UBulletLayeredMoveLogic::K2_GetActiveMoveData(UBulletLayeredMoveLogic* MoveLogic, FBulletLayeredMoveInstancedData& OutMoveData)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();
	return false;
}


DEFINE_FUNCTION(UBulletLayeredMoveLogic::execK2_GetActiveMoveData)
{
	P_GET_OBJECT(UBulletLayeredMoveLogic, MoveLogic);
	
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* MoveDataProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	uint8* OutMoveDataPtr = Stack.MostRecentPropertyAddress;
	
	P_FINISH

	const bool bHasValidMoveData = ValidateMoveDataGetSet(P_THIS, MoveLogic, MoveDataProperty, OutMoveDataPtr, Stack);
	if (bHasValidMoveData)
	{
		// Write the active move data to the function output
		P_NATIVE_BEGIN;
		MoveDataProperty->Struct->CopyScriptStruct(OutMoveDataPtr, MoveLogic->CurrentInstancedData.Get());
		P_NATIVE_END;
	}
	
	*(bool*)RESULT_PARAM = bHasValidMoveData;
}

void UBulletLayeredMoveLogic::K2_SetActiveMoveData(UBulletLayeredMoveLogic* MoveLogic, const FBulletLayeredMoveInstancedData& OutMoveData)
{
	// This will never be called, the exec version below will be hit instead
	checkNoEntry();
}

DEFINE_FUNCTION(UBulletLayeredMoveLogic::execK2_SetActiveMoveData)
{
	P_GET_OBJECT(UBulletLayeredMoveLogic, MoveLogic);
	
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* MoveDataProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
	const uint8* MoveDataPtr = Stack.MostRecentPropertyAddress;

	P_FINISH

	if (ValidateMoveDataGetSet(P_THIS, MoveLogic, MoveDataProperty, MoveDataPtr, Stack))
	{
		// Overwrite the contents of the current move data with that provided
		P_NATIVE_BEGIN;
		MoveDataProperty->Struct->CopyScriptStruct(MoveLogic->CurrentInstancedData.Get(), MoveDataPtr);
		P_NATIVE_END;
	}
}

FBulletLayeredMoveInstance::FBulletLayeredMoveInstance()
	: InstanceMoveData(nullptr)
	, MoveLogic(nullptr)
{
}

FBulletLayeredMoveInstance::FBulletLayeredMoveInstance(const FBulletLayeredMoveInstance& OtherActiveLayeredMove)
	: InstanceMoveData(OtherActiveLayeredMove.InstanceMoveData)
	, MoveLogic(OtherActiveLayeredMove.MoveLogic)
{
}

FBulletLayeredMoveInstance::FBulletLayeredMoveInstance(const TSharedRef<FBulletLayeredMoveInstancedData>& InMoveData, UBulletLayeredMoveLogic* InMoveLogic)
	: InstanceMoveData(InMoveData)
	, MoveLogic(InMoveLogic)
{
}

bool UBulletLayeredMoveLogic::ValidateMoveDataGetSet(const UObject* ObjectValidatingData, const UBulletLayeredMoveLogic* MoveLogic, const FStructProperty* MoveDataProperty, const uint8* MoveDataPtr, FFrame& StackFrame)
{
	FText BlueprintExceptionText;
	bool bIsValid = true;
	if (!MoveLogic)
	{
		BlueprintExceptionText = LOCTEXT("ValidateMoveDataNoMoveLogic", "No MoveLogic was present.");
		bIsValid = false;
	}
	else if (!MoveLogic->CurrentInstancedData)
	{
		BlueprintExceptionText = LOCTEXT("ValidateMoveDataNoCurrentActiveMoveData", "No CurrentActiveMoveData on the MoveLogic was present.");
		bIsValid = false;
	}
	else if (!MoveDataProperty)
	{
		BlueprintExceptionText = LOCTEXT("ValidateMoveDataNoMoveData", "No MoveData was present.");
		bIsValid = false;
	}
	else if (!MoveDataPtr)
	{
		BlueprintExceptionText = LOCTEXT("ValidateMoveDataNoMoveData", "No MoveData was present.");
		bIsValid = false;
	}
	else if (MoveDataProperty->Struct != MoveLogic->InstancedDataStructType.Get())
	{
		BlueprintExceptionText = FText::Format(LOCTEXT("ValidateMoveDataMisMatchData", "MoveData passed in did not match MoveLogic Active mode data. Expected: {0}. Found: {1}."),
			FText::FromString(MoveDataProperty->Struct.GetName()),
			FText::FromString(MoveLogic->InstancedDataStructType.GetName())
		);
		bIsValid = false;
	}

	if (!bIsValid)
	{
		const FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::NonFatalError,
			BlueprintExceptionText
		);
		
		FBlueprintCoreDelegates::ThrowScriptException(ObjectValidatingData, StackFrame, ExceptionInfo);
	}
	
	return bIsValid;
}

//////////////////////////////////////////////////////////////////////////
// FBulletLayeredMoveInstance

/**
 * Scoped wrapper that is the only means of calling the virtual functions in UBulletLayeredMoveLogic
 * that depend on/expect access to valid active move data.
 */
class FScopedMoveLogicExecContext
{
public:	
	FScopedMoveLogicExecContext(UBulletLayeredMoveLogic& MoveLogic, const TSharedRef<FBulletLayeredMoveInstancedData>& MoveData)
		: LogicObj(MoveLogic)
	{
		MoveLogic.CurrentInstancedData = MoveData.ToSharedPtr();
	}

	template <typename... ArgsT>
    void OnStart(ArgsT&&... Args)
    {
    	LogicObj.OnStart(Args...);
    }
	
	template <typename... ArgsT>
	void OnEnd(ArgsT&&... Args)
	{
		LogicObj.OnEnd(Args...);
	}
	
	template <typename... ArgsT>
	bool GenerateMove(ArgsT&&... Args)
	{
		return LogicObj.GenerateMove(Args...);
	}

	template <typename... ArgsT>
	bool IsFinished(ArgsT&&... Args)
	{
		return LogicObj.IsFinished(Args...);
	}
	
	~FScopedMoveLogicExecContext()
	{
		LogicObj.CurrentInstancedData.Reset();
	}

private:
	UBulletLayeredMoveLogic& LogicObj;
};

void FBulletLayeredMoveInstance::StartMove(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard) const
{
	if (ensure(MoveLogic))
	{
		FScopedMoveLogicExecContext LogicExecContext(*MoveLogic, InstanceMoveData.ToSharedRef());
		LogicExecContext.OnStart(TimeStep, SimBlackboard);
		
		InstanceMoveData->StartSimTimeMs = TimeStep.BaseSimTimeMs;
	}
}

bool FBulletLayeredMoveInstance::GenerateMove(const FBulletMoverTickStartData& StartState, const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard, FBulletProposedMove& OutProposedMove) const
{
	if (ensure(MoveLogic))
	{
		FScopedMoveLogicExecContext LogicExecContext(*MoveLogic, InstanceMoveData.ToSharedRef());
		if (LogicExecContext.GenerateMove(TimeStep, SimBlackboard, StartState, OutProposedMove))
		{
			//@todo DanH: Feels like the preferred mode from moves should come from OnStart if that's the only time we listen to it
			// Wipe the preferred mode if this wasn't the first tick of the mode
			if (InstanceMoveData->GetStartSimTimeMs() < TimeStep.BaseSimTimeMs)
			{
				OutProposedMove.PreferredMode = NAME_None;
			}
			return true;
		}
	}
	
	return false;
}

void FBulletLayeredMoveInstance::EndMove(const FBulletMoverTimeStep& TimeStep, UBulletMoverBlackboard* SimBlackboard) const
{
	if (ensure(MoveLogic))
	{
		FScopedMoveLogicExecContext LogicExecContext(*MoveLogic, InstanceMoveData.ToSharedRef());
		LogicExecContext.OnEnd(TimeStep, SimBlackboard);
	}
}

bool FBulletLayeredMoveInstance::IsFinished(const FBulletMoverTimeStep& TimeStep, const UBulletMoverBlackboard* SimBlackboard) const
{
	if (ensure(MoveLogic))
	{
		FScopedMoveLogicExecContext LogicExecContext(*MoveLogic, InstanceMoveData.ToSharedRef());
		return LogicExecContext.IsFinished(TimeStep, SimBlackboard);
	}
	return true;
}

const FBulletLayeredMoveFinishVelocitySettings& FBulletLayeredMoveInstance::GetFinishVelocitySettings() const
{
	if (MoveLogic)
	{
		return MoveLogic->GetFinishVelocitySettings();
	}
	
	static const FBulletLayeredMoveFinishVelocitySettings Defaults;
	return Defaults;
}

EBulletMoveMixMode FBulletLayeredMoveInstance::GetMixMode() const
{
	if (MoveLogic)
	{
		return MoveLogic->GetMixMode();
	}
	return EBulletMoveMixMode::AdditiveVelocity;
}

FBulletLayeredMoveInstance FBulletLayeredMoveInstance::Clone() const
{
	return FBulletLayeredMoveInstance(TSharedRef<FBulletLayeredMoveInstancedData>(InstanceMoveData->Clone()), MoveLogic.Get());
}

void FBulletLayeredMoveInstance::NetSerialize(FArchive& Ar)
{
	// Step carefully! When writing from an archive, this move may have been created via TArray::AddZeroed, which does not call the constructor
	// Therefore, this is the one spot where we are actually ok with MoveData being null, but that violates the rules of TSharedRef.
	// So the ref is converted first to a TSharedPtr so we can safely, albeit cheekily, check it for validity.
	TSharedPtr<FBulletLayeredMoveInstancedData> PossiblyNullData = InstanceMoveData;
	TCheckedObjPtr<UScriptStruct> DataStructType = PossiblyNullData ? PossiblyNullData->GetScriptStruct() : nullptr;
	UScriptStruct* ExistingDataType = DataStructType.Get();
	Ar << DataStructType;

	UClass* CurrentMoveLogicClassType = MoveLogic ? MoveLogic->GetClass() : nullptr;
	
	Ar << CurrentMoveLogicClassType;
	if (!MoveLogic)
	{
		MoveLogicClassType = CurrentMoveLogicClassType;
	}
	
	if (DataStructType.IsValid())
	{
		//@todo DanH: Fwiw FBulletLayeredMoveInstanceGroup is not actually ever expected to be sent in a server RPC right?
		// Restrict replication to derived classes of FBulletLayeredMoveInstancedData for security reasons:
		// If FBulletLayeredMoveInstanceGroup is replicated through a Server RPC, we need to prevent clients from sending us
		// arbitrary ScriptStructs due to the allocation/reliance on GetCppStructOps below which could trigger a server crash
		// for invalid structs.
		if (DataStructType->IsChildOf(FBulletLayeredMoveInstancedData::StaticStruct()))
		{
			// If the struct type at this index should be different from the one that's already here, we need to change it
			if (DataStructType.Get() != ExistingDataType && ensure(Ar.IsLoading()))
			{
				//@todo DanH: There was a comment here about doing it this way "for now", but it was copy-pasted from FRootMotionSourceGroup::NetSerializeRMSArray. Is it valid?
				FBulletLayeredMoveInstancedData* NewMove = (FBulletLayeredMoveInstancedData*)FMemory::Malloc(DataStructType->GetCppStructOps()->GetSize());
				DataStructType->InitializeStruct(NewMove);

				struct FSerializationAllocatedLayeredMoveDataDeleter
				{
					FORCEINLINE void operator()(FBulletLayeredMoveInstancedData* MoveData) const
					{
						check(MoveData);
						UScriptStruct* ScriptStruct = MoveData->GetScriptStruct();
						check(ScriptStruct);
						ScriptStruct->DestroyStruct(MoveData);
						FMemory::Free(MoveData);
					}
				};
				InstanceMoveData = TSharedRef<FBulletLayeredMoveInstancedData>(NewMove, FSerializationAllocatedLayeredMoveDataDeleter());
			}

			InstanceMoveData->NetSerialize(Ar);
		}
		else
		{
			UE_LOG(LogBulletMover, Error, TEXT("FBulletLayeredMoveInstanceGroup::NetSerialize: ScriptStruct [%s] not derived from FBulletLayeredMoveInstancedData attempted to serialize."),
				*DataStructType->GetName());
			Ar.SetError();
		}
	}
	else if (DataStructType.IsError())
	{
		UE_LOG(LogBulletMover, Error, TEXT("FBulletLayeredMoveInstanceGroup::NetSerialize: Invalid ScriptStruct serialized."));
		Ar.SetError();
	}
}

const UClass* FBulletLayeredMoveInstance::GetSerializedMoveLogicClass() const
{
	return MoveLogicClassType;
}

bool FBulletLayeredMoveInstance::PopulateMissingActiveMoveLogic(const TArray<TObjectPtr<UBulletLayeredMoveLogic>>& RegisteredMoves)
{
	if (!HasLogic())
	{
		bool bFoundMissingLogic = false;
		
		if (MoveLogicClassType)
		{
			for (const TObjectPtr<UBulletLayeredMoveLogic>& RegisteredMoveLogic : RegisteredMoves)
			{
				if (RegisteredMoveLogic->GetClass() == MoveLogicClassType)
				{
					MoveLogic = RegisteredMoveLogic;
					
					bFoundMissingLogic = true;
					break;
				}
			}

			if (!bFoundMissingLogic)
			{
				UE_LOG(LogBulletMover, Warning, TEXT("Active Layered Move couldn't find it's serialized MoveLogicClass (%s) among registered MoveLogic"), *MoveLogicClassType->GetName());
			}
		}
		else
		{
			UE_LOG(LogBulletMover, Warning, TEXT("Active Layered Move didn't have a valid logic class or class type to search for"));
		}

		return bFoundMissingLogic;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
