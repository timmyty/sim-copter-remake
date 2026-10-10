#pragma once
#include "CoreMinimal.h"

// Remake parade choreography; BHAV 443/444 still owns the instrument animation and music.
namespace SimCopterBandNavigation
{
	constexpr float PhaseSeconds = 10.0f;
	constexpr float MemberSpacing = 100.0f;
	constexpr float PlayerClearance = 250.0f;
	int32 GetOriginalBandCount(int32 CityIndex);
	int32 GetFormationIndex(float Elapsed);
	FVector FormationOffset(int32 Member, int32 Count, int32 Formation, float Radius = 250.0f);
	bool KeepsDistance(const FVector& From, const FVector& To, const FVector& Center, float Radius);
	// Bounded local A*: every edge goes through the same physical walk probe as the agents.
	bool FindPath(const FVector& Start, const FVector& Goal,
		TFunctionRef<bool(const FVector&, const FVector&)> CanWalk, TArray<FVector>& OutPath);
}
