// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backends/BulletMoverNetworkPredictionLiaison.h"
#include "BulletNetworkPredictionModelDefRegistry.h"
#include "BulletNetworkPredictionProxyInit.h"
#include "BulletNetworkPredictionProxyWrite.h"
#include "GameFramework/Actor.h"
#include "BulletMoverComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMoverNetworkPredictionLiaison)


#define LOCTEXT_NAMESPACE "BulletMover"

// ----------------------------------------------------------------------------------------------------------
//	FBulletMoverActorModelDef: the piece that ties everything together that we use to register with the NP system.
// ----------------------------------------------------------------------------------------------------------

class FBulletMoverActorModelDef : public FBulletNetworkPredictionModelDef
{
public:

	JNP_MODEL_BODY();

	using Simulation = UBulletMoverNetworkPredictionLiaisonComponent;
	using StateTypes = KinematicMoverStateTypes;
	using Driver = UBulletMoverNetworkPredictionLiaisonComponent;

	static const TCHAR* GetName() { return TEXT("BulletMoverActor"); }
	static constexpr int32 GetSortPriority() { return (int32)EBulletNetworkPredictionSortPriority::PreKinematicMovers; }
};

JNP_MODEL_REGISTER(FBulletMoverActorModelDef);



UBulletMoverNetworkPredictionLiaisonComponent::UBulletMoverNetworkPredictionLiaisonComponent()
{
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;

	bWantsInitializeComponent = true;
	bAutoActivate = true;
	SetIsReplicatedByDefault(true);
}

void UBulletMoverNetworkPredictionLiaisonComponent::ProduceInput(const int32 DeltaTimeMS, FBulletMoverInputCmdContext* Cmd)
{
	check(MoverComp);
	MoverComp->ProduceInput(DeltaTimeMS, Cmd);
}

void UBulletMoverNetworkPredictionLiaisonComponent::RestoreFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState)
{
	check(MoverComp);

	int32 NewBaseSimTimeMs = 0;
	int32 NextFrameNum = 0;

	switch (UBulletNetworkPredictionWorldManager::ActiveInstance->PreferredDefaultTickingPolicy())
	{
		default:	// fall through
		case EBulletNetworkPredictionTickingPolicy::Fixed:
		{
			const FBulletFixedTickState& FixedTickState = UBulletNetworkPredictionWorldManager::ActiveInstance->GetFixedTickState();
			FBulletNetSimTimeStep TimeStep = FixedTickState.GetNextTimeStep();
			NewBaseSimTimeMs = TimeStep.TotalSimulationTime;
			NextFrameNum = TimeStep.Frame;
		}
		break; 

		case EBulletNetworkPredictionTickingPolicy::Independent:
		{
			const FBulletVariableTickState& VariableTickState = UBulletNetworkPredictionWorldManager::ActiveInstance->GetVariableTickState();
			const FBulletNetSimTimeStep NextVariableTimeStep = VariableTickState.GetNextTimeStep(VariableTickState.Frames[VariableTickState.ConfirmedFrame]);
			NewBaseSimTimeMs = NextVariableTimeStep.TotalSimulationTime;
			NextFrameNum = NextVariableTimeStep.Frame;

		}
		break;
	}

	FBulletMoverTimeStep MoverTimeStep;

	MoverTimeStep.ServerFrame = NextFrameNum;
	MoverTimeStep.BaseSimTimeMs = NewBaseSimTimeMs;
	MoverTimeStep.StepMs = 0;

	MoverComp->RestoreFrame(SyncState, AuxState, MoverTimeStep);
}

void UBulletMoverNetworkPredictionLiaisonComponent::RestorePhysicsFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState)
{
	// I believe this only needs to happen once on the first rollback frame.
	//TODO:@GreggoryAddison::CodeCompletion || This should set the physics state of all mover bodies back to their authoritative state. Static colliders don't need to be reset
}

void UBulletMoverNetworkPredictionLiaisonComponent::FinalizeFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState)
{
	check(MoverComp);

	const FBulletNetworkPredictionSettings NetworkPredictionSettings = UBulletNetworkPredictionWorldManager::ActiveInstance->GetSettings();
	if (MoverComp->GetOwnerRole() == ROLE_SimulatedProxy && NetworkPredictionSettings.SimulatedProxyNetworkLOD == EBulletNetworkLOD::Interpolated)
	{
		FBulletMoverInputCmdContext InputCmd;
		MoverComp->TickInterpolatedSimProxy(MoverComp->GetLastTimeStep(), InputCmd, MoverComp, MoverComp->GetSyncState(), *SyncState, *AuxState);
	}
	
	MoverComp->FinalizeFrame(SyncState, AuxState);
}

void UBulletMoverNetworkPredictionLiaisonComponent::FinalizeSmoothingFrame(const FBulletMoverSyncState* SyncState, const FBulletMoverAuxStateContext* AuxState)
{
	check(MoverComp);
	MoverComp->FinalizeSmoothingFrame(SyncState, AuxState);
}

void UBulletMoverNetworkPredictionLiaisonComponent::InitializeSimulationState(FBulletMoverSyncState* OutSync, FBulletMoverAuxStateContext* OutAux)
{
	check(MoverComp);
	StartingOutSync = OutSync;
	StartingOutAux = OutAux;
	MoverComp->InitializeSimulationState(StartingOutSync, StartingOutAux);
}

void UBulletMoverNetworkPredictionLiaisonComponent::SimulationTick(const FBulletNetSimTimeStep& TimeStep, const TBulletNetSimInput<KinematicMoverStateTypes>& SimInput, const TBulletNetSimOutput<KinematicMoverStateTypes>& SimOutput)
{
	check(MoverComp);

	FBulletMoverTickStartData StartData;
	FBulletMoverTickEndData EndData;

	StartData.InputCmd  = *SimInput.Cmd;
	StartData.SyncState = *SimInput.Sync;
	StartData.AuxState  = *SimInput.Aux;

	// Ensure persistent SyncStates are present in the start state for a SimTick.
	for (const FBulletMoverDataPersistence& PersistentSyncEntry : MoverComp->PersistentSyncStateDataTypes)
	{
		StartData.SyncState.Collection.FindOrAddDataByType(PersistentSyncEntry.RequiredType);
	}
	
	FBulletMoverTimeStep MoverTimeStep;

	MoverTimeStep.ServerFrame	= TimeStep.Frame;
	MoverTimeStep.BaseSimTimeMs = TimeStep.TotalSimulationTime;
	MoverTimeStep.StepMs		= TimeStep.StepMS;

	MoverComp->SimulationTick(MoverTimeStep, StartData, OUT EndData);

	*SimOutput.Sync = EndData.SyncState;
    *SimOutput.Aux.Get() = EndData.AuxState;
}

void UBulletMoverNetworkPredictionLiaisonComponent::PostPhysicsTick(const FBulletNetSimTimeStep& TimeStep, const TBulletNetSimInput<KinematicMoverStateTypes>& SimInput, const TBulletNetSimOutput<KinematicMoverStateTypes>& SimOutput)
{
	check(MoverComp);

	FBulletMoverTickEndData EndData;
	
	EndData.AuxState = *SimOutput.Aux.Get();
	EndData.SyncState = *SimOutput.Sync;
	
	MoverComp->PostPhysicsTick(OUT EndData);

	*SimOutput.Sync = EndData.SyncState;
	*SimOutput.Aux.Get() = EndData.AuxState;
}


double UBulletMoverNetworkPredictionLiaisonComponent::GetCurrentSimTimeMs()
{
	return NetworkPredictionProxy.GetTotalSimTimeMS();
}

int32 UBulletMoverNetworkPredictionLiaisonComponent::GetCurrentSimFrame()
{
	return NetworkPredictionProxy.GetPendingFrame();
}


bool UBulletMoverNetworkPredictionLiaisonComponent::ReadPendingSyncState(OUT FBulletMoverSyncState& OutSyncState)
{
	if (const FBulletMoverSyncState* PendingSyncState = NetworkPredictionProxy.ReadSyncState<FBulletMoverSyncState>(EBulletNetworkPredictionStateRead::Simulation))
	{
		OutSyncState = *PendingSyncState;
		return true;
	}

	return false;
}

bool UBulletMoverNetworkPredictionLiaisonComponent::WritePendingSyncState(const FBulletMoverSyncState& SyncStateToWrite)
{
	bool bDidWriteSucceed = NetworkPredictionProxy.WriteSyncState<FBulletMoverSyncState>([&SyncStateToWrite](FBulletMoverSyncState& PendingSyncStateRef)
		{
			PendingSyncStateRef = SyncStateToWrite;
		}) != nullptr;

	return bDidWriteSucceed;
}


bool UBulletMoverNetworkPredictionLiaisonComponent::ReadPresentationSyncState(OUT FBulletMoverSyncState& OutSyncState)
{
	if (const FBulletMoverSyncState* PendingSyncState = NetworkPredictionProxy.ReadSyncState<FBulletMoverSyncState>(EBulletNetworkPredictionStateRead::Presentation))
	{
		OutSyncState = *PendingSyncState;
		return true;
	}

	return false;
}


bool UBulletMoverNetworkPredictionLiaisonComponent::WritePresentationSyncState(const FBulletMoverSyncState& SyncStateToWrite)
{
	bool bDidWriteSucceed = NetworkPredictionProxy.WritePresentationSyncState<FBulletMoverSyncState>([&SyncStateToWrite](FBulletMoverSyncState& PresentationSyncStateRef)
		{
			PresentationSyncStateRef = SyncStateToWrite;
		}) != nullptr;

	return bDidWriteSucceed;
}


bool UBulletMoverNetworkPredictionLiaisonComponent::ReadPrevPresentationSyncState(FBulletMoverSyncState& OutSyncState)
{
	if (const FBulletMoverSyncState* PrevPresentationSyncState = NetworkPredictionProxy.ReadPrevPresentationSyncState<FBulletMoverSyncState>())
	{
		OutSyncState = *PrevPresentationSyncState;
		return true;
	}

	return false;
}


bool UBulletMoverNetworkPredictionLiaisonComponent::WritePrevPresentationSyncState(const FBulletMoverSyncState& SyncStateToWrite)
{
	bool bDidWriteSucceed = NetworkPredictionProxy.WritePrevPresentationSyncState<FBulletMoverSyncState>([&SyncStateToWrite](FBulletMoverSyncState& PresentationSyncStateRef)
		{
			PresentationSyncStateRef = SyncStateToWrite;
		}) != nullptr;

	return bDidWriteSucceed;
}


#if WITH_EDITOR
EDataValidationResult UBulletMoverNetworkPredictionLiaisonComponent::ValidateData(FDataValidationContext& Context, const UBulletMoverComponent& ValidationMoverComp) const
{
	if (const AActor* OwnerActor = ValidationMoverComp.GetOwner())
	{
		if (OwnerActor->IsReplicatingMovement())
		{
			Context.AddError(FText::Format(LOCTEXT("ConflictingReplicateMovementProperty", "The owning actor ({0}) has the ReplicateMovement property enabled. This will conflict with Network Prediction and cause poor quality movement. Please disable it."),
				FText::FromString(GetNameSafe(OwnerActor))));

			return EDataValidationResult::Invalid;
		}
	}

	return EDataValidationResult::Valid;
}
#endif // WITH_EDITOR

void UBulletMoverNetworkPredictionLiaisonComponent::BeginPlay()
{
	Super::BeginPlay();

	if (StartingOutSync && StartingOutAux)
	{
		if (FBulletMoverDefaultSyncState* StartingSyncState = StartingOutSync->Collection.FindMutableDataByType<FBulletMoverDefaultSyncState>())
		{
			const FTransform UpdatedComponentTransform = MoverComp->GetUpdatedComponentTransform();
			// if our location has changed between initialization and begin play (ex: Actors sharing an exact start location and one gets "pushed" to make them fit) lets write the new location to avoid any disagreements
			if (!UpdatedComponentTransform.GetLocation().Equals(StartingSyncState->GetLocation_WorldSpace()))
			{
				StartingSyncState->SetTransforms_WorldSpace(UpdatedComponentTransform.GetLocation(),
													 UpdatedComponentTransform.GetRotation().Rotator(),
													 FVector::ZeroVector,
													 FVector::ZeroVector);	// no initial velocity
			}
		}
	}
}


void UBulletMoverNetworkPredictionLiaisonComponent::InitializeComponent()
{
	Super::InitializeComponent();
}


void UBulletMoverNetworkPredictionLiaisonComponent::UninitializeComponent()
{
	NetworkPredictionProxy.EndPlay();

	Super::UninitializeComponent();
}

void UBulletMoverNetworkPredictionLiaisonComponent::OnRegister()
{
	Super::OnRegister();
}


void UBulletMoverNetworkPredictionLiaisonComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);
}

void UBulletMoverNetworkPredictionLiaisonComponent::InitializeNetworkPredictionProxy()
{
	MoverComp = GetOwner()->FindComponentByClass<UBulletMoverComponent>();


	if (ensureAlwaysMsgf(MoverComp, TEXT("UBulletMoverNetworkPredictionLiaisonComponent on actor %s failed to find associated Mover component. This actor's movement will not be simulated. Verify its setup."), *GetNameSafe(GetOwner())))
	{
		NetworkPredictionProxy.Init<FBulletMoverActorModelDef>(GetWorld(), GetReplicationProxies(), this, this);
	}
}

#undef LOCTEXT_NAMESPACE
