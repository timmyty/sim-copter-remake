#include "Ground/SimCopterBandNavigation.h"

namespace SimCopterBandNavigation
{
bool IsFormationPhase(float Elapsed)
{
	return (FMath::FloorToInt(FMath::Max(0.0f, Elapsed) / PhaseSeconds) % 2) == 0;
}

FVector FormationOffset(int32 Member, int32 Count, int32 Formation)
{
	constexpr float Spacing = 75;
	if (Formation % 3 == 0) return FVector(0, (Member - (Count - 1) * 0.5f) * Spacing, 0);
	if (Formation % 3 == 1) return FVector(-(Member / 2) * Spacing, (Member % 2 - 0.5f) * Spacing, 0);
	return FVector(-FMath::Abs(Member - (Count - 1) * 0.5f) * Spacing,
		(Member - (Count - 1) * 0.5f) * Spacing, 0);
}

bool FindPath(const FVector& Start, const FVector& Goal,
	TFunctionRef<bool(const FVector&, const FVector&)> CanWalk, TArray<FVector>& OutPath)
{
	OutPath.Reset();
	if (CanWalk(Start, Goal)) { OutPath.Add(Goal); return true; }
	constexpr float CellSize = 75;
	constexpr int32 Radius = 24;
	struct FNode { FIntPoint Cell; int32 Parent; float Cost; bool bClosed = false; };
	TArray<FNode> Nodes;
	TMap<FIntPoint, int32> Indices;
	Nodes.Add({FIntPoint::ZeroValue, INDEX_NONE, 0});
	Indices.Add(FIntPoint::ZeroValue, 0);
	auto Position = [&](const FIntPoint& Cell) { return Start + FVector(Cell.X * CellSize, Cell.Y * CellSize, 0); };
	const FIntPoint Directions[] = {{1,0},{0,1},{-1,0},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
	int32 Last = INDEX_NONE;
	for (int32 Iteration = 0; Iteration < 1024; ++Iteration)
	{
		int32 Best = INDEX_NONE;
		float BestScore = TNumericLimits<float>::Max();
		for (int32 I = 0; I < Nodes.Num(); ++I)
		{
			const float Score = Nodes[I].Cost + FVector::Dist2D(Position(Nodes[I].Cell), Goal);
			if (!Nodes[I].bClosed && Score < BestScore) { Best = I; BestScore = Score; }
		}
		if (Best == INDEX_NONE) break;
		Nodes[Best].bClosed = true;
		const FNode Current = Nodes[Best];
		const FVector Here = Position(Current.Cell);
		if (FVector::Dist2D(Here, Goal) < CellSize * 1.5f && CanWalk(Here, Goal)) { Last = Best; break; }
		for (const auto& Direction : Directions)
		{
			const FIntPoint Cell = Current.Cell + Direction;
			if (FMath::Abs(Cell.X) > Radius || FMath::Abs(Cell.Y) > Radius) continue;
			const FVector Next = Position(Cell);
			const float Cost = Current.Cost + FVector::Dist2D(Here, Next);
			int32* Existing = Indices.Find(Cell);
			if (Existing && (Nodes[*Existing].bClosed || Nodes[*Existing].Cost <= Cost)) continue;
			if (!CanWalk(Here, Next)) continue;
			if (Existing) { Nodes[*Existing].Parent = Best; Nodes[*Existing].Cost = Cost; }
			else { Indices.Add(Cell, Nodes.Num()); Nodes.Add({Cell, Best, Cost}); }
		}
	}
	if (Last == INDEX_NONE) return false;
	OutPath.Add(Goal);
	for (int32 I = Last; I > 0; I = Nodes[I].Parent) OutPath.Insert(Position(Nodes[I].Cell), 0);
	return true;
}
}
