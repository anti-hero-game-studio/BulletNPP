// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HAL/Platform.h"

// Enum to identify the state types
enum class EBulletNetworkPredictionStateType : uint8
{
	Input,
	Sync,
	Aux
};

inline const TCHAR* LexToString(EBulletNetworkPredictionStateType A)
{
	switch(A)
	{
		case EBulletNetworkPredictionStateType::Input: return TEXT("Input");
		case EBulletNetworkPredictionStateType::Sync: return TEXT("Sync");
		case EBulletNetworkPredictionStateType::Aux: return TEXT("Aux");
	};
	return TEXT("Unknown");
}

// State type defines
template<typename InInputCmd=void, typename InSyncState=void, typename InAuxState=void>
struct TBulletNetworkPredictionStateTypes
{
	using InputType = InInputCmd;
	using SyncType = InSyncState;
	using AuxType = InAuxState;
};

// Tuple of state types
template<typename StateType=TBulletNetworkPredictionStateTypes<>>
struct TBulletNetworkPredictionState
{
	using InputType = typename StateType::InputType;
	using SyncType = typename StateType::SyncType;
	using AuxType = typename StateType::AuxType;

	const InputType* Cmd;
	const SyncType* Sync;
	const AuxType* Aux;

	TBulletNetworkPredictionState(const InputType* InInputCmd, const SyncType* InSync, const AuxType* InAux)
		: Cmd(InInputCmd), Sync(InSync), Aux(InAux) { }	

	// Allows implicit downcasting to a parent simulation's types
	template<typename T>
	TBulletNetworkPredictionState(const TBulletNetworkPredictionState<T>& Other)
		: Cmd(Other.Cmd), Sync(Other.Sync), Aux(Other.Aux) { }
};

// Just the Sync/Aux pair
template<typename StateType=TBulletNetworkPredictionStateTypes<>>
struct TBulletSyncAuxPair
{
	using SyncType = typename StateType::SyncType;
	using AuxType = typename StateType::AuxType;

	const SyncType* Sync;
	const AuxType* Aux;

	TBulletSyncAuxPair(const SyncType* InSync, const AuxType* InAux)
		: Sync(InSync), Aux(InAux) { }	

	// Allows implicit downcasting to a parent simulation's types
	template<typename T>
	TBulletSyncAuxPair(const TBulletSyncAuxPair<T>& Other)
		: Sync(Other.Sync), Aux(Other.Aux) { }
};
