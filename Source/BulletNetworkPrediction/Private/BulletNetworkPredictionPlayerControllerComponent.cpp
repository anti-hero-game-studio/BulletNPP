// 2025 Yohoho Productions /  Sirkai


#include "BulletNetworkPredictionPlayerControllerComponent.h"

#include "BulletNetworkPredictionWorldManager.h"


// Sets default values for this component's properties
UBulletNetworkPredictionPlayerControllerComponent::UBulletNetworkPredictionPlayerControllerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	// ...
}


// Called when the game starts
void UBulletNetworkPredictionPlayerControllerComponent::BeginPlay()
{
	Super::BeginPlay();
	// ...
	
}

void UBulletNetworkPredictionPlayerControllerComponent::OnRegister()
{
	Super::OnRegister();

	UBulletNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UBulletNetworkPredictionWorldManager>();
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());
	if (OwningPlayerController && Manager)
	{
		Manager->RegisterRPCHandler(this);
	}
}

void UBulletNetworkPredictionPlayerControllerComponent::OnUnregister()
{
	Super::OnUnregister();
	UBulletNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UBulletNetworkPredictionWorldManager>();
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());
	if (OwningPlayerController && Manager)
	{
		Manager->UnRegisterRPCHandler(this);
	}
}


// Called every frame
void UBulletNetworkPredictionPlayerControllerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// ...
}

void UBulletNetworkPredictionPlayerControllerComponent::GetLifetimeReplicatedProps(
	TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBulletNetworkPredictionPlayerControllerComponent, TimeDilation);
}

void UBulletNetworkPredictionPlayerControllerComponent::SendServerRpc(const int32& Frame)
{
	Server_ReceivedInput(Frame,InterpolationTimeMS,InputsToSend);
	InputsToSend.Reset();
}

void UBulletNetworkPredictionPlayerControllerComponent::SendAckedFrames(const FBulletSerializedAckedFrames& AckedFrames)
{
	Server_ReceivedAckedFrames(AckedFrames);
}

void UBulletNetworkPredictionPlayerControllerComponent::Server_ReceivedAckedFrames_Implementation(
	const FBulletSerializedAckedFrames& AckedFrames)
{
	UBulletNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UBulletNetworkPredictionWorldManager>();
	if (Manager)
	{
		Manager->OnReceivedAckedData(AckedFrames,this);
	}
}

void UBulletNetworkPredictionPlayerControllerComponent::Server_ReceivedInput_Implementation(const int32& Frame,const float& InInterpolationTime,
                                                                                      const TArray<FBulletSimulationReplicatedInput>& Inputs)
{
	UBulletNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UBulletNetworkPredictionWorldManager>();

	/*for (const FBulletSimulationReplicatedInput& Input : Inputs)
	{
		if (TFunction<void(
		const int32&, const float&, const FBulletSimulationReplicatedInput&,
		UBulletNetworkPredictionPlayerControllerComponent*, const FBulletFixedTickState& TickState)>* Receiver = InputReceivers.BoundReceivers.Find(Input.ID))
		{
			(*Receiver)(Frame, InterpolationTime, Input,Manager->GetFixedTickState());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No handler registered for Input ID %d"), Input.ID);
		}
	}*/
	if (Manager)
	{
		Manager->OnInputReceived(Frame,InInterpolationTime,Inputs,this);
	}
}


UNetConnection* UBulletNetworkPredictionPlayerControllerComponent::GetNetConnection() const
{
	if (APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner()))
	{
		return OwningPlayerController->NetConnection;
	}
	return nullptr;
}

void UBulletNetworkPredictionPlayerControllerComponent::UpdateTimeDilation(const float& InTimeDilation)
{
	if (GetOwner()->GetLocalRole() != ROLE_Authority)
	{
		return;
	}
	TimeDilation.UpdateTimeDilation(InTimeDilation);
}

void UBulletNetworkPredictionPlayerControllerComponent::OnRep_TimeDilation()
{
	if (GetOwner()->GetLocalRole() == ROLE_Authority)
	{
		return;
	}
	UBulletNetworkPredictionWorldManager* Manager = GetWorld()->GetSubsystem<UBulletNetworkPredictionWorldManager>();
	if (Manager)
	{
		Manager->SetTimeDilation(TimeDilation);
	}
}


void UBulletNetworkPredictionPlayerControllerComponent::AdvanceLastConsumedFrame(const int32& MaxBufferSize)
{
	if (LastReceivedFrame == INDEX_NONE)
	{
		return;
	}
	if (LastConsumedFrame >= LastReceivedFrame)
	{
		//ToDo Log input starvation.
		LastConsumedFrame = FMath::Max(LastReceivedFrame - 2,0);
		return;
	}
	if (LastReceivedFrame - LastConsumedFrame > FMath::Max(2,MaxBufferSize))
	{
		//ToDo Log Buffer Overflow
		LastConsumedFrame = LastReceivedFrame - 7;
		return;
	}
	LastConsumedFrame++;
}

void UBulletNetworkPredictionPlayerControllerComponent::AddInputToSend(const int32& ID, const uint32& DataSize,
	const TArray<uint8>& Data)
{
	InputsToSend.Add(FBulletSimulationReplicatedInput(ID, DataSize,Data));
}

