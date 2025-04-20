#include "LightmapExportFunctionLibrary.h"

#include "AssetExportTask.h"
#include "BusyCursor.h"
#include "EditorDirectories.h"
#include "ObjectTools.h"
#include "Dialogs/Dialogs.h"
#include "Dom/JsonObject.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Exporters/Exporter.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GCObjectScopeGuard.h"

void ULightmapExportFunctionLibrary::ExportObjects(const TArray<UObject*>& ObjectsToExport, const TArray<FString>& SaveFileNameList)
{
	GWarn->BeginSlowTask(NSLOCTEXT("UnrealEd", "Exporting", "Exporting"), true);

    // Create an array of all available exporters.
    TArray<UExporter*> Exporters;
    ObjectTools::AssembleListOfExporters(Exporters);

    //Array to control the batch mode and the show options for the exporters that will be use by the selected assets
    TArray<UExporter*> UsedExporters;

    // Export the objects.
    bool bAnyObjectMissingSourceData = false;

    for (int32 Index = 0; Index < ObjectsToExport.Num(); Index++)
    {
        GWarn->StatusUpdate(Index, ObjectsToExport.Num(), FText::Format(NSLOCTEXT("UnrealEd", "Exportingf", "Exporting ({0} of {1})"), FText::AsNumber(Index), FText::AsNumber(ObjectsToExport.Num())));

        UObject* ObjectToExport = ObjectsToExport[Index];
        if (!ObjectToExport)
        {
            continue;
        }

        if (ObjectToExport->GetOutermost()->HasAnyPackageFlags(PKG_DisallowExport))
        {
            continue;
        }

        // Create the path, then make sure the target file is not read-only.
        FString SaveFileName = SaveFileNameList[Index];
        const FString ObjectExportPath(FPaths::GetPath(SaveFileName));
        const bool bFileInSubdirectory = ObjectExportPath.Contains(TEXT("/"));
        if (bFileInSubdirectory && (!IFileManager::Get().MakeDirectory(*ObjectExportPath, true)))
        {
            FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "Error_FailedToMakeDirectory", "Failed to make directory {0}"), FText::FromString(ObjectExportPath)));
        }
        else if (IFileManager::Get().IsReadOnly(*SaveFileName))
        {
            FMessageDialog::Open(EAppMsgType::Ok, FText::Format(NSLOCTEXT("UnrealEd", "Error_CouldntWriteToFile_F", "Couldn't write to file '{0}'. Maybe file is read-only?"), FText::FromString(SaveFileName)));
        }
        else
        {
            // We have a writeable file.  Now go through that list of exporters again and find the right exporter and use it.
            TArray<UExporter*> ValidExporters;

            for (int32 ExporterIndex = 0; ExporterIndex < Exporters.Num(); ++ExporterIndex)
            {
                UExporter* Exporter = Exporters[ExporterIndex];
                if (Exporter->SupportsObject(ObjectToExport))
                {
                    check(Exporter->FormatExtension.Num() == Exporter->FormatDescription.Num());
                    for (int32 FormatIndex = 0; FormatIndex < Exporter->FormatExtension.Num(); ++FormatIndex)
                    {
                        const FString& FormatExtension = Exporter->FormatExtension[FormatIndex];
                        if (FCString::Stricmp(*FormatExtension, *FPaths::GetExtension(SaveFileName)) == 0 ||
                            FCString::Stricmp(*FormatExtension, TEXT("*")) == 0)
                        {
                            ValidExporters.Add(Exporter);
                            break;
                        }
                    }
                }
            }

            // Handle the potential of multiple exporters being found
            UExporter* ExporterToUse = NULL;
            if (ValidExporters.Num() == 1)
            {
                ExporterToUse = ValidExporters[0];
            }
            else if (ValidExporters.Num() > 1)
            {
                // Set up the first one as default
                ExporterToUse = ValidExporters[0];

                // ...but search for a better match if available
                for (int32 ExporterIdx = 0; ExporterIdx < ValidExporters.Num(); ExporterIdx++)
                {
                    if (ValidExporters[ExporterIdx]->GetClass()->GetFName() == ObjectToExport->GetExporterName())
                    {
                        ExporterToUse = ValidExporters[ExporterIdx];
                        break;
                    }
                }
            }

            // If an exporter was found, use it.
            if (ExporterToUse)
            {
                const FScopedBusyCursor BusyCursor;

                if (!UsedExporters.Contains(ExporterToUse))
                {
                    ExporterToUse->SetBatchMode(false);
                    ExporterToUse->SetCancelBatch(false);
                    ExporterToUse->SetShowExportOption(false);
                    ExporterToUse->AddToRoot();
                    UsedExporters.Add(ExporterToUse);
                }

                UAssetExportTask* ExportTask = NewObject<UAssetExportTask>();
                FGCObjectScopeGuard ExportTaskGuard(ExportTask);
                ExportTask->Object = ObjectToExport;
                ExportTask->Exporter = ExporterToUse;
                ExportTask->Filename = SaveFileName;
                ExportTask->bSelected = false;
                ExportTask->bReplaceIdentical = true;
                ExportTask->bPrompt = false;
                ExportTask->bUseFileArchive = ObjectToExport->IsA(UPackage::StaticClass());
                ExportTask->bWriteEmptyFiles = false;

                UExporter::RunAssetExportTask(ExportTask);

                if (ExporterToUse->GetBatchMode() && ExporterToUse->GetCancelBatch())
                {
                    //Exit the export file loop when there is a cancel all
                    break;
                }
            }
        }
    }

    //Set back the default value for the all used exporters
    for (UExporter* UsedExporter : UsedExporters)
    {
        UsedExporter->SetBatchMode(false);
        UsedExporter->SetCancelBatch(false);
        UsedExporter->SetShowExportOption(false);
        UsedExporter->RemoveFromRoot();
    }
    UsedExporters.Empty();

    if (bAnyObjectMissingSourceData)
    {
        FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Exporter_Error_SourceDataUnavailable", "No source data available for some objects.  See the log for details."));
    }

    GWarn->EndSlowTask();
}

void ULightmapExportFunctionLibrary::ExportLightmap(UWorld* World)
{
	UE_LOG(LogTemp, Warning, TEXT("Start Export lightmap for world: %s"), *World->GetName());

	FString DefaultPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::WORLD_ROOT);

	FString SelectedDirectory;
	bool bOk = PromptUserForDirectory(SelectedDirectory, TEXT("Select Directory to Export Lightmap"), DefaultPath);

	if (bOk == false)
	{
		UE_LOG(LogTemp, Warning, TEXT("User cancelled the directory selection."));
		return;
	}

	FString ExportPath = SelectedDirectory / World->GetName();

	TArray<UTexture2D*> AllLightmaps;
	World->GetLightMapsAndShadowMaps(World->PersistentLevel, AllLightmaps, false);

	TArray<UObject*> LightmapObjs;
	TArray<FString> LightmapNames;
	const FString LightmapPostfix = TEXT(".png");
	for (auto ObjIt = AllLightmaps.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UObject* Obj = *ObjIt)
		{
			FString ObjExportName = FPaths::Combine(ExportPath, Obj->GetName() + LightmapPostfix);
			if (LightmapNames.Contains(ObjExportName) == false)
			{
				LightmapNames.Add(ObjExportName);
				LightmapObjs.Add(Obj);
			}
		}
	}
	ExportObjects(LightmapObjs, LightmapNames);

	ExportPrimitiveLightmapInfoInWorld(World, ExportPath);
}

void ULightmapExportFunctionLibrary::ExportPrimitiveLightmapInfoInWorld(UWorld* World, const FString& ExportPath)
{
	if (World == nullptr)
	{
		return;
	}

	if (FPaths::DirectoryExists(ExportPath) == false)
	{
		return;
	}

	const UMapBuildDataRegistry* MapBuildDataRegistry = World->PersistentLevel->MapBuildData;

	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		if (Actor == nullptr)
		{
			continue;
		}

		TSharedRef<FJsonObject> JsonObject = MakeShareable(new FJsonObject());
		
		FString ActorName = Actor->GetActorNameOrLabel();
		FString ActorGuid = Actor->GetActorGuid().ToString();
		JsonObject->SetStringField(TEXT("ActorName"), ActorName);
		JsonObject->SetStringField(TEXT("ActorGuid"), ActorGuid);

		TArray<TSharedPtr<FJsonValue>> JsonComponents;
		for (const UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp == nullptr)
			{
				continue;
			}

			if (Comp->IsA(UPrimitiveComponent::StaticClass()) == false)
			{
				continue;
			}

			const UStaticMeshComponent* SMComp = Cast<UStaticMeshComponent>(Comp);
			if (SMComp == nullptr || SMComp->LODData.Num() <= 0)
			{
				continue;
			}

			TSharedPtr<FJsonObject> JsonCompObject = MakeShareable(new FJsonObject());
			FString CompName = SMComp->GetName();
			FString CompGuid = SMComp->LODData[0].MapBuildDataId.ToString();
			JsonCompObject->SetStringField(TEXT("ComponentName"), CompName);
			JsonCompObject->SetStringField(TEXT("ComponentGuid"), CompGuid);

			int32 LightmapSize = SMComp->GetStaticLightMapResolution();
			JsonCompObject->SetNumberField(TEXT("LightmapSize"), LightmapSize);

			if (const FMeshMapBuildData* MeshBuildData = MapBuildDataRegistry->GetMeshBuildData(SMComp->LODData[0].MapBuildDataId))
			{
				FLightMap2D* Lightmap = MeshBuildData->LightMap->GetLightMap2D();

				FString HQLightmapName = Lightmap->GetTexture(0)->GetName();
				FString LQLightmapName = Lightmap->GetTexture(1)->GetName();
				JsonCompObject->SetStringField(TEXT("HQLightmapName"), HQLightmapName);
				JsonCompObject->SetStringField(TEXT("LQLightmapName"), LQLightmapName);

				FVector2D CoordScale = Lightmap->GetCoordinateScale();
				FVector2D CoordBias = Lightmap->GetCoordinateBias();
				JsonCompObject->SetArrayField(TEXT("CoordScaleBias"), ConvertVectorToJsonArray(FVector4d(CoordScale, CoordBias)));

				FVector4f HQLightmapScale0 = Lightmap->ScaleVectors[0];
				FVector4f HQLightmapScale1 = Lightmap->ScaleVectors[1];
				FVector4f HQLightmapAdd0 = Lightmap->AddVectors[0];
				FVector4f HQLightmapAdd1 = Lightmap->AddVectors[1];
				JsonCompObject->SetArrayField(TEXT("HQLightmapScale0"), ConvertVectorToJsonArray(HQLightmapScale0));
				JsonCompObject->SetArrayField(TEXT("HQLightmapScale1"), ConvertVectorToJsonArray(HQLightmapScale1));
				JsonCompObject->SetArrayField(TEXT("HQLightmapAdd0"), ConvertVectorToJsonArray(HQLightmapAdd0));
				JsonCompObject->SetArrayField(TEXT("HQLightmapAdd1"), ConvertVectorToJsonArray(HQLightmapAdd1));

				FVector4f LQLightmapScale0 = Lightmap->ScaleVectors[2];
				FVector4f LQLightmapScale1 = Lightmap->ScaleVectors[3];
				FVector4f LQLightmapAdd0 = Lightmap->AddVectors[2];
				FVector4f LQLightmapAdd1 = Lightmap->AddVectors[3];
				JsonCompObject->SetArrayField(TEXT("LQLightmapScale0"), ConvertVectorToJsonArray(LQLightmapScale0));
				JsonCompObject->SetArrayField(TEXT("LQLightmapScale1"), ConvertVectorToJsonArray(LQLightmapScale1));
				JsonCompObject->SetArrayField(TEXT("LQLightmapAdd0"), ConvertVectorToJsonArray(LQLightmapAdd0));
				JsonCompObject->SetArrayField(TEXT("LQLightmapAdd1"), ConvertVectorToJsonArray(LQLightmapAdd1));

				JsonComponents.Add(MakeShareable(new FJsonValueObject(JsonCompObject)));
			}
		}

		if (JsonComponents.Num() > 0)
		{
			JsonObject->SetArrayField(TEXT("Components"), JsonComponents);

			FString JsonString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
			if (FJsonSerializer::Serialize(JsonObject, Writer))
			{
				FString FileName = FPaths::Combine(ExportPath, ActorName + TEXT(".json"));
				FFileHelper::SaveStringToFile(JsonString, *FileName);
			}
		}
	}
}

template <typename T>
TArray<TSharedPtr<FJsonValue>> ULightmapExportFunctionLibrary::ConvertVectorToJsonArray(const UE::Math::TVector4<T>& Vector)
{
	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray.Add(MakeShareable(new FJsonValueNumber(Vector.X)));
	JsonArray.Add(MakeShareable(new FJsonValueNumber(Vector.Y)));
	JsonArray.Add(MakeShareable(new FJsonValueNumber(Vector.Z)));
	JsonArray.Add(MakeShareable(new FJsonValueNumber(Vector.W)));
	return JsonArray;
}

