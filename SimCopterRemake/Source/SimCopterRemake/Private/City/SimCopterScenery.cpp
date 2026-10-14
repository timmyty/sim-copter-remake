#include "City/SimCopterScenery.h"

namespace SimCopterScenery
{
int32 ParkVariantForTile(int32 X, int32 Y)
{
	// Stable across rebuilds and saves without consuming the simulation's random stream.
	uint32 Hash = static_cast<uint32>(X) * 0x8da6b343u ^ static_cast<uint32>(Y) * 0xd8163841u;
	Hash ^= Hash >> 16; Hash *= 0x7feb352du; Hash ^= Hash >> 15;
	return Hash % ParkVariantCount;
}

bool IsBuildingBase(int32 Id)
{
	// FUN_00482890's separate residential, commercial and industrial foundation objects.
	return (Id >= 1 && Id <= 4) || (Id >= 0x164 && Id <= 0x16b);
}

bool IsParkPond(const FMaxisMeshFace& Face)
{
	return Face.FaceType == 18 && Face.TextureAtlasIndex == 20 && Face.MaterialIndex == 0;
}

FMaxisMeshObject MakeParkVariant(const FMaxisMeshObject& Original, int32 Variant)
{
	FMaxisMeshObject Result = Original;
	Variant = FMath::Clamp(Variant, 0, ParkVariantCount - 1);
	if (Variant == 0) return Result; // One in six keeps LP13's original pond.
	Result.Faces.RemoveAll([](const FMaxisMeshFace& Face) { return IsParkPond(Face); });
	if (Variant == 1 && Result.Faces.Num() > 2) Result.Faces.SetNum(2);
	// Move the sprite centres, not their corner vectors: rotating those would change tree widths.
	const FIntPoint Centres[5][3] = {
		{{-18,-15},{17,18},{0,0}}, {{-20,17},{19,10},{-7,-20}},
		{{-19,-14},{18,15},{-18,20}}, {{-18,17},{18,-17},{19,19}},
		{{-18,-19},{18,19},{-19,18}}
	};
	int32 Tree = 0;
	for (const auto& Face : Result.Faces)
	{
		if (Face.FaceType != 2 || Face.VertexIndices.Num() != 2 || Tree >= 3) continue;
		auto& Centre = Result.Vertices[Face.VertexIndices[0]];
		auto& Corner = Result.Vertices[Face.VertexIndices[1]];
		const int32 DX = Centres[Variant-1][Tree].X * 65536 - Centre.X;
		const int32 DZ = Centres[Variant-1][Tree].Y * 65536 - Centre.Z;
		Centre.X += DX; Corner.X += DX; Centre.Z += DZ; Corner.Z += DZ;
		++Tree;
	}
	// Presentation-only footpaths sit just above the flattened park terrain. The original model
	// contains only trees and water; removing its pond exposes the existing grass underneath.
	auto Path = [&](int32 X0, int32 Z0, int32 X1, int32 Z1)
	{
		const uint16 Start = Result.Vertices.Num();
		for (const FIntPoint P : {FIntPoint(X0,Z0),FIntPoint(X0,Z1),FIntPoint(X1,Z1),FIntPoint(X1,Z0)})
			Result.Vertices.Add({P.X*65536, 8192, P.Y*65536});
		FMaxisMeshFace Face;
		Face.FaceType = 15; Face.MaterialIndex = 48; Face.VertexCount = 4;
		Face.VertexIndices = {Start, static_cast<uint16>(Start+1), static_cast<uint16>(Start+2), static_cast<uint16>(Start+3)};
		Face.RawUVs = {{0,0},{0,65536},{65536,65536},{65536,0}};
		Result.Faces.Add(MoveTemp(Face));
	};
	if (Variant >= 3) Path(-4,-32,4,32);
	if (Variant == 4) Path(4,-4,32,4);
	if (Variant == 5) { Path(-32,-4,-4,4); Path(4,-4,32,4); }
	Result.Header.VertexCount = Result.Vertices.Num();
	Result.Header.FaceCount = Result.Faces.Num();
	return Result;
}
}
