#include "BulletEditor.h"

#include "Selection.h"
#include "Core/BaseClasses/BulletStaticMeshActor.h"
#include "Factories/BulletStaticMeshActorFactory.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Toolbar/BulletToolbarCommands.h"
#include "Toolbar/BulletToolbarStyle.h"

#define LOCTEXT_NAMESPACE "FBulletEditorModule"

void FBulletEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FBulletToolbarStyle::Initialize();
	FBulletToolbarStyle::ReloadTextures();

	FBulletToolbarCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FBulletToolbarCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FBulletEditorModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FBulletEditorModule::RegisterMenus));
	/*if (GEditor)
	{
		UBulletStaticMeshActorFactory* NewFactory = NewObject<UBulletStaticMeshActorFactory>();
		GEditor->ActorFactories.Insert(NewFactory,0);
	}*/
}

void FBulletEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FBulletToolbarStyle::Shutdown();

	FBulletToolbarCommands::Unregister();
	
	
	/*if (GEditor)
	{
		GEditor->ActorFactories.RemoveAll([](UActorFactory* Factory) 
		{
			return Factory && Factory->IsA<UBulletStaticMeshActorFactory>();
		});
	}*/
}

void FBulletEditorModule::PluginButtonClicked()
{
	// Put your "OnButtonClicked" stuff here
	
	TSet<FString> ComponentsToConsider;
	ComponentsToConsider.Add("StaticMeshComponent");

	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	GEditor->GetSelectedActors()->GetSelectedObjects(SelectedObjects);

	UClass* ReplaceWithClass = ABulletStaticMeshActor::StaticClass();
	
	TArray<AActor*> SelectedActors;
	for (TWeakObjectPtr<UObject> S : SelectedObjects)
	{
		if (!Cast<AStaticMeshActor>(S)) continue;
		
		SelectedActors.Add(Cast<AActor>(S));
	}

	UEditorActorSubsystem::ConvertActors(SelectedActors, ReplaceWithClass, ComponentsToConsider, true);


	if (SelectedActors.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("No static mesh actors are selected")));
		return;
	}

	
	FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Selected Static Mesh Actors Converted To Bullet Static Mesh Actors")));
}

void FBulletEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FBulletToolbarCommands::Get().PluginAction,
				PluginCommands,
				TAttribute<FText>(FText::FromString(TEXT("Convert Actors"))),
					FText::FromString(TEXT("Converts selected AStaticMeshActor mesh components to UBulletStaticMeshComponent ")),
					 FSlateIcon(FAppStyle::Get().GetStyleSetName(), "MergeActors.MeshMergingTool"));
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FBulletToolbarCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
				
				Entry.Icon = FSlateIcon(FAppStyle::Get().GetStyleSetName(), "MergeActors.MeshMergingTool");
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FBulletEditorModule, BulletEditor)