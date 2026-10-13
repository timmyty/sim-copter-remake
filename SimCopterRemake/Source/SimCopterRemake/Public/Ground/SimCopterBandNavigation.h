#pragma once
#include "CoreMinimal.h"
class ASimCopterTrafficSystemActor;

// Remake parade choreography; BHAV 443/444 still owns the instrument animation and music.
namespace SimCopterBandNavigation
{
	constexpr float PhaseSeconds = 10.0f;
	constexpr float MemberSpacing = 100.0f;
	constexpr float PlayerClearance = 250.0f;
	// Airport-aligned footprint; Z is deliberately left to the physical ground probes.
	struct FBaseArea
	{
		FVector Center = FVector::ZeroVector;
		FVector AxisX = FVector::ForwardVector;
		FVector AxisY = FVector::RightVector;
		FVector2D HalfExtent = FVector2D::ZeroVector;
		FVector Clamp(const FVector& Location, float Inset = 0) const;
		bool Contains(const FVector& Location, float Inset = 0) const;
		bool AllowsStep(const FVector& From, const FVector& To, float Inset) const;
	};
	bool TryGetBaseArea(const ASimCopterTrafficSystemActor* Traffic, FBaseArea& OutArea);
	int32 GetOriginalBandCount(int32 CityIndex);
	int32 GetCelebrationBandCount(int32 CityIndex);
	int32 GetFormationIndex(float Elapsed);
	FVector FormationOffset(int32 Member, int32 Count, int32 Formation, float Radius = 250.0f);
	bool KeepsDistance(const FVector& From, const FVector& To, const FVector& Center, float Radius);
	// A responder may pursue a moving aircraft or cycle between unreachable patients.
	// Retain the local search between updates instead of blocking a frame on 1024 nodes.
	class FPathSearch
	{
	public:
		void Begin(const FVector& Start, const FVector& Goal);
		void Reset();
		bool IsPending() const { return Phase != EPhase::Idle; }
		bool Advance(TFunctionRef<bool(const FVector&, FVector&)> CanWalk, TArray<FVector>& OutPath,
			int32 MaxWork = 32, double MaxSeconds = 0.001);
	private:
		enum class EPhase : uint8 { Idle, Direct, Search, Smooth };
		struct FNode { FIntPoint Cell; int32 Parent; float Cost; FVector Location; bool bClosed = false; };
		EPhase Phase = EPhase::Idle;
		FVector Origin = FVector::ZeroVector, Destination = FVector::ZeroVector, Previous = FVector::ZeroVector;
		TArray<FNode> Nodes;
		TMap<FIntPoint, int32> Indices;
		TArray<FVector> Result;
		int32 Iteration = 0, SmoothIndex = 0, SmoothCandidate = 0;
	};
	// Bounded local A*: each probe may resolve its endpoint Z to the supported walk surface.
	bool FindPath(const FVector& Start, const FVector& Goal,
		TFunctionRef<bool(const FVector&, FVector&)> CanWalk, TArray<FVector>& OutPath);
}
