#pragma once

#include "Formats/MaxisProceduralMeshBuilder.h"

namespace SimCopterUfoMesh
{
	// User-requested replacement art. Section 0 hull, 1 canopy, 2 drive lights.
	SIMCOPTERREMAKE_API void Build(float RadiusCm, TArray<FMaxisMeshSection>& OutSections);
}
