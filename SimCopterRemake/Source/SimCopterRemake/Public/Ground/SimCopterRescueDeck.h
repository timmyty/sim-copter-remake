#pragma once
#include "CoreMinimal.h"
class UProceduralMeshComponent;

namespace SimCopterRescueDeck
{
	// Vertical intersection against the rendered triangles; ambient hulls have no physics body.
	bool FindSurface(const UProceduralMeshComponent* Mesh, const FVector& Candidate, FVector& OutFeet);
}
