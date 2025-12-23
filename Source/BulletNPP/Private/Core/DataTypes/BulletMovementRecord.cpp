// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/DataTypes/BulletMovementRecord.h"

void FBulletMovementRecord::Reset()
{
	TotalMoveDelta = FVector::ZeroVector;
	RelevantMoveDelta = FVector::ZeroVector;
	TotalDeltaSeconds = 0.f;

	bIsRelevancyLocked = false;
	bRelevancyLockValue = false;

	Substeps.Empty(Substeps.Num());	// leaving space for the next user, as this is likely to fill up again
}

void FBulletMovementRecord::Append(FBulletMovementSubstep Substep)
{
	if (bIsRelevancyLocked)
	{
		Substep.bIsRelevant = bRelevancyLockValue;
	}

	if (Substep.bIsRelevant)
	{
		RelevantMoveDelta += Substep.MoveDelta;
	}

	TotalMoveDelta += Substep.MoveDelta;

	Substeps.Add(Substep);
}

FString FBulletMovementRecord::ToString() const
{
	return FString::Printf( TEXT("TotalMove: %s over %.3f seconds. RelevantVelocity: %s. Substeps: %s"),
		*TotalMoveDelta.ToCompactString(),
		TotalDeltaSeconds,
		*GetRelevantVelocity().ToCompactString(),
		*FString::JoinBy(Substeps, TEXT(","), [](const FBulletMovementSubstep& Substep) { return Substep.MoveName.ToString(); }));
}
