#pragma once

#include "Formats/MaxisMeshReader.h"

namespace SimCopterEmergencyBuildings
{
constexpr int32 HospitalId = 0x016, PoliceId = 0x000, FireId = 0x0e3;
constexpr int32 SectionKey = -209;
constexpr uint8 DetailAtlasTag = 250;
enum EFinish : uint8 { Brick, Cyan, Orange, Stone, Dark, Metal, Glass, Gold };
bool IsEmergencyBuilding(int32 ObjectId);
bool IsDetail(const FMaxisMeshObject& Model, const FMaxisMeshFace& Face);
// Reference-based facade additions. Original roof vertices, faces, UVs and light markers survive.
FMaxisMeshObject MakeModel(const FMaxisMeshObject& Original, bool bUpdated = true);
}
