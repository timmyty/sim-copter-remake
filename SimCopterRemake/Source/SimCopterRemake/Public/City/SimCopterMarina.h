#pragma once

#include "Formats/MaxisMeshReader.h"
#include "Formats/SimCity2000Reader.h"

namespace SimCopterMarina
{
constexpr int32 ObjectId = 0x0f3; // MA248, dispatched by FUN_0047c0c0 for XBLD 0xf8.
constexpr uint8 BuildingId = 0xf8;
constexpr uint8 RoadApronFinish = 4;
struct FLayout
{
	uint16 LandMask = 0;
	uint16 RoadMask = 0;
};
// Returns layouts keyed by claimed footprint origin. Only valid marina footprints are changed;
// surrounding roads/land keep their existing terrain. Masks use file-grid row-major 3x3 cells.
TMap<int32, FLayout> PrepareTerrain(const FSimCity2000City& City, TArray<uint8>& TerrainTypes);
FMaxisMeshObject MakeModel(const FMaxisMeshObject& Original, const FLayout& Layout);
}
