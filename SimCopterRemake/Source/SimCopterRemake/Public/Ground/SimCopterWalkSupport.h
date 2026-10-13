#pragma once
#include "CoreMinimal.h"

class UWorld;
struct FCollisionQueryParams;

namespace SimCopterWalkSupport
{
	// A bounded probe of the feet and their full footprint, never a search down to street level.
	bool FindSurface(UWorld* World, const FVector& Feet, float Radius, float MaxRise, float MaxDrop,
		const FCollisionQueryParams& Query, FVector& OutFeet);
}
