// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverCVDSimDataSettings.h"

#include "ChaosVDSettingsManager.h"
#include "Utils/ChaosVDUserInterfaceUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BulletMoverCVDSimDataSettings)

void UBulletMoverCVDSimDataSettings::SetDataVisualizationFlags(EBulletMoverCVDSimDataVisualizationFlags NewFlags)
{
	if (UBulletMoverCVDSimDataSettings* Settings = FChaosVDSettingsManager::Get().GetSettingsObject<UBulletMoverCVDSimDataSettings>())
	{
		Settings->DebugDrawFlags = static_cast<uint32>(NewFlags);
		Settings->BroadcastSettingsChanged();
	}
}

EBulletMoverCVDSimDataVisualizationFlags UBulletMoverCVDSimDataSettings::GetDataVisualizationFlags()
{
	if (UBulletMoverCVDSimDataSettings* Settings = FChaosVDSettingsManager::Get().GetSettingsObject<UBulletMoverCVDSimDataSettings>())
	{
		return static_cast<EBulletMoverCVDSimDataVisualizationFlags>(Settings->DebugDrawFlags);
	}

	return EBulletMoverCVDSimDataVisualizationFlags::None;
}

bool UBulletMoverCVDSimDataSettings::CanVisualizationFlagBeChangedByUI(uint32 Flag)
{
	return Chaos::VisualDebugger::Utils::ShouldVisFlagBeEnabledInUI(Flag, DebugDrawFlags, EBulletMoverCVDSimDataVisualizationFlags::EnableDraw);
}
