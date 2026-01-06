// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletNetworkPredictionCues.h"

DEFINE_LOG_CATEGORY(LogBulletNetworkPredictionCues);

FGlobalCueTypeTable FGlobalCueTypeTable::Singleton;

FGlobalCueTypeTable::FRegisteredCueTypeInfo& FGlobalCueTypeTable::GetRegistedTypeInfo()
{
	static FGlobalCueTypeTable::FRegisteredCueTypeInfo PendingCueTypes;
	return PendingCueTypes;
}
