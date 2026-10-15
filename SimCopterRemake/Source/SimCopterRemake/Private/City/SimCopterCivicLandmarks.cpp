#include "City/SimCopterCivicLandmarks.h"
#include "City/SimCopterLandmarkGeometry.h"

namespace SimCopterCivicLandmarks
{
bool IsPier(int32 ObjectId)
{
	// FUN_0047c0c0 dispatches XBLD 0xdf to AP223/AP223F according to saved rotation/XBIT.
	return ObjectId == 0x08c || ObjectId == 0x10c;
}

float PierOriginZ(float WaterZ, float UnitsPerCentimeter, float MeshScale, float TileSize, float WaveAmplitude)
{
	// The authored deck is 33 original units above its origin. Anchor that surface to
	// the rendered water, keeping enough freeboard for its maximum wave displacement.
	return WaterZ + FMath::Max(TileSize*.08f, WaveAmplitude+12.f) - 33.f*65536.f/UnitsPerCentimeter*MeshScale;
}

FMaxisMeshObject MakeCityHall(const FMaxisMeshObject& Original)
{
	FMaxisMeshObject Result=Original;
	if (Original.Header.Id != CityHallObjectId) return Result;
	Result.Vertices.Reset(); Result.Faces.Reset();
	SimCopterLandmarkGeometry::FBuilder B{Result};
	// Reference: the supplied SC2000 City Hall image. Preserve CH208's three-tile site,
	// symmetric wings and central dome; author the missing colonnades and stone detailing.
	// Finish tags: limestone, carved trim, metal roof, architectural facade, planting,
	// paving, door, rooftop equipment. Repeated columns/windows are material relief.
	auto QuadUV=[&](const TArray<FVector>& Points, uint8 Finish, double U0, double U1, double V0, double V1)
	{
		B.Face(Points,Finish);
		auto& UVs=Result.Faces.Last().RawUVs; UVs.Reset();
		for (FVector2D UV : {FVector2D(U0,V0),FVector2D(U1,V0),FVector2D(U1,V1),FVector2D(U0,V1)})
			UVs.Add(FIntPoint(FMath::RoundToInt(UV.X*65536),FMath::RoundToInt((1-UV.Y)*65536)));
	};
	auto FacadeBox=[&](FVector C,FVector S,double LongBays,double EndBays,double Floors)
	{
		const FVector A=C-S*.5,D=C+S*.5;
		QuadUV({{A.X,A.Y,A.Z},{D.X,A.Y,A.Z},{D.X,D.Y,A.Z},{A.X,D.Y,A.Z}},3,0,EndBays,0,Floors);
		QuadUV({{D.X,A.Y,D.Z},{A.X,A.Y,D.Z},{A.X,D.Y,D.Z},{D.X,D.Y,D.Z}},3,0,EndBays,0,Floors);
		QuadUV({{A.X,A.Y,D.Z},{A.X,A.Y,A.Z},{A.X,D.Y,A.Z},{A.X,D.Y,D.Z}},3,0,LongBays,0,Floors);
		QuadUV({{D.X,A.Y,A.Z},{D.X,A.Y,D.Z},{D.X,D.Y,D.Z},{D.X,D.Y,A.Z}},3,0,LongBays,0,Floors);
		B.Face({{A.X,D.Y,A.Z},{D.X,D.Y,A.Z},{D.X,D.Y,D.Z},{A.X,D.Y,D.Z}},0);
	};
	auto FacadeDrum=[&](double Y,double R,double Height,int32 Sides,int32 Bays)
	{
		for (int32 I=0;I<Sides;++I)
		{
			const double A=2*PI*I/Sides,D=2*PI*(I+1)/Sides;
			QuadUV({{R*cos(A),Y,12+R*sin(A)},{R*cos(D),Y,12+R*sin(D)},
				{R*cos(D),Y+Height,12+R*sin(D)},{R*cos(A),Y+Height,12+R*sin(A)}},
				3,double(I)*Bays/Sides,double(I+1)*Bays/Sides,0,1);
		}
	};
	auto Hedge=[&](FVector C,FVector S)
	{
		// One chamfered block per clipped hedge; leaf clusters live in the material.
		const int32 First=Result.Faces.Num(); const FVector A=C-S*.5,D=C+S*.5;
		const double E=1.2,Shoulder=D.Y-1.5;
		B.Box({C.X,(A.Y+Shoulder)*.5,C.Z},{S.X,Shoulder-A.Y,S.Z},4);
		B.Face({{A.X,Shoulder,A.Z},{D.X,Shoulder,A.Z},{D.X-E,D.Y,A.Z+E},{A.X+E,D.Y,A.Z+E}},4);
		B.Face({{D.X,Shoulder,D.Z},{A.X,Shoulder,D.Z},{A.X+E,D.Y,D.Z-E},{D.X-E,D.Y,D.Z-E}},4);
		B.Face({{A.X,Shoulder,D.Z},{A.X,Shoulder,A.Z},{A.X+E,D.Y,A.Z+E},{A.X+E,D.Y,D.Z-E}},4);
		B.Face({{D.X,Shoulder,A.Z},{D.X,Shoulder,D.Z},{D.X-E,D.Y,D.Z-E},{D.X-E,D.Y,A.Z+E}},4);
		B.Face({{A.X+E,D.Y,A.Z+E},{D.X-E,D.Y,A.Z+E},{D.X-E,D.Y,D.Z-E},{A.X+E,D.Y,D.Z-E}},4);
		// UV region +128 distinguishes foliage from the lawn without another material slot.
		for(int32 I=First;I<Result.Faces.Num();++I) for(auto& UV:Result.Faces[I].RawUVs) UV.X+=128*65536;
	};
	B.Box({0,.4,0},{178,.8,178},5);
	for (double X : {-57.,57.})
	{
		B.Box({X,1,-65},{47,1,36},4);
		Hedge({FMath::Sign(X)*36,5,-65},{5,7,29});
		Hedge({X,5,-49.5},{47,7,5});
	}
	B.Box({0,2,8},{151,4,119},1);
	B.Box({0,18,16},{62,32,89},0);
	for (double X : {-55.,55.})
	{
		FacadeBox({X,18,5},{44,32,112},9,4,2);
		for (double Y : {5.,17.,32.,35.}) B.Box({X,Y,5},{46,1.6,114},1,Y==35.?2:INDEX_NONE);
		// Low rooftop housings echo the reference's scattered service structures.
		for (double Z : {-32.,-9.,15.})
		{
			const int32 First=Result.Faces.Num();
			B.Box({X,38.1,Z},{9,4.5,11},7);
			for(int32 I=First;I<Result.Faces.Num();++I)
			{
				auto& UVs=Result.Faces[I].RawUVs;
				UVs={FIntPoint(0,65536),FIntPoint(65536,65536),FIntPoint(65536,0),FIntPoint(0,0)};
			}
		}
		FacadeBox({X,40,44},{18,8,17},2,2,1);
		B.Box({X,44.5,44},{20,1,19},1,2);
	}
	// Broad steps and a six-column pedimented entrance.
	for (int32 I=0; I<5; ++I) B.Box({0,.65+I*.75,-64+I*2.4},{54-I*1.4,1.3+I*1.5,30-I*4.8},1);
	B.Box({0,14,-31},{48,24,12},0);
	B.Box({0,12,-37.15},{14,17,.3},6);
	for (int32 I=0; I<6; ++I)
	{
		const double X=-22+I*8.8;
		B.Ring({X,4.5,-56},2.2,2.2,1.8,1,8,true);
		B.Ring({X,6.3,-56},1.5,1.25,21,1,8);
		B.Ring({X,27.3,-56},2.1,2.1,1.8,1,8,true);
	}
	B.Box({0,30,-48},{52,3,23},1);
	B.Face({{-27,31.5,-60},{27,31.5,-60},{0,46,-60}},1);
	B.Face({{-27,31.5,-36},{0,46,-36},{27,31.5,-36}},0);
	B.Face({{-27,31.5,-60},{0,46,-60},{0,46,-36},{-27,31.5,-36}},2);
	B.Face({{0,46,-60},{27,31.5,-60},{27,31.5,-36},{0,46,-36}},2);
	B.Beam({-27,32,-60.5},{0,46.5,-60.5},.8,1); B.Beam({0,46.5,-60.5},{27,32,-60.5},.8,1);
	// SC2000 reference: stepped colonnaded drums, a shallow flared crown, then a
	// narrow lantern. A hemisphere obscures this silhouette. Ribs/columns are UV relief.
	B.Ring({0,31,12},32,32,3,1,32,true);
	FacadeDrum(34,29,20,32,16);
	B.Ring({0,54,12},31,31,3,1,32,true);
	FacadeDrum(57,25.5,17,32,16);
	B.Ring({0,74,12},28,28,3,1,32,true);
	const double CrownY[]={77,80,86,91},CrownR[]={28,25,18,8.5};
	for (int32 Ring=0; Ring<3; ++Ring)
	{
		for (int32 I=0; I<32; ++I)
		{
			const double A=2*PI*I/32,D=2*PI*(I+1)/32,R0=CrownR[Ring],R1=CrownR[Ring+1];
			// UV region +32 selects crown ribs; sixteen ribs wrap the full circumference.
			QuadUV({{R0*cos(A),CrownY[Ring],12+R0*sin(A)},{R0*cos(D),CrownY[Ring],12+R0*sin(D)},
				{R1*cos(D),CrownY[Ring+1],12+R1*sin(D)},{R1*cos(A),CrownY[Ring+1],12+R1*sin(A)}},
				2,I*.5,(I+1)*.5,32+(CrownY[Ring]-77)/14.,32+(CrownY[Ring+1]-77)/14.);
		}
	}
	B.Ring({0,91,12},9.5,9.5,1.5,1,16,true);
	FacadeDrum(92.5,6,13,16,8);
	B.Ring({0,105.5,12},7.5,7.5,1.5,1,16,true);
	B.Ring({0,107,12},7.5,4,1.5,2,16,true);
	B.Ring({0,108.5,12},4,0,2,2,16);
	B.Finish();
	return Result;
}
}
