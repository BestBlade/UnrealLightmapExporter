// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

class FLightmapExportModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedRef<FExtender> ExtendAssetContextMenu(const TArray<FAssetData>& SelectedAssets);

	void AddWorldAssetMenuExtension(FMenuBuilder& MenuBuilder, TArray<UWorld*> Worlds);

	static FString GetPluginBaseDir(bool bFullPath = false);
};
