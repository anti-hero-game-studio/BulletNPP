// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Build.h"

#ifndef BNP_ENSURES_ALWAYS
#define BNP_ENSURES_ALWAYS 0 
#endif

#define BNP_CHECKS_AND_ENSURES 1
#if BNP_CHECKS_AND_ENSURES
	#define bnpCheck(Condition) check(Condition)
	#define bnpCheckf(Condition, ...) checkf(Condition, ##__VA_ARGS__)
	#if BNP_ENSURES_ALWAYS
		#define bnpEnsure(Condition) ensureAlways(Condition)
		#define bnpEnsureMsgf(Condition, ...) ensureAlwaysMsgf(Condition, ##__VA_ARGS__)
	#else
		#define bnpEnsure(Condition) ensure(Condition)
		#define bnpEnsureMsgf(Condition, ...) ensureMsgf(Condition, ##__VA_ARGS__)
	#endif
#else
	#define bnpCheck(...)
	#define bnpCheckf(...)
	#define bnpEnsure(Condition) (!!(Condition))
	#define bnpEnsureMsgf(Condition, ...) (!!(Condition))
#endif

#define BNP_CHECKS_AND_ENSURES_SLOW !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#if BNP_CHECKS_AND_ENSURES_SLOW
	#define bnpCheckSlow(Condition) check(Condition)
	#define bnpCheckfSlow(Condition, ...) checkf(Condition, ##__VA_ARGS__)
	#if BNP_ENSURES_ALWAYS
		#define bnpEnsureSlow(Condition) ensureAlways(Condition)
		#define bnpEnsureMsgfSlow(Condition, ...) ensureAlwaysMsgf(Condition, ##__VA_ARGS__)
	#else
		#define bnpEnsureSlow(Condition) ensure(Condition)
		#define bnpEnsureMsgfSlow(Condition, ...) ensureMsgf(Condition, ##__VA_ARGS__)
	#endif
#else
	#define bnpCheckSlow(Condition)
	#define bnpCheckfSlow(...)
	#define bnpEnsureSlow(Condition) (!!(Condition))
	#define bnpEnsureMsgfSlow(Condition, ...) (!!(Condition))
#endif