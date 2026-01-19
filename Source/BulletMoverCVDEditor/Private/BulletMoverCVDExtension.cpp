// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverCVDExtension.h"
#include "BulletMoverCVDSimDataComponent.h"
#include "BulletMoverCVDSimDataProcessor.h"
#include "BulletMoverCVDTab.h"
#include "BulletMoverCVDStyle.h"
#include "Widgets/SChaosVDMainTab.h"

namespace NBulletMoverCVDExtension
{
	static const FName BulletMoverTabName = FName(TEXT("BulletMover Info"));
	static const FName ExtensionName = FName(TEXT("FBulletMoverCVDExtension"));
};

FBulletMoverCVDExtension::FBulletMoverCVDExtension() : FChaosVDExtension()
{
	DataComponentsClasses.Add(UBulletMoverCVDSimDataComponent::StaticClass());

	ExtensionName = NBulletMoverCVDExtension::ExtensionName;

	FBulletMoverCVDStyle::Initialize();
}

FBulletMoverCVDExtension::~FBulletMoverCVDExtension()
{
	DataComponentsClasses.Reset();

	FBulletMoverCVDStyle::Shutdown();
}

void FBulletMoverCVDExtension::RegisterDataProcessorsInstancesForProvider(const TSharedRef<FChaosVDTraceProvider>& InTraceProvider)
{
	FChaosVDExtension::RegisterDataProcessorsInstancesForProvider(InTraceProvider);

    TSharedPtr<FBulletMoverCVDSimDataProcessor> SimDataProcessor = MakeShared<FBulletMoverCVDSimDataProcessor>();
    SimDataProcessor->SetTraceProvider(InTraceProvider);
    InTraceProvider->RegisterDataProcessor(SimDataProcessor);
}

TConstArrayView<TSubclassOf<UActorComponent>> FBulletMoverCVDExtension::GetSolverDataComponentsClasses()
{
	return DataComponentsClasses;
}

void FBulletMoverCVDExtension::RegisterCustomTabSpawners(const TSharedRef<SChaosVDMainTab>& InParentTabWidget)
{
	InParentTabWidget->RegisterTabSpawner<FBulletMoverCVDTab>(NBulletMoverCVDExtension::BulletMoverTabName);
}
