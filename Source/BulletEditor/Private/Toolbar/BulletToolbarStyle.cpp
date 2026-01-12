// Fill out your copyright notice in the Description page of Project Settings.


#include "Toolbar/BulletToolbarStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FBulletToolbarStyle::StyleInstance = nullptr;

void FBulletToolbarStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBulletToolbarStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBulletToolbarStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BulletEditor"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FBulletToolbarStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("BulletEditor"));
	Style->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/"));

	Style->Set("Bullet.ConvertActorsAction", new IMAGE_BRUSH_SVG( "Starship/Common/Apply", Icon20x20));
	return Style;
}

void FBulletToolbarStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FBulletToolbarStyle::Get()
{
	return *StyleInstance;
}
