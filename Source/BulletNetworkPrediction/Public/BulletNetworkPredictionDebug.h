// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Math/Box.h"
#include "Misc/NetworkGuid.h"

class FNetworkGUID;

// non ModelDef specific debug helpers
namespace BulletNetworkPredictionDebug
{
	BULLETNETWORKPREDICTION_API void DrawDebugOutline(FTransform Transform, FBox BoundingBox, FColor Color, float Lifetime);
	BULLETNETWORKPREDICTION_API void DrawDebugText3D(const TCHAR* Str, FTransform Transform, FColor, float Lifetime);
	BULLETNETWORKPREDICTION_API UObject* FindReplicatedObjectOnPIEServer(UObject* ClientObject);
	BULLETNETWORKPREDICTION_API FNetworkGUID FindObjectNetGUID(UObject* Obj);
};
