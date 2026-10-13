#include "Ground/SimCopterWalkSupport.h"
#include "Engine/World.h"

bool SimCopterWalkSupport::FindSurface(UWorld* World, const FVector& Feet, const float Radius,
	const float MaxRise, const float MaxDrop, const FCollisionQueryParams& Query, FVector& OutFeet)
{
	if (!World) return false;
	for (int32 Sample = 0; Sample < 9; ++Sample)
	{
		const float Angle = (Sample - 1) * PI / 4.0f;
		const FVector Point = Feet + (Sample == 0 ? FVector::ZeroVector :
			FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Point + FVector(0,0,MaxRise + 1),
			Point - FVector(0,0,MaxDrop + 1), ECC_Camera, Query) || !Hit.bBlockingHit ||
			Hit.bStartPenetrating || Hit.ImpactNormal.Z < 0.7f ||
			Hit.ImpactPoint.Z > Feet.Z + MaxRise || Hit.ImpactPoint.Z < Feet.Z - MaxDrop) return false;
		if (Sample == 0) OutFeet = Hit.ImpactPoint;
	}
	return true;
}
