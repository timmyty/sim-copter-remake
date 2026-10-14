#pragma once

#include "Formats/MaxisMeshReader.h"

namespace SimCopterScenery
{
constexpr int32 ParkVariantCount = 6;
constexpr int32 PavingSection = -204;
constexpr int32 ParkPathSection = -205;
int32 ParkVariantForTile(int32 X, int32 Y);
bool IsBuildingBase(int32 ObjectId);
bool IsParkPond(const FMaxisMeshFace& Face);
FMaxisMeshObject MakeParkVariant(const FMaxisMeshObject& Original, int32 Variant);
}
