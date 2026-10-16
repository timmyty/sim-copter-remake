#pragma once

#include "Formats/MaxisMeshReader.h"

namespace SimCopterCivicLandmarks
{
constexpr int32 CityHallObjectId = 0x00d; // CH208, XBLD 208.
constexpr int32 CityHallSectionKey = -207;
FMaxisMeshObject MakeCityHall(const FMaxisMeshObject& Original);
bool IsPier(int32 ObjectId);
// Section key and finish tags for the authored wooden dock that replaces AP223/AP223F's solid
// painted hull (user request, v1.0.19). Tags travel in the face MaterialIndex.
constexpr int32 PierSectionKey = -208;
enum EPierFinish : uint8 { PierPlankLight = 0, PierPlankDark = 1, PierPiling = 2, PierRail = 3 };
FLinearColor PierFinishColor(uint8 Finish);
FMaxisMeshObject MakePier(const FMaxisMeshObject& Original);
float PierOriginZ(float WaterZ, float UnitsPerCentimeter, float MeshScale, float TileSize, float WaveAmplitude);
}
