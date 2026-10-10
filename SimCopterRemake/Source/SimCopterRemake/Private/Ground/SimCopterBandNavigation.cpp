#include "Ground/SimCopterBandNavigation.h"

namespace SimCopterBandNavigation
{
int32 GetOriginalBandCount(int32 CityIndex)
{
	// SCHOOK: SpawnVictoryBand 0x004c2a20 -> SpawnTubaGroup 0x004c29d0.
	// DAT_00518d64 is the zero-based career CITY index (FUN_00408210), not the level.
	const int32 City = FMath::Clamp(CityIndex, 0, 29);
	return FMath::Max(1, City / 8) * (City / 2 + 5);
}

int32 GetFormationIndex(float Elapsed)
{
	return FMath::FloorToInt(FMath::Max(0.0f, Elapsed) / PhaseSeconds) % 3;
}

FVector FormationOffset(int32 Member, int32 Count, int32 Formation, float Radius)
{
	// Keep the same row/column ordering between shapes, so members don't repeatedly cross
	// the whole crowd. Larger original-game bands grow outward instead of packing tighter.
	const int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(float(FMath::Max(1, Count)))));
	const int32 Row = Member / Columns;
	const int32 Column = Member % Columns;
	const int32 RowCount = FMath::Min(Columns, Count - Row * Columns);
	const float Lateral = Column - (RowCount - 1) * 0.5f;
	Radius = FMath::Max(PlayerClearance, Radius);
	if (Formation % 3 == 1)
	{
		const float ArcRadius = FMath::Max(Radius + 80.0f, Columns * 150.0f / 1.4f) + Row * 160.0f;
		const float Angle = Lateral * 150.0f / ArcRadius;
		return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * ArcRadius;
	}
	if (Formation % 3 == 2)
	{
		return FVector(Radius + 60.0f + Row * 160.0f, Lateral * 160.0f + (Row % 2) * 55.0f, 0);
	}
	// Stable irregular spacing reads as a loose gathering, without jittering every update.
	return FVector(Radius + 35.0f + Row * 170.0f + (Member * 37 % 61),
		Lateral * 165.0f + (Member * 53 % 41) - 20.0f, 0);
}

bool KeepsDistance(const FVector& From, const FVector& To, const FVector& Center, float Radius)
{
	const FVector Start(From.X - Center.X, From.Y - Center.Y, 0);
	const FVector Delta(To.X - From.X, To.Y - From.Y, 0);
	const double RadiusSquared = FMath::Square(Radius);
	if (Start.SizeSquared() < RadiusSquared)
	{
		// The player can step into the group. Let musicians back out of that overlap.
		return FVector::DotProduct(Start, Delta) >= 0 &&
			(Start + Delta).SizeSquared() > Start.SizeSquared() + UE_SMALL_NUMBER;
	}
	const double Alpha = Delta.IsNearlyZero() ? 0.0 :
		FMath::Clamp(-FVector::DotProduct(Start, Delta) / Delta.SizeSquared(), 0.0, 1.0);
	return (Start + Delta * Alpha).SizeSquared() >= RadiusSquared;
}

bool FindPath(const FVector& Start, const FVector& Goal,
	TFunctionRef<bool(const FVector&, const FVector&)> CanWalk, TArray<FVector>& OutPath)
{
	OutPath.Reset();
	if (CanWalk(Start, Goal)) { OutPath.Add(Goal); return true; }
	constexpr float CellSize = 75;
	constexpr int32 Radius = 48;
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
		// Once around a wall, connect to a visible distant goal. Requiring the goal to be
		// inside the local grid stranded musicians on the far side of the airport.
		if ((Iteration % 8 == 0 || FVector::Dist2D(Here, Goal) < CellSize * 1.5f) &&
			CanWalk(Here, Goal)) { Last = Best; break; }
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
	// Remove grid zigzags only where the physical walk probe accepts the full shortcut.
	FVector Previous = Start;
	for (int32 I = 0; I < OutPath.Num(); ++I)
	{
		int32 LastVisible = I;
		for (int32 J = OutPath.Num() - 1; J > I; --J)
		{
			if (CanWalk(Previous, OutPath[J])) { LastVisible = J; break; }
		}
		if (LastVisible > I) OutPath.RemoveAt(I, LastVisible - I);
		Previous = OutPath[I];
	}
	return true;
}
}
