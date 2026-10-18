#include "City/SimCopterMarina.h"
#include "City/SimCopterCityGeometryRules.h"
#include "City/SimCopterCivicLandmarks.h"
#include "City/SimCopterLandmarkGeometry.h"
#include "City/SimCopterTunnel.h"
#include "Algo/Reverse.h"

namespace SimCopterMarina
{
TMap<int32, FLayout> PrepareTerrain(const FSimCity2000City& City, TArray<uint8>& TerrainTypes)
{
	TMap<int32, FLayout> Layouts;
	constexpr int32 N = FSimCity2000City::MapSize;
	if (City.Tiles.Num() != N*N || TerrainTypes.Num() != N*N) return Layouts;
	// Read an immutable snapshot: adjacent marinas must not depend on traversal order.
	const TArray<uint8> OriginalTypes = TerrainTypes;
	TArray<uint8> Claims; Claims.Init(0, N*N);
	auto Neighbor = [&](int32 X, int32 Y, FIntPoint Direction, bool& bRoad) -> uint8
	{
		// Look through adjoining marina sites to the actual shore, rather than treating
		// their legacy building-fill terrain as land in the middle of a harbor.
		while (X >= 0 && X < N && Y >= 0 && Y < N)
		{
			const int32 I = Y*N+X;
			if (City.Tiles[I].Building != BuildingId)
			{
				bRoad = SimCopterTunnel::IsRoad(City.Tiles[I].Building) && !SimCopterTunnel::IsPortal(City.Tiles[I].Building);
				return bRoad && OriginalTypes[I] < 10 ? uint8(0x30) : OriginalTypes[I];
			}
			X += Direction.X; Y += Direction.Y;
		}
		return 0; // Open map boundary.
	};
	for (int32 Y=0; Y<N; ++Y) for (int32 X=0; X<N; ++X)
	{
		if (City.Tiles[Y*N+X].Building != BuildingId) continue;
		if (FSimCopterCityGeometryRules::ClaimOriginalBuildingFootprint(City, X, Y, Claims) != FIntPoint(3,3)) continue;
		FLayout Layout;
		for (int32 V=0; V<3; ++V) for (int32 U=0; U<3; ++U)
		{
			const FIntPoint Points[] = {{X-1,Y+V},{X+3,Y+V},{X+U,Y-1},{X+U,Y+3}};
			const FIntPoint Directions[] = {{-1,0},{1,0},{0,-1},{0,1}};
			const int32 Distances[] = {U+1,3-U,V+1,3-V};
			const int32 Nearest = FMath::Min(FMath::Min(U+1,3-U), FMath::Min(V+1,3-V));
			bool bLand=false, bWater=false, bRoad=false;
			uint8 LandType=0x30;
			for (int32 Side=0; Side<4; ++Side)
			{
				if (Distances[Side] != Nearest) continue;
				bool bNeighborRoad=false;
				const uint8 Type = Neighbor(Points[Side].X, Points[Side].Y, Directions[Side], bNeighborRoad);
				bWater |= Type < 10;
				if (Type >= 10) { bLand=true; LandType=Type; }
				bRoad |= bNeighborRoad && Nearest == 1;
			}
			// A shore-connected corner stays dry; inland water wins ties only under the
			// central platform. A marina surrounded by land remains on land.
			bLand &= Nearest == 1 || !bWater;
			const uint16 Bit = uint16(1 << (V*3+U));
			if (bLand) Layout.LandMask |= Bit;
			if (bRoad) Layout.RoadMask |= Bit;
			TerrainTypes[(Y+V)*N+X+U] = bLand ? LandType : 0;
		}
		Layouts.Add(Y*N+X, Layout);
	}
	return Layouts;
}

FMaxisMeshObject MakeModel(const FMaxisMeshObject& Original, const FLayout& Layout)
{
	if (Original.Header.Id != ObjectId) return Original;
	FMaxisMeshObject Result = Original;
	Result.Vertices.Reset(); Result.Faces.Reset(); Result.Header.bConsistentWinding = true;
	constexpr double DeckTop=6.4, Thickness=1.3;
	// User-requested presentation departure: MA248's flat red apron/Ts become timber.
	// Keep every original building wall, roof and atlas UV; only translate the house
	// from its original 2/3-unit base onto the 40 cm deck above the water crest.
	for (const FMaxisMeshFace& Source : Original.Faces)
	{
		if (Source.FaceType == 15 && Source.MaterialIndex == 16) continue;
		FMaxisMeshFace Face = Source;
		Face.VertexIndices.Reset();
		FVector Center=FVector::ZeroVector, Normal=FVector::ZeroVector;
		for (int32 I=0; I<Source.VertexIndices.Num(); ++I)
		{
			const auto& A=Original.Vertices[Source.VertexIndices[I]];
			const auto& B=Original.Vertices[Source.VertexIndices[(I+1)%Source.VertexIndices.Num()]];
			const FVector P=FVector(A.X,A.Y,A.Z)/65536., Q=FVector(B.X,B.Y,B.Z)/65536.;
			Center += P;
			Normal += FVector((P.Y-Q.Y)*(P.Z+Q.Z),(P.Z-Q.Z)*(P.X+Q.X),(P.X-Q.X)*(P.Y+Q.Y));
			Face.VertexIndices.Add(Result.Vertices.Num());
			Result.Vertices.Add({A.X, A.Y+int32(FMath::RoundToInt((DeckTop-2./3)*65536)), A.Z});
		}
		// Match the authored boxes' inward raw winding. The shared mesh builder then
		// resolves one global outward normal sign, including the original roof/walls.
		Center /= FMath::Max(1, Source.VertexIndices.Num());
		if (FVector::DotProduct(Normal, Center-FVector(2,15,8)) > 0)
		{
			Algo::Reverse(Face.VertexIndices); Algo::Reverse(Face.RawUVs);
		}
		Result.Faces.Add(MoveTemp(Face));
	}
	using namespace SimCopterCivicLandmarks;
	SimCopterLandmarkGeometry::FBuilder B{Result};
	auto Deck = [&](double X0, double X1, double Z0, double Z1, bool AlongX)
	{
		const double Across0=AlongX?Z0:X0, Across1=AlongX?Z1:X1;
		const double Along0=AlongX?X0:Z0, Along1=AlongX?X1:Z1;
		const int32 Rows=FMath::Max(1,FMath::RoundToInt((Across1-Across0)/3.2));
		const double Pitch=(Across1-Across0)/Rows;
		for (int32 Row=0; Row<Rows; ++Row)
		{
			// Stagger board ends, with narrow real seams rather than a repeating red plane.
			for (double Start=Along0; Start<Along1-.01; )
			{
				const double End=FMath::Min(Along1, Start+(Start==Along0 && Row%2 ? 12. : 24.));
				const double A=(Start+End)*.5, C=Across0+(Row+.5)*Pitch;
				B.Box(AlongX?FVector(A,DeckTop-Thickness*.5,C):FVector(C,DeckTop-Thickness*.5,A),
					AlongX?FVector(End-Start-.12,Thickness,Pitch-.16):FVector(Pitch-.16,Thickness,End-Start-.12),
					(Row+FMath::RoundToInt(Start/12))%3==0 ? PierPlankDark : PierPlankLight);
				Start=End;
			}
		}
	};
	Deck(-48,48,-44,44,true);
	Deck(-8,8,-84,-44,true); Deck(-8,8,44,84,true);
	Deck(-84,-48,-8,8,false); Deck(48,84,-8,8,false);
	Deck(-52,52,-96,-84,false); Deck(-52,52,84,96,false);
	Deck(-96,-84,-52,52,true); Deck(84,96,-52,52,true);
	// Rim beams sit below the boards, without closing off the surrounding water.
	for (double X:{-47.,47.}) B.Box({X,3.9,0},{2,2.2,88},PierPiling);
	for (double Z:{-43.,43.}) B.Box({0,3.9,Z},{92,2.2,2},PierPiling);
	for (double S:{-1.,1.})
	{
		B.Box({0,3.9,S*90},{104,2.2,3},PierPiling);
		B.Box({S*90,3.9,0},{3,2.2,104},PierPiling);
		for (double Q:{-5.,5.})
		{
			B.Box({Q,3.9,S*64},{2,2.2,40},PierPiling);
			B.Box({S*64,3.9,Q},{40,2.2,2},PierPiling);
		}
		for (double Q:{-46.,-24.,24.,46.})
		{
			B.Box({Q,-5,S*92},{2.5,26,2.5},PierPiling);
			B.Box({S*92,-5,Q},{2.5,26,2.5},PierPiling);
		}
		for (double Q:{-38.,0.,38.})
		{
			B.Box({Q,-6,S*41},{3,24,3},PierPiling);
			B.Box({S*45,-6,Q},{3,24,3},PierPiling);
		}
	}
	// Keep road approaches paved, with no blanket slab over the water-facing cells.
	// Model->city conversion is (world X,world Y)=(-Maxis Z,-Maxis X).
	for (int32 V=0; V<3; ++V) for (int32 U=0; U<3; ++U)
	{
		if (Layout.RoadMask & (1 << (V*3+U)))
			B.Box({(V-1)*64.,.12,(1-U)*64.},{64,.24,64},RoadApronFinish);
	}
	B.Finish();
	return Result;
}
}
