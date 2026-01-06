// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BulletNetworkPredictionStateTypes.h"

template <typename ElementType> struct TBulletNetSimLazyWriter;

struct FBulletNetSimCueDispatcher;

// Input state is just a collection of references to the simulation state types
template<typename StateTypes=TBulletNetworkPredictionStateTypes<>>
using TBulletNetSimInput = TBulletNetworkPredictionState<StateTypes>;

// Output state: the output SyncState (always created) and TBulletNetSimLazyWriter for the AuxState (created on demand since every tick does not generate a new aux frame)
template<typename StateType=TBulletNetworkPredictionStateTypes<>>
struct TBulletNetSimOutput
{
	using InputType = typename StateType::InputType;
	using SyncType = typename StateType::SyncType;
	using AuxType = typename StateType::AuxType;

	SyncType* Sync;
	const TBulletNetSimLazyWriter<AuxType>& Aux;
	FBulletNetSimCueDispatcher& CueDispatch;

	TBulletNetSimOutput(SyncType* InSync, const TBulletNetSimLazyWriter<AuxType>& InAux, FBulletNetSimCueDispatcher& InCueDispatch)
		: Sync(InSync), Aux(InAux), CueDispatch(InCueDispatch) { }
};

