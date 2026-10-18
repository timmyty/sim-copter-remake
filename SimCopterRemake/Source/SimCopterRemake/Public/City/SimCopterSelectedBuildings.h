#pragma once
#include "Formats/MaxisMeshReader.h"

// Reference-authored SC2000 landmarks. GEO IDs, not XBLD IDs; intentionally an allowlist.
namespace SimCopterSelectedBuildings
{
constexpr int32 SectionKey = -210;
constexpr int32 GlassSectionKey = -211;
constexpr uint8 DetailTag = 242;
bool IsSelected(int32 ObjectId);
bool IsDetail(const FMaxisMeshFace& Face);
bool IsCausewayTile(int32 Xbld);
bool IsCausewayModel(int32 ObjectId);
FMaxisMeshObject MakeModel(const FMaxisMeshObject& Original);
FMaxisMeshObject MakeCauseway(const FMaxisMeshObject& Original, bool bSupport);
FLinearColor FinishColor(uint8 Finish);
}
