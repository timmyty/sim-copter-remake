#include "City/SimCopterDriveInPlacement.h"
#include "Formats/SimCity2000Reader.h"

namespace SimCopterDriveInPlacement
{
namespace
{
bool FindFoundation(const FSimCity2000City& City, const TArray<int16>& Corners, FIntPoint Origin, int16& OutHeight, int32& OutGrading)
{
	constexpr int32 N = FSimCity2000City::MapSize;
	int32 FixedHeight = INDEX_NONE, Sum = 0;
	for (int32 Y = Origin.Y; Y <= Origin.Y + Span; ++Y)
	for (int32 X = Origin.X; X <= Origin.X + Span; ++X)
	{
		const int32 Height = Corners[Y * (N + 1) + X];
		Sum += Height;
		for (int32 Dy = -1; Dy <= 0; ++Dy)
		for (int32 Dx = -1; Dx <= 0; ++Dx)
		{
			const int32 Tx = X + Dx, Ty = Y + Dy;
			if (Tx >= Origin.X && Tx < Origin.X + Span && Ty >= Origin.Y && Ty < Origin.Y + Span) continue;
			const auto& Neighbor = City.Tiles[Ty * N + Tx];
			// Shared corners under roads, buildings, parks, utilities and water cannot move.
			// Empty/tree-covered shoulders interpolate to their untouched outer vertices.
			if ((Neighbor.Building != 0 && !(Neighbor.Building >= 6 && Neighbor.Building <= 12)) ||
				Neighbor.bWater || Neighbor.Terrain > 0x0c)
			{
				if (FixedHeight != INDEX_NONE && FixedHeight != Height) return false;
				FixedHeight = Height;
			}
		}
	}
	const int32 Target = FixedHeight != INDEX_NONE ? FixedHeight : FMath::RoundToInt(Sum / (16.0f * 32.0f)) * 32;
	if (Target < 32 || Target > 32 * 32 || Target % 32 != 0) return false;
	OutGrading = 0;
	for (int32 Y = Origin.Y; Y <= Origin.Y + Span; ++Y)
	for (int32 X = Origin.X; X <= Origin.X + Span; ++X)
	{
		const int32 Delta = FMath::Abs(Corners[Y * (N + 1) + X] - Target);
		if (Delta > 32) return false; // One original altitude step: no excavating hillsides.
		OutGrading += Delta;
		if (X < Origin.X + Span && Y < Origin.Y + Span &&
			FMath::Abs((int32(City.Tiles[Y * N + X].Altitude) + 1) * 32 - Target) > 32) return false;
	}
	OutHeight = int16(Target);
	return true;
}
}

FIntPoint FindSite(const FSimCity2000City& City, const TArray<int16>& Corners)
{
	constexpr int32 N = FSimCity2000City::MapSize;
	const FIntPoint None(INDEX_NONE, INDEX_NONE);
	if (City.Tiles.Num() != N * N || Corners.Num() != (N + 1) * (N + 1)) return None;
	// Preserve authored theaters. Reloading always starts from the original city file.
	for (const auto& Tile : City.Tiles) if (Tile.Building == BuildingId) return None;

	// Multi-source distance field makes road proximity cheap for every candidate lot.
	TArray<int32> RoadDistance, Queue;
	RoadDistance.Init(N * 2, N * N);
	for (int32 I = 0; I < City.Tiles.Num(); ++I)
	{
		const uint8 Id = City.Tiles[I].Building;
		if (Id == 0x1d || Id == 0x1e || (Id >= 0x23 && Id <= 0x2b) || Id == 0x43 || Id == 0x44)
		{
			RoadDistance[I] = 0;
			Queue.Add(I);
		}
	}
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 I = Queue[Head];
		for (const FIntPoint D : {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)})
		{
			const int32 X = I % N + D.X, Y = I / N + D.Y;
			if (X < 0 || Y < 0 || X >= N || Y >= N) continue;
			const int32 J = Y * N + X;
			if (RoadDistance[J] > RoadDistance[I] + 1)
			{
				RoadDistance[J] = RoadDistance[I] + 1;
				Queue.Add(J);
			}
		}
	}

	FIntPoint Best = None;
	int64 BestScore = MAX_int64;
	for (int32 Y = 1; Y < N - Span; ++Y)
	for (int32 X = 1; X < N - Span; ++X)
	{
		bool bClear = true;
		int32 Trees = 0;
		int32 Parks = 0, Neighbors = 0;
		// Prefer clearance around the lot, but existing adjoining parcels are allowed in
		// dense cities. The complete footprint still has to be dry, unoccupied and level.
		for (int32 Dy = -1; Dy <= Span && bClear; ++Dy)
		for (int32 Dx = -1; Dx <= Span && bClear; ++Dx)
		{
			const auto& T = City.Tiles[(Y + Dy) * N + X + Dx];
			if (Dx < 0 || Dy < 0 || Dx >= Span || Dy >= Span)
			{
				Neighbors += (T.bWater || T.Building >= 0x70) ? 1 : 0;
				continue;
			}
			if (T.bWater || T.Terrain > 0x0c) { bClear = false; break; }
			const bool bTree = T.Building >= 6 && T.Building <= 12;
			const bool bSmallPark = T.Building == 13;
			if ((T.Building != 0 && !bTree && !bSmallPark) || (T.Zone & 15) > 6 || T.Text != 0)
			{
				bClear = false; break;
			}
			Trees += bTree ? 1 : 0;
			Parks += bSmallPark ? 1 : 0;
		}
		if (!bClear) continue;
		int16 Ground = 0; int32 Grading = 0;
		if (!FindFoundation(City, Corners, FIntPoint(X, Y), Ground, Grading)) continue;
		// Level natural lots always win. Dense cities can use a lightly graded landscaped lot.
		const int64 Score = int64(Grading) * 10000000 + Parks * 1000000 + Trees * 100000 + Neighbors * 1000 + RoadDistance[(Y + 1) * N + X + 1] * 100 +
			FMath::Abs(X + 1 - N / 2) + FMath::Abs(Y + 1 - N / 2);
		if (Score < BestScore) { Best = FIntPoint(X, Y); BestScore = Score; }
	}
	return Best;
}

void Stamp(FSimCity2000City& City, FIntPoint Origin, TArray<int16>* Corners)
{
	constexpr int32 N = FSimCity2000City::MapSize;
	if (City.Tiles.Num() != N * N || Origin.X < 0 || Origin.Y < 0 || Origin.X + Span > N || Origin.Y + Span > N) return;
	int16 Ground = int16((City.Tiles[Origin.Y * N + Origin.X].Altitude + 1) * 32);
	int32 Grading = 0;
	if (Corners)
	{
		if (Corners->Num() != (N + 1) * (N + 1) || Origin.X < 1 || Origin.Y < 1 ||
			Origin.X + Span >= N || Origin.Y + Span >= N || !FindFoundation(City, *Corners, Origin, Ground, Grading)) return;
		for (int32 Y = Origin.Y; Y <= Origin.Y + Span; ++Y)
		for (int32 X = Origin.X; X <= Origin.X + Span; ++X) (*Corners)[Y * (N + 1) + X] = Ground;
	}
	for (int32 Y = 0; Y < Span; ++Y)
	for (int32 X = 0; X < Span; ++X)
	{
		auto& Tile = City.Tiles[(Origin.Y + Y) * N + Origin.X + X];
		Tile.Building = BuildingId;
		Tile.Altitude = Tile.SecondaryAltitude = uint8(Ground / 32 - 1);
		Tile.RawAltitude = (Tile.RawAltitude & 0x8000) | Tile.Altitude | (Tile.Altitude << 5);
		Tile.Terrain = Tile.Slope = 0;
		// Same XZON corner convention as the original 3x3 commercial CO182 building.
		Tile.Zone = 5 | (X == 0 && Y == 0 ? 0x80 : X == Span - 1 && Y == 0 ? 0x40 :
			X == Span - 1 && Y == Span - 1 ? 0x20 : X == 0 && Y == Span - 1 ? 0x10 : 0);
		Tile.BitFlags = 0;
	}
}
}
