// Decorative counterparts of the objects in DHANGAR, built in hangar-local space.
#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UProceduralMeshComponent;

namespace SimCopterHangarInterior
{
void Build(UProceduralMeshComponent& Mesh, UMaterialInterface* Material, const FString& OriginalRoot, float TileSizeCm);
// Solid shell, architectural detail, and separate light-transmitting glazing.
void BuildStructure(UProceduralMeshComponent& Shell, UProceduralMeshComponent& Glazing,
	UMaterialInterface* Material, UMaterialInterface* GlassMaterial, float TileSizeCm);
}
