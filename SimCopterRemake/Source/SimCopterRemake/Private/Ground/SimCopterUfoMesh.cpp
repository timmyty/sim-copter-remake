#include "Ground/SimCopterUfoMesh.h"

void SimCopterUfoMesh::Build(const FMaxisMeshObject& Object, const TArray<FColor>* Palette,
	float UnitsPerCentimeter, float Scale, TArray<FMaxisMeshSection>& OutSections)
{
	// Original SIM3D2.MAX object 0x17c: authored hull, centre structure and aerial.
	// Type-11 effect cards must not become opaque hull; type-25 lights are rendered
	// separately by USimCopterFlashingLightsComponent (FUN_00496c00).
	FMaxisMeshObject Hull = Object;
	Hull.Faces.RemoveAll([](const FMaxisMeshFace& Face) { return Face.FaceType == 11 || Face.FaceType == 25; });
	OutSections.SetNum(1);
	FMaxisProceduralMeshBuilder::BuildPaletteColoredSection(Hull, Palette, UnitsPerCentimeter,
		Scale, true, FLinearColor(0.15f, 0.15f, 0.2f), OutSections[0]);
}
