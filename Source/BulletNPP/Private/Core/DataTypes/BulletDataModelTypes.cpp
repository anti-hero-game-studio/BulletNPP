// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/DataTypes/BulletDataModelTypes.h"

#include "BulletLogChannels.h"
#include "Core/Libraries/BulletBasedMovementLibrary.h"

void FBulletDefaultInputs::SetMoveInput(EBulletMoveInputType InMoveInputType, const FVector& InMoveInput)
{
	MoveInputType = InMoveInputType;

	// Limit the precision that we store, so that it matches what is NetSerialized (2 decimal place of precision).
	// This ensures the authoring client, server, and any networking peers are all simulating with the same move input.
	// Note: any change to desired precision must be made here and in NetSerialize
	MoveInput.X = FMath::RoundToFloat(InMoveInput.X * 100.0) / 100.0;
	MoveInput.Y = FMath::RoundToFloat(InMoveInput.Y * 100.0) / 100.0;
	MoveInput.Z = FMath::RoundToFloat(InMoveInput.Z * 100.0) / 100.0;
}


FVector FBulletDefaultInputs::GetMoveInput_WorldSpace() const
{
	if (bUsingMovementBase && MovementBase)
	{
		FVector MoveInputWorldSpace;
		UBulletBasedMovementLibrary::TransformBasedDirectionToWorld(MovementBase, MovementBaseBoneName, MoveInput, MoveInputWorldSpace);
		return MoveInputWorldSpace;
	}

	return MoveInput;	// already in world space
}


FVector FBulletDefaultInputs::GetOrientationIntentDir_WorldSpace() const
{
	if (bUsingMovementBase && MovementBase)
	{
		FVector OrientIntentDirWorldSpace;
		UBulletBasedMovementLibrary::TransformBasedDirectionToWorld(MovementBase, MovementBaseBoneName, OrientationIntent, OrientIntentDirWorldSpace);
		return OrientIntentDirWorldSpace;
	}

	return OrientationIntent;	// already in world space
}


FBulletDataStructBase* FBulletDefaultInputs::Clone() const
{
	// TODO: ensure that this memory allocation jives with deletion method
	FBulletDefaultInputs* CopyPtr = new FBulletDefaultInputs(*this);
	return CopyPtr;
}

bool FBulletDefaultInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << MoveInputType;

	SerializePackedVector<100, 30>(MoveInput, Ar);	// Note: if you change this serialization, also change in SetMoveInput
	SerializeFixedVector<1, 16>(OrientationIntent, Ar);
	ControlRotation.SerializeCompressedShort(Ar);

	Ar << SuggestedMovementMode;

	Ar.SerializeBits(&bUsingMovementBase, 1);

	if (bUsingMovementBase)
	{
		Ar << MovementBase;
		Ar << MovementBaseBoneName;
	}
	else if (Ar.IsLoading())
	{
		// skip attempts to load movement base properties if flagged as not using a movement base
		MovementBase = nullptr;
		MovementBaseBoneName = NAME_None;
	}

	Ar.SerializeBits(&bIsJumpJustPressed, 1);
	Ar.SerializeBits(&bIsJumpPressed, 1);

	bOutSuccess = true;
	return true;
}


void FBulletDefaultInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("MoveInput: %s (Type %i)\n", TCHAR_TO_ANSI(*MoveInput.ToCompactString()), MoveInputType);
	Out.Appendf("OrientationIntent: X=%.2f Y=%.2f Z=%.2f\n", OrientationIntent.X, OrientationIntent.Y, OrientationIntent.Z);
	Out.Appendf("ControlRotation: P=%.2f Y=%.2f R=%.2f\n", ControlRotation.Pitch, ControlRotation.Yaw, ControlRotation.Roll);
	Out.Appendf("SuggestedMovementMode: %s\n", TCHAR_TO_ANSI(*SuggestedMovementMode.ToString()));

	if (MovementBase)
	{
		Out.Appendf("MovementBase: %s (bone %s)\n", TCHAR_TO_ANSI(*GetNameSafe(MovementBase->GetOwner())), TCHAR_TO_ANSI(*MovementBaseBoneName.ToString()));
	}
	else
	{
		Out.Appendf("MovementBase: none\n");
	}

	Out.Appendf("bIsJumpPressed: %i\tbIsJumpJustPressed: %i\n", bIsJumpPressed, bIsJumpJustPressed);
}

bool FBulletDefaultInputs::ShouldReconcile(const FBulletDataStructBase& AuthorityState) const
{
	const FBulletDefaultInputs& TypedAuthority = static_cast<const FBulletDefaultInputs&>(AuthorityState);
	return *this != TypedAuthority;
}

void FBulletDefaultInputs::Interpolate(const FBulletDataStructBase& From, const FBulletDataStructBase& To, float Pct)
{
	const FBulletDefaultInputs& TypedFrom = static_cast<const FBulletDefaultInputs&>(From);
	const FBulletDefaultInputs& TypedTo = static_cast<const FBulletDefaultInputs&>(To);
	
	// Note, this ignores movement base as this is not used by the physics mover
	const FBulletDefaultInputs* ClosestInputs = Pct < 0.5f ? &TypedFrom : &TypedTo;
	bIsJumpJustPressed = ClosestInputs->bIsJumpJustPressed;
	bIsJumpPressed = ClosestInputs->bIsJumpPressed;
	SuggestedMovementMode = ClosestInputs->SuggestedMovementMode;

	SetMoveInput(ClosestInputs->GetMoveInputType(), FMath::Lerp(TypedFrom.GetMoveInput(), TypedTo.GetMoveInput(), Pct));
	OrientationIntent = FMath::Lerp(TypedFrom.OrientationIntent, TypedTo.OrientationIntent, Pct);
	ControlRotation = FMath::Lerp(TypedFrom.ControlRotation, TypedTo.ControlRotation, Pct);
}

void FBulletDefaultInputs::Merge(const FBulletDataStructBase& From)
{
	const FBulletDefaultInputs& TypedFrom = static_cast<const FBulletDefaultInputs&>(From);
	bIsJumpJustPressed |= TypedFrom.bIsJumpJustPressed;
	bIsJumpPressed |= TypedFrom.bIsJumpPressed;
}

static float CharacterDefaultInputsDecayAmountMultiplier = 1.f;
FAutoConsoleVariableRef CVarCharacterDefaultInputsDecayAmountMultiplier(
	TEXT("Bullet.Input.CharacterDefaultInputsDecayAmountMultiplier"),
	CharacterDefaultInputsDecayAmountMultiplier,
	TEXT("Multiplier to use when decaying CharacterDefaultInputs."));

void FBulletDefaultInputs::Decay(float DecayAmount)
{
	DecayAmount *= CharacterDefaultInputsDecayAmountMultiplier;

	MoveInput *= (1.0f - DecayAmount);

	// Single use inputs
	bIsJumpJustPressed = FMath::IsNearlyZero(DecayAmount) ? bIsJumpJustPressed : false;
}

// FBulletDefaultSyncState //////////////////////////////////////////////////////////////

FBulletDataStructBase* FBulletDefaultSyncState::Clone() const
{
	// TODO: ensure that this memory allocation jives with deletion method
	FBulletDefaultSyncState* CopyPtr = new FBulletDefaultSyncState(*this);
	return CopyPtr;
}

bool FBulletDefaultSyncState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	SerializePackedVector<100, 30>(Location, Ar);
	SerializeFixedVector<2, 8>(MoveDirectionIntent, Ar);
	SerializePackedVector<10, 16>(Velocity, Ar);
	SerializePackedVector<10, 16>(AngularVelocityDegrees, Ar);
	Orientation.SerializeCompressedShort(Ar);

	// Optional movement base
	bool bIsUsingMovementBase = (Ar.IsSaving() ? MovementBase.IsValid() : false);
	Ar.SerializeBits(&bIsUsingMovementBase, 1);

	if (bIsUsingMovementBase)
	{
		Ar << MovementBase;
		Ar << MovementBaseBoneName;

		SerializePackedVector<100, 30>(MovementBasePos, Ar);
		MovementBaseQuat.NetSerialize(Ar, Map, bOutSuccess);
	}
	else if (Ar.IsLoading())
	{
		MovementBase = nullptr;
	}

	bOutSuccess = true;
	return true;
}

void FBulletDefaultSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("Loc: X=%.2f Y=%.2f Z=%.2f\n", Location.X, Location.Y, Location.Z);
	Out.Appendf("Intent: X=%.2f Y=%.2f Z=%.2f\n", MoveDirectionIntent.X, MoveDirectionIntent.Y, MoveDirectionIntent.Z);
	Out.Appendf("Vel: X=%.2f Y=%.2f Z=%.2f\n", Velocity.X, Velocity.Y, Velocity.Z);
	Out.Appendf("Ang Vel: X=%.2f Y=%.2f Z=%.2f\n", AngularVelocityDegrees.X, AngularVelocityDegrees.Y, AngularVelocityDegrees.Z);
	Out.Appendf("Orient: P=%.2f Y=%.2f R=%.2f\n", Orientation.Pitch, Orientation.Yaw, Orientation.Roll);

	if (const UPrimitiveComponent* MovementBasePtr = MovementBase.Get())
	{
		Out.Appendf("MovementBase: %s (bone %s)\n", TCHAR_TO_ANSI(*GetNameSafe(MovementBasePtr->GetOwner())), TCHAR_TO_ANSI(*MovementBaseBoneName.ToString()));
		Out.Appendf("    BasePos: %s   BaseRot: %s\n", TCHAR_TO_ANSI(*MovementBasePos.ToCompactString()), TCHAR_TO_ANSI(*MovementBaseQuat.Rotator().ToCompactString()));
	}
	else
	{
		Out.Appendf("MovementBase: none\n");
	}

}


bool FBulletDefaultSyncState::ShouldReconcile(const FBulletDataStructBase& AuthorityState) const
{
	const FBulletDefaultSyncState* AuthoritySyncState = static_cast<const FBulletDefaultSyncState*>(&AuthorityState);
	const float DistErrorTolerance = 5.f;	// JAH TODO: define these elsewhere as CVars or data asset settings

	const bool bAreInDifferentSpaces = !((MovementBase.HasSameIndexAndSerialNumber(AuthoritySyncState->MovementBase)) && (MovementBaseBoneName == AuthoritySyncState->MovementBaseBoneName));

	bool bIsNearEnough = false;

	if (!bAreInDifferentSpaces)
	{
		if (MovementBase.IsValid())
		{
			bIsNearEnough = GetLocation_BaseSpace().Equals(AuthoritySyncState->GetLocation_BaseSpace(), DistErrorTolerance);
		}
		else
		{
			bIsNearEnough = GetLocation_WorldSpace().Equals(AuthoritySyncState->GetLocation_WorldSpace(), DistErrorTolerance);
		}
	}

	return bAreInDifferentSpaces || !bIsNearEnough;
}


void FBulletDefaultSyncState::Interpolate(const FBulletDataStructBase& From, const FBulletDataStructBase& To, float Pct)
{
	const FBulletDefaultSyncState* FromState = static_cast<const FBulletDefaultSyncState*>(&From);
	const FBulletDefaultSyncState* ToState = static_cast<const FBulletDefaultSyncState*>(&To);

	// TODO: investigate replacing this threshold with a flag indicating that the state (or parts thereof) isn't intended to be interpolated
	static constexpr float TeleportThreshold = 500.f * 500.f;
	if (FVector::DistSquared(FromState->GetLocation_WorldSpace(), ToState->GetLocation_WorldSpace()) > TeleportThreshold)
	{
		*this = *ToState;
	}
	else
	{

		// No matter what base we started from, we always interpolate into the "To" movement base's space
		MovementBase         = ToState->MovementBase;
		MovementBaseBoneName = ToState->MovementBaseBoneName;
		MovementBasePos      = ToState->MovementBasePos;
		MovementBaseQuat     = ToState->MovementBaseQuat;

		FVector FromLocation_ToSpace, FromIntent_ToSpace, FromVelocity_ToSpace, FromAngularVelocity_ToSpace;
		FRotator FromOrientation_ToSpace;

	
		// Bases match (or not using based movement at all)
		if (FromState->MovementBase.HasSameIndexAndSerialNumber(ToState->MovementBase) && FromState->MovementBaseBoneName == ToState->MovementBaseBoneName)
		{
			if (FromState->MovementBase.IsValid())
			{
				MovementBasePos = FMath::Lerp(FromState->MovementBasePos, ToState->MovementBasePos, Pct);
				MovementBaseQuat = FQuat::Slerp(FromState->MovementBaseQuat, ToState->MovementBaseQuat, Pct);
			}

			FromLocation_ToSpace    = FromState->Location;
			FromIntent_ToSpace      = FromState->MoveDirectionIntent;
			FromVelocity_ToSpace    = FromState->Velocity;
			FromAngularVelocity_ToSpace    = FromState->AngularVelocityDegrees;
			FromOrientation_ToSpace = FromState->Orientation;
		}
		else if (ToState->MovementBase.IsValid())	// if moving onto a different base, regardless of coming from a prior base or not
		{
			UBulletBasedMovementLibrary::TransformLocationToLocal(ToState->MovementBasePos, ToState->MovementBaseQuat, FromState->GetLocation_WorldSpace(), OUT FromLocation_ToSpace);
			UBulletBasedMovementLibrary::TransformDirectionToLocal(ToState->MovementBaseQuat, FromState->GetIntent_WorldSpace(), OUT FromIntent_ToSpace);
			UBulletBasedMovementLibrary::TransformDirectionToLocal(ToState->MovementBaseQuat, FromState->GetVelocity_WorldSpace(), OUT FromVelocity_ToSpace);
			UBulletBasedMovementLibrary::TransformDirectionToLocal(ToState->MovementBaseQuat, FromState->GetAngularVelocityDegrees_WorldSpace(), OUT FromAngularVelocity_ToSpace);
			UBulletBasedMovementLibrary::TransformRotatorToLocal(ToState->MovementBaseQuat, FromState->GetOrientation_WorldSpace(), OUT FromOrientation_ToSpace);
		}
		else if (FromState->MovementBase.IsValid())	// if leaving a base
		{
			FromLocation_ToSpace	= FromState->GetLocation_WorldSpace();
			FromIntent_ToSpace		= FromState->GetIntent_WorldSpace();
			FromVelocity_ToSpace	= FromState->GetVelocity_WorldSpace();
			FromAngularVelocity_ToSpace	= FromState->GetAngularVelocityDegrees_WorldSpace();
			FromOrientation_ToSpace = FromState->GetOrientation_WorldSpace();
		}

		Location			= FMath::Lerp(FromLocation_ToSpace,		ToState->Location, Pct);
		MoveDirectionIntent = FMath::Lerp(FromIntent_ToSpace,		ToState->MoveDirectionIntent, Pct);
		Velocity			= FMath::Lerp(FromVelocity_ToSpace,		ToState->Velocity, Pct);
		AngularVelocityDegrees = FMath::Lerp(FromAngularVelocity_ToSpace,ToState->AngularVelocityDegrees, Pct);
		Orientation			= FMath::Lerp(FromOrientation_ToSpace,	ToState->Orientation, Pct);

	}
}

void FBulletDefaultSyncState::SetTransforms_WorldSpace(FVector WorldLocation, FRotator WorldOrient, FVector WorldVelocity, UPrimitiveComponent* Base,
	FName BaseBone)
{
	SetTransforms_WorldSpace(WorldLocation, WorldOrient, WorldVelocity, FVector::ZeroVector, Base, BaseBone);
}

void FBulletDefaultSyncState::SetTransforms_WorldSpace(FVector WorldLocation, FRotator WorldOrient, FVector WorldVelocity, FVector WorldAngularVelocityDegrees, UPrimitiveComponent* Base, FName BaseBone)
{
	if (SetMovementBase(Base, BaseBone))
	{
		UBulletBasedMovementLibrary::TransformLocationToLocal(  MovementBasePos,  MovementBaseQuat, WorldLocation, OUT Location);
		UBulletBasedMovementLibrary::TransformRotatorToLocal(   MovementBaseQuat, WorldOrient, OUT Orientation);
		UBulletBasedMovementLibrary::TransformDirectionToLocal( MovementBaseQuat, WorldVelocity, OUT Velocity);
		UBulletBasedMovementLibrary::TransformDirectionToLocal( MovementBaseQuat, WorldAngularVelocityDegrees, OUT AngularVelocityDegrees);
	}
	else
	{
		if (Base)
		{
			UE_LOG(LogBullet, Warning, TEXT("Failed to set base as %s. Falling back to world space movement"), *GetNameSafe(Base->GetOwner()));
		}

		Location = WorldLocation;
		Orientation = WorldOrient;
		Velocity = WorldVelocity;
		AngularVelocityDegrees = WorldAngularVelocityDegrees;
	}
}


bool FBulletDefaultSyncState::SetMovementBase(UPrimitiveComponent* Base, FName BaseBone)
{
	MovementBase = Base;
	MovementBaseBoneName = BaseBone;

	const bool bDidCaptureBaseTransform = UpdateCurrentMovementBase();
	return !Base || bDidCaptureBaseTransform;
}


bool FBulletDefaultSyncState::UpdateCurrentMovementBase()
{
	bool bDidGetBaseTransform = false;

	if (const UPrimitiveComponent* MovementBasePtr = MovementBase.Get())
	{
		//bDidGetBaseTransform = UBulletBasedMovementLibrary::GetMovementBaseTransform(MovementBasePtr, MovementBaseBoneName, OUT MovementBasePos, OUT MovementBaseQuat);
	}

	if (!bDidGetBaseTransform)
	{
		MovementBase = nullptr;
		MovementBaseBoneName = NAME_None;
		MovementBasePos = FVector::ZeroVector;
		MovementBaseQuat = FQuat::Identity;
	}

	return bDidGetBaseTransform;
}

bool FBulletDefaultSyncState::IsNearlyEqual(const FBulletDefaultSyncState& Other) const
{
	const bool bHasSameBaseBaseInfo = (!MovementBase.IsValid() && !Other.MovementBase.IsValid()) ||
											(MovementBase == Other.MovementBase && 
											 MovementBaseBoneName == Other.MovementBaseBoneName &&
											 (MovementBasePos - Other.MovementBasePos).IsNearlyZero() && 
											 MovementBaseQuat.Equals(Other.MovementBaseQuat));

	return (Location-Other.Location).IsNearlyZero() &&
		(Orientation-Other.Orientation).IsNearlyZero() &&
		(Velocity-Other.Velocity).IsNearlyZero() &&(AngularVelocityDegrees-Other.AngularVelocityDegrees).IsNearlyZero() &&
		(MoveDirectionIntent-Other.MoveDirectionIntent).IsNearlyZero() &&
			bHasSameBaseBaseInfo;
}

FVector FBulletDefaultSyncState::GetLocation_WorldSpace() const
{
	if (MovementBase.IsValid())
	{
		return FTransform(MovementBaseQuat, MovementBasePos).TransformPositionNoScale(Location);
	}

	return Location; // if no base, assumed to be in world space
}

FVector FBulletDefaultSyncState::GetLocation_BaseSpace() const
{
	return Location;
}


FVector FBulletDefaultSyncState::GetIntent_WorldSpace() const
{
	if (MovementBase.IsValid())
	{
		return MovementBaseQuat.RotateVector(MoveDirectionIntent);
	}

	return MoveDirectionIntent; // if no base, assumed to be in world space
}

FVector FBulletDefaultSyncState::GetIntent_BaseSpace() const
{
	return MoveDirectionIntent;
}

FVector FBulletDefaultSyncState::GetVelocity_WorldSpace() const
{
	if (MovementBase.IsValid())
	{
		return MovementBaseQuat.RotateVector(Velocity);
	}

	return Velocity; // if no base, assumed to be in world space
}

FVector FBulletDefaultSyncState::GetVelocity_BaseSpace() const
{
	return Velocity;
}


FRotator FBulletDefaultSyncState::GetOrientation_WorldSpace() const
{
	if (MovementBase.IsValid())
	{
		return (MovementBaseQuat * FQuat(Orientation)).Rotator();
	}

	return Orientation; // if no base, assumed to be in world space
}


FRotator FBulletDefaultSyncState::GetOrientation_BaseSpace() const
{
	return Orientation;
}

FTransform FBulletDefaultSyncState::GetTransform_WorldSpace() const
{
	if (MovementBase.IsValid())
	{
		return FTransform(Orientation, Location) * FTransform(MovementBaseQuat, MovementBasePos);
	}

	return FTransform(Orientation, Location);
}

FTransform FBulletDefaultSyncState::GetTransform_BaseSpace() const
{
	return FTransform(Orientation, Location);
}

FVector FBulletDefaultSyncState::GetAngularVelocityDegrees_WorldSpace() const
{
	if (MovementBase.IsValid())
	{
		return MovementBaseQuat.RotateVector(AngularVelocityDegrees);
	}

	return AngularVelocityDegrees; // if no base, assumed to be in world space
}

FVector FBulletDefaultSyncState::GetAngularVelocityDegrees_BaseSpace() const
{
	return AngularVelocityDegrees;
}


// UBulletDataModelBlueprintLibrary ///////////////////////////////////////////////////

void UBulletDataModelBlueprintLibrary::SetDirectionalInput(FBulletDefaultInputs& Inputs, const FVector& DirectionInput)
{
	Inputs.SetMoveInput(EBulletMoveInputType::DirectionalIntent, DirectionInput);
}

void UBulletDataModelBlueprintLibrary::SetVelocityInput(FBulletDefaultInputs& Inputs, const FVector& VelocityInput)
{
	Inputs.SetMoveInput(EBulletMoveInputType::Velocity, VelocityInput);
}

FVector UBulletDataModelBlueprintLibrary::GetMoveDirectionIntentFromInputs(const FBulletDefaultInputs& Inputs)
{
	return Inputs.GetMoveInput_WorldSpace();
}

FVector UBulletDataModelBlueprintLibrary::GetLocationFromSyncState(const FBulletDefaultSyncState& SyncState)
{
	return SyncState.GetLocation_WorldSpace();
}

FVector UBulletDataModelBlueprintLibrary::GetMoveDirectionIntentFromSyncState(const FBulletDefaultSyncState& SyncState)
{
	return SyncState.GetIntent_WorldSpace();
}

FVector UBulletDataModelBlueprintLibrary::GetVelocityFromSyncState(const FBulletDefaultSyncState& SyncState)
{
	return SyncState.GetVelocity_WorldSpace();
}

FVector UBulletDataModelBlueprintLibrary::GetAngularVelocityDegreesFromSyncState(const FBulletDefaultSyncState& SyncState)
{
	return SyncState.GetAngularVelocityDegrees_WorldSpace();
}

FRotator UBulletDataModelBlueprintLibrary::GetOrientationFromSyncState(const FBulletDefaultSyncState& SyncState)
{
	return SyncState.GetOrientation_WorldSpace();
}
