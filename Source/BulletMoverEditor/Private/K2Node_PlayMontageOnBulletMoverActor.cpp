// Copyright Epic Games, Inc. All Rights Reserved.

#include "K2Node_PlayMontageOnBulletMoverActor.h"

#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "MoveLibrary/PlayBulletMoverMontageCallbackProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(K2Node_PlayMontageOnBulletMoverActor)

#define LOCTEXT_NAMESPACE "BulletMover_K2Nodes"

UK2Node_PlayMontageOnBulletMoverActor::UK2Node_PlayMontageOnBulletMoverActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UPlayBulletMoverMontageCallbackProxy, CreateProxyObjectForPlayMoverMontage);
	ProxyFactoryClass = UPlayBulletMoverMontageCallbackProxy::StaticClass();
	ProxyClass = UPlayBulletMoverMontageCallbackProxy::StaticClass();
}

FText UK2Node_PlayMontageOnBulletMoverActor::GetTooltipText() const
{
	return LOCTEXT("K2Node_PlayMontageOnBulletMoverActor_Tooltip", "Plays a Montage on an actor with BulletMover and SkeletalMesh components. Used for networked animation root motion.");
}

FText UK2Node_PlayMontageOnBulletMoverActor::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("PlayMontageOnBulletMoverActor", "Play Montage (BulletMover Actor)");
}

FText UK2Node_PlayMontageOnBulletMoverActor::GetMenuCategory() const
{
	return LOCTEXT("PlayMontageCategory", "Animation|Montage");
}

static const FName NAME_OnNotifyBegin = "OnNotifyBegin";
static const FName NAME_OnNotifyEnd = "OnNotifyEnd";

void UK2Node_PlayMontageOnBulletMoverActor::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	Super::GetPinHoverText(Pin, HoverTextOut);

	if (Pin.PinName == NAME_OnNotifyBegin)
	{
		FText ToolTipText = LOCTEXT("K2Node_PlayMontageOnBulletMoverActor_OnNotifyBegin_Tooltip", "Event called when using a PlayMontageNotify or PlayMontageNotifyWindow Notify in a Montage.");
		HoverTextOut = FString::Printf(TEXT("%s\n%s"), *ToolTipText.ToString(), *HoverTextOut);
	}
	else if (Pin.PinName == NAME_OnNotifyEnd)
	{
		FText ToolTipText = LOCTEXT("K2Node_PlayMontageOnBulletMoverActor_OnNotifyEnd_Tooltip", "Event called when using a PlayMontageNotifyWindow Notify in a Montage.");
		HoverTextOut = FString::Printf(TEXT("%s\n%s"), *ToolTipText.ToString(), *HoverTextOut);
	}
}

#undef LOCTEXT_NAMESPACE
