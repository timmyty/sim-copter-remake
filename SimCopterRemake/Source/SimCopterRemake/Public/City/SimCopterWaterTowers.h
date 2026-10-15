#pragma once

#include "Formats/MaxisMeshReader.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;

namespace SimCopterWaterTowers
{
constexpr int32 ObjectId = 0x01c; // WT235, XBLD 235. WT220 is a different utility building.
constexpr int32 SectionKey = -206;
constexpr int32 LabelWidth = 1024;
constexpr int32 LabelHeight = 256;

// A presentation copy retaining the original tank/frame, with a sealed metal lid and bracing.
FMaxisMeshObject PrepareModel(const FMaxisMeshObject& Original);
FString ResolveCityName(const FString& SourceFile, const FString& MapName);
UMaterialInstanceDynamic* CreateMaterial(UObject* Owner, UMaterialInterface* Parent, const FString& CityName);
}
