#pragma once

#include "Formats/MaxisProceduralMeshBuilder.h"

namespace SimCopterUfoMesh
{
	// Original palette hull. Authored flashing light points are attached separately.
	SIMCOPTERREMAKE_API void Build(const FMaxisMeshObject& Object, const TArray<FColor>* Palette,
		float UnitsPerCentimeter, float Scale, TArray<FMaxisMeshSection>& OutSections);
}
