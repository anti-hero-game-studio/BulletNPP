// Copyright Epic Games, Inc. All Rights Reserved.

#include "Bullet.h"

#include "BulletCoreSettings.h"
#include "ISettingsModule.h"

#define LOCTEXT_NAMESPACE "FBulletNPPModule"


namespace BulletPhysicsEngine
{
	int32 DisableDataCopyInPlace = 0;
	static FAutoConsoleVariableRef CVarDisableDataCopyInPlace(
		TEXT("bullet.debug.DisableDataCopyInPlace"),
		DisableDataCopyInPlace,
		TEXT("Whether to allow Bullet data collections with identical contained struct types to be copied in place, avoiding reallocating memory"),
		ECVF_Default);
}


void FBulletModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->RegisterSettings("Project", "Project", "Bullet",
			LOCTEXT("BulletNetworkPredictionSettingsName", "Bullet"),
			LOCTEXT("BulletNetworkPredictionSettingsDescription", "Settings for the Bullet Physics runtime module."),
			GetMutableDefault<UBulletCoreSettings>()
		);
	}
}

void FBulletModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
#if WITH_EDITOR

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Project", "Bullet");
	}
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FBulletModule, Bullet)