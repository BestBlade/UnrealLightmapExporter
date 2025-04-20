// Copyright Epic Games, Inc. All Rights Reserved.

#include "LightmapExport.h"

#include "ContentBrowserModule.h"
#include "LightmapExportFunctionLibrary.h"

#define LOCTEXT_NAMESPACE "FLightmapExportModule"

void FLightmapExportModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(FContentBrowserMenuExtender_SelectedAssets::CreateRaw(
		this,
		&FLightmapExportModule::ExtendAssetContextMenu)
	);
}

void FLightmapExportModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

TSharedRef<FExtender> FLightmapExportModule::ExtendAssetContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender(new FExtender());
	
	// Add your custom menu item here
	// Extender->AddMenuExtension(...);
	TArray<UWorld*> Worlds;

	for (const FAssetData& Asset : SelectedAssets)
	{
		UClass* AssetClass = FindObject<UClass>(nullptr, *Asset.AssetClassPath.ToString());
		if (AssetClass)
		{
			if (AssetClass->IsChildOf(UWorld::StaticClass()) || AssetClass == UWorld::StaticClass())
			{
				Worlds.Add(Cast<UWorld>(Asset.GetAsset()));
			}
		}
	}

	if (Worlds.Num() > 0)
	{
		Extender->AddMenuExtension(
			TEXT("CommonAssetActions"),
			EExtensionHook::Before,
			nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FLightmapExportModule::AddWorldAssetMenuExtension, Worlds)
		);
	}

	return Extender;
}

void FLightmapExportModule::AddWorldAssetMenuExtension(FMenuBuilder& MenuBuilder, TArray<UWorld*> Worlds)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ExportLightmap", "Export Lightmap"),
		LOCTEXT("ExportLightmapTooltip", "Export the lightmap for the selected world(s)"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic(&ULightmapExportFunctionLibrary::ExportLightmap, Worlds[0])
		),
		NAME_None,
		EUserInterfaceActionType::Button
	);
}

FString FLightmapExportModule::GetPluginBaseDir(bool bFullPath)
{
	FString RelativePath = IPluginManager::Get().FindPlugin(TEXT("LightmapExport"))->GetBaseDir();

	if (bFullPath)
	{
		return FPaths::ConvertRelativePathToFull(RelativePath);
	}

	return RelativePath;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLightmapExportModule, LightmapExport)