// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BulletNetworkPredictionCheck.h"
#include "UObject/NameTypes.h"

namespace UE_NP
{
#ifndef BNP_MAX_ASYNC_MODEL_DEFS
#define BNP_MAX_ASYNC_MODEL_DEFS 16
#endif

	const int32 MaxAsyncModelDefs = BNP_MAX_ASYNC_MODEL_DEFS;

#ifndef BNP_NUM_FRAME_STORAGE
#define BNP_NUM_FRAME_STORAGE 64
#endif

	const int32 NumFramesStorage = BNP_NUM_FRAME_STORAGE;

#ifndef BNP_FRAME_STORAGE_GROWTH
#define BNP_FRAME_STORAGE_GROWTH 8
#endif

	const int32 FrameStorageGrowth = BNP_FRAME_STORAGE_GROWTH;

#ifndef BNP_FRAME_INPUTCMD_BUFFER_SIZE
#define BNP_FRAME_INPUTCMD_BUFFER_SIZE 16
#endif

	const int32 InputCmdBufferSize = BNP_FRAME_INPUTCMD_BUFFER_SIZE;

#ifndef BNP_INLINE_SIMOBJ_INPUTS
#define BNP_INLINE_SIMOBJ_INPUTS 3
#endif

	const int32 InlineSimObjInputs = BNP_INLINE_SIMOBJ_INPUTS;

};

// Sets index to value, resizing bit array if necessary and setting new bits to false
template<typename BitArrayType>
void BnpResizeAndSetBit(BitArrayType& BitArray, int32 Index, bool Value=true)
{
	if (!BitArray.IsValidIndex(Index))
	{
		const int32 PreNum = BitArray.Num();
		BitArray.SetNumUninitialized(Index+1);
		BitArray.SetRange(PreNum, BitArray.Num() - PreNum, false);
		bnpCheckSlow(BitArray.IsValidIndex(Index));
	}

	BitArray[Index] = Value;
}

// Resize BitArray to NewNum, setting default value of new bits to false
template<typename BitArrayType>
void BnpResizeBitArray(BitArrayType& BitArray, int32 NewNum)
{
	if (BitArray.Num() < NewNum)
	{
		const int32 PreNum = BitArray.Num();
		BitArray.SetNumUninitialized(NewNum);
		BitArray.SetRange(PreNum, BitArray.Num() - PreNum, false);
		bnpCheckSlow(BitArray.Num() == NewNum);
	}
}

// Set bit array contents to false
template<typename BitArrayType>
void BnpClearBitArray(BitArrayType& BitArray)
{
	BitArray.SetRange(0, BitArray.Num(), false);
}

template<typename ArrayType>
void BnpResizeForIndex(ArrayType& Array, int32 Index)
{
	bnpEnsure(Index >= 0);
	if (Array.IsValidIndex(Index) == false)
	{
		Array.SetNum(Index + UE_NP::FrameStorageGrowth);
	}
}

class AActor;
