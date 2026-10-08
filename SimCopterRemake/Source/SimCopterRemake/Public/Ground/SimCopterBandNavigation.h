#pragma once
#include "CoreMinimal.h"

// Remake parade choreography. The original BHAV 443/444 still owns music and free following.
namespace SimCopterBandNavigation
{
	constexpr float PhaseSeconds = 150.0f;
	bool IsFormationPhase(float Elapsed);
	FVector FormationOffset(int32 Member, int32 Count, int32 Formation);
	// Bounded local A*: every edge goes through the same physical walk probe as the agents.
	bool FindPath(const FVector& Start, const FVector& Goal,
		TFunctionRef<bool(const FVector&, const FVector&)> CanWalk, TArray<FVector>& OutPath);
}
