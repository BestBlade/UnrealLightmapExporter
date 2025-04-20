#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "LightmapExportFunctionLibrary.generated.h"

UCLASS()
class LIGHTMAPEXPORT_API ULightmapExportFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void ExportObjects(const TArray<UObject*>& ObjectsToExport, const TArray<FString>& SaveFileNameList);

	static void ExportLightmap(UWorld* World);

	static void ExportPrimitiveLightmapInfoInWorld(UWorld* World, const FString& ExportPath);

	template<typename T>
	static TArray<TSharedPtr<FJsonValue>> ConvertVectorToJsonArray(const UE::Math::TVector4<T>& Vector);
};