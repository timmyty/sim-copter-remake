#include "Flight/SimCopterHelicopterPresentation.h"

namespace SimCopterHelicopterPresentation
{
void ApplyCatalogPaintRegions(FMaxisMeshObject& Body, int32 TypeIndex)
{
	// The catalog uses the same shell but different accent placement on these three aircraft.
	// Coordinates here are GEO source meters (262144 units/m), before the display scale.
	const auto SourceFaces = Body.Faces;
	Body.Faces.Reset();
	for (auto Face : SourceFaces)
	{
		if (Face.MaterialIndex == 0 || Face.MaterialIndex >= 240 || Face.VertexIndices.Num() < 3)
		{
			Body.Faces.Add(Face); continue;
		}
		FVector Center = FVector::ZeroVector;
		TArray<FVector> Points;
		for (auto Index : Face.VertexIndices)
		{
			const auto& V = Body.Vertices[Index];
			const FVector P(V.X/262144.0,V.Y/262144.0,V.Z/262144.0);
			Center += P; Points.Add(P);
		}
		Center /= Points.Num();
		const bool bJetBelly = TypeIndex == 0 && Face.MaterialIndex == 160 && Center.Z > -1.5;
		const bool bDauphinBelly = TypeIndex == 6 && Face.MaterialIndex == 160;
		if (TypeIndex == 1 && ((Face.MaterialIndex == 160 && Center.Y < 0.6) ||
			(Face.MaterialIndex == 32 && Center.Y > 2.2 && Center.Z > -1.1)))
			Face.MaterialIndex = 176; // MD 500's red skids and upper engine accent.
		if (TypeIndex != 7 && !bJetBelly && !bDauphinBelly)
		{
			Body.Faces.Add(Face); continue;
		}
		// Explorer: navy cabin, thin white separator, red belly; keep the red tailplane tips.
		if (TypeIndex == 7 && (Center.Z < -2.0 || Center.Y < 0.43))
		{
			if (Face.MaterialIndex == 32) Face.MaterialIndex = 176;
			Body.Faces.Add(Face); continue;
		}
		auto Clip = [](const TArray<FVector>& Polygon, double Height, bool bAbove)
		{
			TArray<FVector> Result;
			if (Polygon.IsEmpty()) return Result;
			FVector Previous = Polygon.Last();
			bool bPreviousInside = bAbove ? Previous.Y >= Height : Previous.Y <= Height;
			for (const FVector& Current : Polygon)
			{
				const bool bInside = bAbove ? Current.Y >= Height : Current.Y <= Height;
				if (bInside != bPreviousInside)
					Result.Add(FMath::Lerp(Previous,Current,(Height-Previous.Y)/(Current.Y-Previous.Y)));
				if (bInside) Result.Add(Current);
				Previous = Current; bPreviousInside = bInside;
			}
			return Result;
		};
		auto AddBand = [&](const TArray<FVector>& Polygon, uint8 Material)
		{
			if (Polygon.Num() < 3) return;
			auto Part = Face; Part.MaterialIndex = Material; Part.VertexIndices.Reset();
			for (const FVector& V : Polygon)
			{
				Part.VertexIndices.Add(uint16(Body.Vertices.Num()));
				Body.Vertices.Add({int32(FMath::RoundToInt(V.X*262144)),int32(FMath::RoundToInt(V.Y*262144)),int32(FMath::RoundToInt(V.Z*262144))});
			}
			Part.VertexCount = uint16(Part.VertexIndices.Num()); Body.Faces.Add(Part);
		};
		if (bJetBelly || bDauphinBelly)
		{
			const double Height = bJetBelly ? 0.8 : 0.95;
			AddBand(Clip(Points,Height,false),bJetBelly ? 32 : 16);
			AddBand(Clip(Points,Height,true),160);
			continue;
		}
		AddBand(Clip(Points,0.73,false),160);
		AddBand(Clip(Clip(Points,0.73,true),0.83,false),32);
		AddBand(Clip(Points,0.83,true),176);
	}
}

void MakePaintPalette(const TArray<FColor>& Source, TArray<FColor>& Out, int32 TypeIndex)
{
	Out = Source;
	// GEO bodies use ramp selectors (face types 15/19), not pre-shaded albedo.
	// The old universal +12 stop washed blue/teal/purple into pastels and made the
	// Apache pale green. Mid-ramp chromatic paint keeps the authored livery hues;
	// the neutral ramps use lighter stops for white/silver trim. Unreal supplies light.
	for (int32 Base = 32; Base < 224; Base += 16)
	{
		const int32 Stop = (Base == 48 || Base == 176) ? 12 : 8;
		if (Source.IsValidIndex(Base + Stop)) Out[Base] = Source[Base + Stop];
	}
	// The 16 ramp runs RED -> orange -> yellow, unlike the other shade ramps.
	// Keep red paint red; never move it to the yellow end used by fire/tracers.
	if (Out.IsValidIndex(16)) Out[16] = FColor(198, 30, 18);
	// The authored GEO ramp ids describe paint regions, but its default palette is a
	// different livery from the shop drawings. Match each CAT_*.BMP without recoloring
	// tires, windows (0), navigation lights (240+) or separately built equipment.
	auto Paint = [&](int32 Index, FColor Color) { if (Out.IsValidIndex(Index)) Out[Index] = Color; };
	switch (TypeIndex)
	{
	case 0: // CAT_JET: blue roof/tail, silver cabin, burgundy lower trim.
		Paint(16, FColor(38,65,151)); Paint(160, FColor(184,189,195)); Paint(32, FColor(83,30,53)); break;
	case 1: // CAT_HUGH: silver body with red accent panels.
		Paint(160, FColor(168,174,182)); Paint(32, FColor(130,141,153)); Paint(176, FColor(173,34,50)); break;
	case 3: // CAT_BELL: teal trim and silver side panels.
		Paint(80, FColor(54,143,153)); Paint(128, FColor(194,198,200)); break;
	case 4: // CAT_SCHW: tan canopy frame/tail with green tubular structure.
		Paint(16, FColor(196,161,105)); Paint(80, FColor(74,116,73)); break;
	case 5: // CAT_AUG: white cabin, blue-gray belly, engine housing and tail.
		Paint(176, FColor(48,65,76)); break;
	case 6: // CAT_DAUP: warm gold upper body/tail around a white cabin band.
		Paint(16, FColor(190,161,91)); Paint(160, FColor(218,218,207)); break;
	case 7: // CAT_MDE: navy upper body and boom, white separator, red belly.
		Paint(176, FColor(39,53,84)); Paint(32, FColor(215,215,209)); Paint(160, FColor(178,31,61)); break;
	case 8: // CAT_MD5: royal blue upper body/tail, silver-white cabin.
		Paint(192, FColor(41,50,148)); Paint(64, FColor(182,191,202)); Paint(176, FColor(207,211,214)); break;
	default: break;
	}
	// The Apache's tail and cockpit frames are military trim, not civilian silver.
	if (TypeIndex == 2 && Out.IsValidIndex(48)) Out[48] = FColor(58, 64, 48);
}

bool IsAgustaWindow(const FMaxisMeshObject& Object, const FMaxisMeshFace& Face)
{
	// Verified against AGUSTA 0x141: the cockpit/door glazing is material 0 above
	// 0.7m and forward of the mast. This excludes all three wheels and the exhaust.
	if (Face.MaterialIndex != 0 || Face.VertexIndices.Num() < 3) return false;
	for (uint16 Index : Face.VertexIndices)
		if (!Object.Vertices.IsValidIndex(Index) || Object.Vertices[Index].Y < 0.7 * 262144 || Object.Vertices[Index].Z < 0) return false;
	return true;
}

FVector AgustaSeat(int32 Index)
{
	// Authored fuselage coordinates at the shipped 0.25 model scale; pilot plus seven passengers.
	const int32 Row = FMath::Clamp(Index, 0, 7) / 2;
	return FVector(Row == 0 ? 46 : 32 - (Row - 1) * 13, (Index % 2 == 0 ? -1 : 1) * 9, 17);
}

void AppendSeatedOccupant(const FVector& Seat, const FLinearColor& Shirt, bool bPatient, FMaxisMeshSection& Out)
{
	auto Box = [&](FVector Center, FVector Half, FLinearColor Color)
	{
		Center += Seat;
		const FVector Normals[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
		for (const FVector& N : Normals)
		{
			const FVector U = FMath::Abs(N.Z) < 0.5 ? FVector(0,0,1) : FVector(1,0,0);
			const FVector V = FVector::CrossProduct(N,U);
			const int32 First = Out.Vertices.Num();
			const FVector Corners[] = {N-U-V,N+U-V,N+U+V,N-U+V};
			for (const FVector& Corner : Corners)
			{
				const FVector P = Center + Corner * Half;
				Out.Vertices.Add(P); Out.Normals.Add(N); Out.UVs.Add(FVector2D::ZeroVector);
				Out.VertexColors.Add(Color); Out.Tangents.Add(FProcMeshTangent(U, false)); Out.LocalBounds += P;
			}
			Out.Triangles.Append({First,First+2,First+1,First,First+3,First+2});
		}
	};
	const FLinearColor Skin(0.65f,0.40f,0.24f);
	const FLinearColor Pants(0.06f,0.10f,0.16f);
	Box(FVector(-2,0,4),FVector(1,5,7),FLinearColor(0.07f,0.09f,0.11f)); // seat back
	Box(FVector(0,0,5),FVector(2,3.5,5),Shirt);
	Box(FVector(0,0,12),FVector(2.7,2.7,3),Skin);
	Box(FVector(0,0,14.7),FVector(2.8,2.8,0.7),bPatient ? FLinearColor::White : FLinearColor(0.05f,0.03f,0.02f));
	for (float Side : {-1.0f,1.0f})
	{
		Box(FVector(3,Side*2,0),FVector(4,1.5,1.5),Pants); // thighs
		Box(FVector(6,Side*2,-4),FVector(1.5,1.5,4),Pants);
		Box(FVector(2,Side*4,4),FVector(3,1,1),Shirt);
		Box(FVector(5,Side*4,4),FVector(1,1,1),Skin);
	}
}
}
