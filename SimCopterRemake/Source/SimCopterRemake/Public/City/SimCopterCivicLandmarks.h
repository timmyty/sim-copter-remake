#pragma once

#include "Formats/MaxisMeshReader.h"

namespace SimCopterCivicLandmarks
{
constexpr int32 CityHallObjectId = 0x00d; // CH208, XBLD 208.
constexpr int32 CityHallSectionKey = -207;
FMaxisMeshObject MakeCityHall(const FMaxisMeshObject& Original);
bool IsPier(int32 ObjectId);
float PierOriginZ(float WaterZ, float UnitsPerCentimeter, float MeshScale, float TileSize, float WaveAmplitude);
}
