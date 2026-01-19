// Copyright Epic Games, Inc. All Rights Reserved.

#include "BulletMoverCVDStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Slate/SlateGameResources.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FBulletMoverCVDStyle::StyleInstance = nullptr;

void FBulletMoverCVDStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBulletMoverCVDStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBulletMoverCVDStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BulletMoverCVDStyle"));
	return StyleSetName;
}

const FVector2D Icon16x16(16.0f, 16.0f);

TSharedRef< FSlateStyleSet > FBulletMoverCVDStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("BulletMoverCVDStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("BulletNPP")->GetBaseDir() / TEXT("Resources"));

	Style->Set("TabIconBulletMoverInfoPanel",  new IMAGE_BRUSH_SVG(TEXT("MoverInfo"), Icon16x16));

	return Style;
}

void FBulletMoverCVDStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FBulletMoverCVDStyle::Get()
{
	return *StyleInstance;
}
