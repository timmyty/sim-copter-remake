#include "Ground/SimCopterBandNavigation.h"

#include "Ground/SimCopterTrafficSystemActor.h"

namespace SimCopterBandNavigation
{
bool TryGetBaseArea(const ASimCopterTrafficSystemActor* Traffic, FBaseArea& OutArea)
{
	// FUN_004829f0's 4x4 airport, using the same pad basis as hangar placement.
	// Confining the remake choreography to that base is a requested adaptation.
	FVector Origin, PadX, PadY;
	if (!Traffic || !Traffic->TryGetAirportPadWorldLocation(8, Origin) ||
		!Traffic->TryGetAirportPadWorldLocation(9, PadX) ||
		!Traffic->TryGetAirportPadWorldLocation(3, PadY)) return false;
	const FVector X = (PadX - Origin) / 3.0f;
	const FVector Y = (PadY - Origin) / 3.0f;
	if (X.IsNearlyZero() || Y.IsNearlyZero()) return false;
	OutArea.Center = Origin + (X + Y) * 1.5f;
	OutArea.AxisX = X.GetSafeNormal2D();
	OutArea.AxisY = Y.GetSafeNormal2D();
	OutArea.HalfExtent = FVector2D(X.Size2D(), Y.Size2D()) * 2.0;
	return true;
}

FVector FBaseArea::Clamp(const FVector& Location, float Inset) const
{
	const FVector Delta = Location - Center;
	const double X = FMath::Max(0.0, HalfExtent.X - Inset);
	const double Y = FMath::Max(0.0, HalfExtent.Y - Inset);
	FVector Result = Center + AxisX * FMath::Clamp(FVector::DotProduct(Delta, AxisX), -X, X) +
		AxisY * FMath::Clamp(FVector::DotProduct(Delta, AxisY), -Y, Y);
	Result.Z = Location.Z;
	return Result;
}

bool FBaseArea::Contains(const FVector& Location, float Inset) const
{
	return FVector::DistSquared2D(Location, Clamp(Location, Inset)) < UE_KINDA_SMALL_NUMBER;
}

bool FBaseArea::AllowsStep(const FVector& From, const FVector& To, float Inset) const
{
	if (Contains(To, Inset)) return true; // A segment between in-bounds points stays inside this rectangle.
	// Older saves can restore stragglers outside. Permit physical return, never further drift.
	return FVector::DistSquared2D(To, Clamp(To, Inset)) + UE_KINDA_SMALL_NUMBER <
		FVector::DistSquared2D(From, Clamp(From, Inset));
}

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

int32 GetCelebrationBandCount(int32 CityIndex)
{
	// Requested first-island greeting: a leader and two musicians, including restored completions.
	return CityIndex <= 0 ? 3 : GetOriginalBandCount(CityIndex);
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

void FPathSearch::Reset()
{
	Phase = EPhase::Idle;
	Nodes.Reset(); Indices.Reset(); Result.Reset();
	Iteration = SmoothIndex = SmoothCandidate = 0;
}

void FPathSearch::Begin(const FVector& Start, const FVector& Goal)
{
	Reset(); Origin = Start; Destination = Goal; Phase = EPhase::Direct;
}

bool FPathSearch::Advance(TFunctionRef<bool(const FVector&, FVector&)> CanWalk, TArray<FVector>& OutPath,
	int32 MaxWork, double MaxSeconds)
{
	OutPath.Reset();
	const double BeginTime = FPlatformTime::Seconds();
	constexpr float CellSize = 75;
	constexpr int32 Radius = 48;
	const FIntPoint Directions[] = {{1,0},{0,1},{-1,0},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
	auto Finish = [&]() { OutPath = MoveTemp(Result); Reset(); return !OutPath.IsEmpty(); };
	// Check the clock between bounded pieces of work, including path smoothing. Physics
	// queries stay on the game thread; no worker reads a changing world or actor pointer.
	for (int32 Work = 0; Work < MaxWork && IsPending(); ++Work)
	{
		if (Work > 0 && MaxSeconds > 0 && FPlatformTime::Seconds() - BeginTime >= MaxSeconds) break;
		if (Phase == EPhase::Direct)
		{
			FVector GroundGoal = Destination;
			if (CanWalk(Origin, GroundGoal)) { Result.Add(GroundGoal); return Finish(); }
			Nodes.Add({FIntPoint::ZeroValue, INDEX_NONE, 0, Origin});
			Indices.Add(FIntPoint::ZeroValue, 0); Phase = EPhase::Search;
		}
		else if (Phase == EPhase::Search)
		{
			if (Iteration >= 1024) { Reset(); return false; }
			int32 Best = INDEX_NONE;
			float BestScore = TNumericLimits<float>::Max();
			for (int32 I = 0; I < Nodes.Num(); ++I)
			{
				const float Score = Nodes[I].Cost + FVector::Dist2D(Nodes[I].Location, Destination);
				if (!Nodes[I].bClosed && Score < BestScore) { Best = I; BestScore = Score; }
			}
			if (Best == INDEX_NONE) { Reset(); return false; }
			Nodes[Best].bClosed = true;
			const FNode Current = Nodes[Best];
			const FVector Here = Current.Location;
			FVector GroundGoal = Destination;
			if ((Iteration % 8 == 0 || FVector::Dist2D(Here, Destination) < CellSize * 1.5f) && CanWalk(Here, GroundGoal))
			{
				Result.Add(GroundGoal);
				for (int32 I = Best; I > 0; I = Nodes[I].Parent) Result.Insert(Nodes[I].Location, 0);
				Previous = Origin; SmoothCandidate = Result.Num() - 1; Phase = EPhase::Smooth;
				continue;
			}
			for (const auto& Direction : Directions)
			{
				const FIntPoint Cell = Current.Cell + Direction;
				if (FMath::Abs(Cell.X) > Radius || FMath::Abs(Cell.Y) > Radius) continue;
				FVector Next = Origin + FVector(Cell.X * CellSize, Cell.Y * CellSize, 0);
				Next.Z = Here.Z;
				const float Cost = Current.Cost + FVector::Dist2D(Here, Next);
				int32* Existing = Indices.Find(Cell);
				if (Existing && (Nodes[*Existing].bClosed || Nodes[*Existing].Cost <= Cost)) continue;
				if (!CanWalk(Here, Next)) continue;
				if (Existing) { Nodes[*Existing].Parent = Best; Nodes[*Existing].Cost = Cost; Nodes[*Existing].Location = Next; }
				else { Indices.Add(Cell, Nodes.Num()); Nodes.Add({Cell, Best, Cost, Next}); }
			}
			++Iteration;
		}
		else if (Phase == EPhase::Smooth)
		{
			if (SmoothIndex >= Result.Num()) return Finish();
			FVector Candidate = Result[SmoothCandidate];
			if (SmoothCandidate > SmoothIndex && !CanWalk(Previous, Candidate)) { --SmoothCandidate; continue; }
			Result[SmoothCandidate] = Candidate;
			if (SmoothCandidate > SmoothIndex) Result.RemoveAt(SmoothIndex, SmoothCandidate - SmoothIndex);
			Previous = Result[SmoothIndex++]; SmoothCandidate = Result.Num() - 1;
		}
	}
	return false;
}

bool FindPath(const FVector& Start, const FVector& Goal,
	TFunctionRef<bool(const FVector&, FVector&)> CanWalk, TArray<FVector>& OutPath)
{
	FPathSearch Search;
	Search.Begin(Start, Goal);
	while (Search.IsPending())
		if (Search.Advance(CanWalk, OutPath, 4096, 0)) return true;
	return false;
}
}
