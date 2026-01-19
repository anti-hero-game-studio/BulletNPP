// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverCVDSimDataProcessor.h"
#include "BulletMoverCVDDataWrappers.h"

FBulletMoverCVDSimDataProcessor::FBulletMoverCVDSimDataProcessor() : FChaosVDDataProcessorBase(FBulletMoverCVDSimDataWrapper::WrapperTypeName)
{
}

bool FBulletMoverCVDSimDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	FChaosVDDataProcessorBase::ProcessRawData(InData);

	const TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	const TSharedPtr<FBulletMoverCVDSimDataWrapper> SimData = MakeShared<FBulletMoverCVDSimDataWrapper>();
	const bool bSuccess = Chaos::VisualDebugger::ReadDataFromBuffer(InData, *SimData, ProviderSharedPtr.ToSharedRef());
	
	if (bSuccess)
	{
		if (FChaosVDSolverFrameData* CurrentSolverFrameData = ProviderSharedPtr->GetCurrentSolverFrame(SimData->SolverID))
		{
			if (TSharedPtr<FBulletMoverCVDSimDataContainer> SimDataContainer = CurrentSolverFrameData->GetCustomData().GetOrAddDefaultData<FBulletMoverCVDSimDataContainer>())
			{
				SimDataContainer->SimDataBySolverID.FindOrAdd(SimData->SolverID).Add(SimData);
			}
		}
	}

	return bSuccess;
}
