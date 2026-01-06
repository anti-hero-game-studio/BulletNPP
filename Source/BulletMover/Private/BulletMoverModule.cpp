// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverModule.h"

#include "Debug/BulletMoverDebugComponent.h"
#include "HAL/ConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#include "Debug/GameplayDebuggerCategory_BulletMover.h"
#define BULLET_MOVER_CATEGORY_NAME "BulletMover"
#endif // WITH_GAMEPLAY_DEBUGGER

#define LOCTEXT_NAMESPACE "FBulletMoverModule"


namespace UE::BulletMover
{
	int32 DisableDataCopyInPlace = 0;
	static FAutoConsoleVariableRef CVarDisableDataCopyInPlace(
		TEXT("bullet.mover.debug.DisableDataCopyInPlace"),
		DisableDataCopyInPlace,
		TEXT("Whether to allow Mover data collections with identical contained struct types to be copied in place, avoiding reallocating memory"),
		ECVF_Default);
}


void FBulletMoverModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
	TEXT("BulletMover.LocalPlayer.ShowTrail"),
	TEXT("Toggles showing the players trail according to the mover component. Trail will show previous path and some information on rollbacks. NOTE: this is applied the first local player controller."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FBulletMoverModule::ShowTrail),
	ECVF_Cheat
	));
	
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
	TEXT("BulletMover.LocalPlayer.ShowTrajectory"),
	TEXT("Toggles showing the players trajectory according to the mover component. NOTE: this is applied the first local player controller"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FBulletMoverModule::ShowTrajectory),
	ECVF_Cheat
	));
	
	ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
	TEXT("BulletMover.LocalPlayer.ShowCorrections"),
	TEXT("Toggles showing corrections that were applied to the actor. Green is the updated position after correction, Red was the position before correction. NOTE: this is applied the first local player controller."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(this, &FBulletMoverModule::ShowCorrections),
	ECVF_Cheat
	));
	
#if WITH_GAMEPLAY_DEBUGGER
	IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
	GameplayDebuggerModule.RegisterCategory(BULLET_MOVER_CATEGORY_NAME, IGameplayDebugger::FOnGetCategory::CreateStatic(&FGameplayDebuggerCategory_BulletMover::MakeInstance));
	GameplayDebuggerModule.NotifyCategoriesChanged();
#endif // WITH_GAMEPLAY_DEBUGGER
}

void FBulletMoverModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

#if WITH_GAMEPLAY_DEBUGGER
	if (IGameplayDebugger::IsAvailable())
	{
		IGameplayDebugger& GameplayDebuggerModule = IGameplayDebugger::Get();
		GameplayDebuggerModule.UnregisterCategory(BULLET_MOVER_CATEGORY_NAME);
		GameplayDebuggerModule.NotifyCategoriesChanged();
	}
#endif // WITH_GAMEPLAY_DEBUGGER
}

void FBulletMoverModule::ShowTrajectory(const TArray<FString>& Args, UWorld* World)
{
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		APawn* MyPawn = PC->GetPawn();
		if (UBulletMoverDebugComponent* MoverDebugComponent = MyPawn ? Cast<UBulletMoverDebugComponent>(MyPawn->GetComponentByClass(UBulletMoverDebugComponent::StaticClass())) : nullptr)
		{
			MoverDebugComponent->bShowTrajectory = !MoverDebugComponent->bShowTrajectory;
		}
		else
		{
			UBulletMoverDebugComponent* NewMoverDebugComponent = Cast<UBulletMoverDebugComponent>(MyPawn->AddComponentByClass(UBulletMoverDebugComponent::StaticClass(), false, FTransform::Identity, false));
			NewMoverDebugComponent->bShowTrajectory = true;
			NewMoverDebugComponent->bShowTrail = false;
			NewMoverDebugComponent->bShowCorrections = false;
			NewMoverDebugComponent->SetHistoryTracking(1.0f, 20.0f);
		}
	}
}
void FBulletMoverModule::ShowTrail(const TArray<FString>& Args, UWorld* World)
{
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		APawn* MyPawn = PC->GetPawn();
		if (UBulletMoverDebugComponent* MoverDebugComponent = MyPawn ? Cast<UBulletMoverDebugComponent>(MyPawn->GetComponentByClass(UBulletMoverDebugComponent::StaticClass())) : nullptr)
		{
			MoverDebugComponent->bShowTrail = !MoverDebugComponent->bShowTrail;
		}
		else
		{
			UBulletMoverDebugComponent* NewMoverDebugComponent = Cast<UBulletMoverDebugComponent>(MyPawn->AddComponentByClass(UBulletMoverDebugComponent::StaticClass(), false, FTransform::Identity, false));
			NewMoverDebugComponent->bShowTrail = true;
			NewMoverDebugComponent->bShowTrajectory = false;
			NewMoverDebugComponent->bShowCorrections = false;
			NewMoverDebugComponent->SetHistoryTracking(1.0f, 20.0f);
		}
	}
}

void FBulletMoverModule::ShowCorrections(const TArray<FString>& Args, UWorld* World)
{
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		APawn* MyPawn = PC->GetPawn();
		if (UBulletMoverDebugComponent* MoverDebugComponent = MyPawn ? Cast<UBulletMoverDebugComponent>(MyPawn->GetComponentByClass(UBulletMoverDebugComponent::StaticClass())) : nullptr)
		{
			MoverDebugComponent->bShowCorrections = !MoverDebugComponent->bShowCorrections;
		}
		else
		{
			UBulletMoverDebugComponent* NewMoverDebugComponent = Cast<UBulletMoverDebugComponent>(MyPawn->AddComponentByClass(UBulletMoverDebugComponent::StaticClass(), false, FTransform::Identity, false));
			NewMoverDebugComponent->bShowTrail = false;
			NewMoverDebugComponent->bShowTrajectory = false;
			NewMoverDebugComponent->bShowCorrections = true;
			NewMoverDebugComponent->SetHistoryTracking(1.0f, 20.0f);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef BULLET_MOVER_CATEGORY_NAME
	
IMPLEMENT_MODULE(FBulletMoverModule, BulletMover)