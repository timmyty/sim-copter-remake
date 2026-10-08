#include "Ground/SimCopterUfoMesh.h"

namespace
{
void AddRingSurface(FMaxisMeshSection& Mesh, const TArray<FVector2D>& Profile,
	const float Radius, const FLinearColor& Color)
{
	constexpr int32 Segments = 64;
	for (int32 Ring = 0; Ring + 1 < Profile.Num(); ++Ring)
	{
		const FVector2D A = Profile[Ring];
		const FVector2D B = Profile[Ring + 1];
		const FVector2D Edge = B - A;
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const int32 Base = Mesh.Vertices.Num();
			const float Angles[] = { Segment * UE_TWO_PI / Segments, (Segment + 1) * UE_TWO_PI / Segments };
			const FVector2D Points[] = { A, A, B, B };
			const int32 AngleIndices[] = { 0, 1, 1, 0 };
			for (int32 Corner = 0; Corner < 4; ++Corner)
			{
				const float Angle = Angles[AngleIndices[Corner]];
				const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0);
				const FVector Vertex = Radius * (Radial * Points[Corner].X + FVector::UpVector * Points[Corner].Y);
				Mesh.Vertices.Add(Vertex);
				Mesh.Normals.Add((Radial * Edge.Y - FVector::UpVector * Edge.X).GetSafeNormal());
				Mesh.UVs.Add(FVector2D(0.5 + Vertex.X / (2.04 * Radius), 0.5 + Vertex.Y / (2.04 * Radius)));
				Mesh.VertexColors.Add(Color);
				Mesh.Tangents.Add(FProcMeshTangent(-FMath::Sin(Angle), FMath::Cos(Angle), 0));
				Mesh.LocalBounds += Vertex;
			}
			if (A.X > 0.0f) Mesh.Triangles.Append({ Base, Base + 1, Base + 2 });
			if (B.X > 0.0f) Mesh.Triangles.Append({ Base, Base + 2, Base + 3 });
		}
	}
}
}

void SimCopterUfoMesh::Build(const float RadiusCm, TArray<FMaxisMeshSection>& OutSections)
{
	OutSections.SetNum(3);
	for (auto& Section : OutSections) Section.Reset();
	const float Radius = FMath::Max(1.0f, RadiusCm);
	// A broad lenticular hull, a raised canopy, and a distinct drive ring remain legible in flight.
	AddRingSurface(OutSections[0], {
		{0, -0.20f}, {0.30f, -0.20f}, {0.70f, -0.12f}, {0.96f, -0.035f},
		{1.0f, 0}, {0.96f, 0.045f}, {0.72f, 0.17f}, {0.34f, 0.26f}, {0, 0.26f}
	}, Radius, FLinearColor(0.65f, 0.7f, 0.75f));
	TArray<FVector2D> Dome;
	for (int32 Ring = 0; Ring <= 12; ++Ring)
	{
		const float Angle = Ring * UE_HALF_PI / 12.0f;
		Dome.Add(FVector2D(Ring == 12 ? 0.0f : 0.32f * FMath::Cos(Angle), 0.259f + 0.24f * FMath::Sin(Angle)));
	}
	AddRingSurface(OutSections[1], Dome, Radius, FLinearColor(0.015f, 0.1f, 0.13f));
	AddRingSurface(OutSections[2], { {0.975f, -0.028f}, {1.005f, -0.006f}, {1.005f, 0.008f}, {0.975f, 0.031f} },
		Radius, FLinearColor(0.02f, 0.9f, 1.0f));
	AddRingSurface(OutSections[2], { {0.32f, -0.203f}, {0.45f, -0.17f} },
		Radius, FLinearColor(0.02f, 0.9f, 1.0f));
}
