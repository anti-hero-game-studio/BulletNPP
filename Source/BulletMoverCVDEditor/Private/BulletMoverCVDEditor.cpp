// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverCVDEditor.h"

#include "BulletMoverCVDExtension.h"
#include "ExtensionsSystem/ChaosVDExtensionsManager.h"

#define LOCTEXT_NAMESPACE "FBulletMoverCVDEditorModule"

void FBulletMoverCVDEditorModule::StartupModule()
{
	TSharedRef<FBulletMoverCVDExtension> NewExtension = MakeShared<FBulletMoverCVDExtension>();
	FChaosVDExtensionsManager::Get().RegisterExtension(NewExtension);
	AvailableExtensions.Add(NewExtension);
}

void FBulletMoverCVDEditorModule::ShutdownModule()
{
	for (const TWeakPtr<FChaosVDExtension>& Extension : AvailableExtensions)
	{
		if (const TSharedPtr<FChaosVDExtension>& ExtensionPtr = Extension.Pin())
		{
			FChaosVDExtensionsManager::Get().UnRegisterExtension(ExtensionPtr.ToSharedRef());
		}
	}

	AvailableExtensions.Reset();
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FBulletMoverCVDEditorModule, BulletMoverCVDEditor)