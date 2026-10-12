#include "SimCopterHangarInterior.h"
#include "SimCopterHangarGeometry.h"
#include "City/SimCopterHangar.h"

namespace
{
using SimCopterHangarGeometry::FProp;
const FLinearColor Iron(0.032f,0.038f,0.042f);
const FLinearColor Beam(0.14f,0.16f,0.17f);
const FLinearColor Timber(0.12f,0.078f,0.043f);
const FLinearColor Plaster(0.43f,0.40f,0.33f);
const FLinearColor Yellow(0.53f,0.34f,0.055f);

// Small block lettering is real geometry, and remains legible on the column from either
// outside approach. It needs no new font asset or cooked texture.
const TCHAR* Glyph(TCHAR C)
{
	switch (C)
	{
	case 'A': return TEXT("01110100011000111111100011000110001");
	case 'C': return TEXT("01111100001000010000100001000001111");
	case 'D': return TEXT("11110100011000110001100011000111110");
	case 'E': return TEXT("11111100001000011110100001000011111");
	case 'H': return TEXT("10001100011000111111100011000110001");
	case 'I': return TEXT("11111001000010000100001000010011111");
	case 'M': return TEXT("10001110111010110101100011000110001");
	case 'N': return TEXT("10001110011010110011100011000110001");
	case 'O': return TEXT("01110100011000110001100011000101110");
	case 'P': return TEXT("11110100011000111110100001000010000");
	case 'R': return TEXT("11110100011000111110101001001010001");
	case 'S': return TEXT("01111100001000001110000010000111110");
	case 'T': return TEXT("11111001000010000100001000010000100");
	case 'U': return TEXT("10001100011000110001100011000101110");
	case 'W': return TEXT("10001100011000110101101011101110001");
	case 'Z': return TEXT("11111000010001000100010001000011111");
	case 'B': return TEXT("11110100011000111110100011000111110");
	default: return TEXT("00000000000000000000000000000000000");
	}
}
void Lettering(FProp& Mesh,FVector Center,const FString& Text,float Pixel)
{
	const float Left=(Text.Len()*6-1)*Pixel*.5f;
	for (int32 Ch=0; Ch<Text.Len(); ++Ch)
		for (int32 Row=0; Row<7; ++Row)
			for (int32 Col=0; Col<5; ++Col)
				if (Glyph(Text[Ch])[Row*5+Col]=='1')
					Mesh.Box(Center+FVector(0,Left-(Ch*6+Col)*Pixel,(3-Row)*Pixel),FVector(.12f,Pixel*.92f,Pixel*.92f),FLinearColor(.008f,.008f,.006f));
}
void Sign(FProp& Mesh,FVector Center,bool bHearing)
{
	Mesh.Box(Center,FVector(1.1f,39,25),Iron);
	Mesh.Box(Center+FVector(.65,0,0),FVector(.2f,37.5f,23.5f),Yellow);
	Lettering(Mesh,Center+FVector(.85,0,7.5),TEXT("CAUTION"),.79f);
	if (bHearing)
	{
		Lettering(Mesh,Center+FVector(.85,0,0),TEXT("EAR PROTECTION"),.39f);
		Lettering(Mesh,Center+FVector(.85,0,-4.5),TEXT("MUST BE WORN"),.39f);
		Lettering(Mesh,Center+FVector(.85,0,-9),TEXT("IN THIS AREA"),.39f);
	}
	else
	{
		Lettering(Mesh,Center+FVector(.85,0,-1),TEXT("NOISE HAZARD"),.43f);
		Lettering(Mesh,Center+FVector(.85,0,-7),TEXT("AREA"),.5f);
	}
}
}

void SimCopterHangarInterior::BuildStructure(UProceduralMeshComponent& Shell,UProceduralMeshComponent& Glazing,
	UMaterialInterface* Material,UMaterialInterface* GlassMaterial,float TileSizeCm)
{
	using namespace SimCopterHangarPlacement;
	const float W=WidthTiles*200, D=DepthTiles*200, E=EavesHeightTiles*400, A=ApexHeightTiles*400;
	const float Door=DoorWidthTiles*200, DoorTop=DoorHeightTiles*400;
	FProp Floor,Walls,Roof,Steel,Details,Signs,Glass;
	// Quiet worn concrete replaces the airport's repeating cobbled/slab atlas.
	for (int32 X=0; X<19; ++X) for (int32 Y=0; Y<19; ++Y)
	{
		const float Tone=.095f+((X*17+Y*31)%11)*.0014f;
		Floor.Quad({-D+X*40,-W+Y*40,0},{-D+(X+1)*40,-W+Y*40,0},
			{-D+(X+1)*40,-W+(Y+1)*40,0},{-D+X*40,-W+(Y+1)*40,0},FLinearColor(Tone,Tone*.98f,Tone*.92f));
	}
	// Back office wall, gable and the full-height aircraft entrance.
	Walls.Box({-D+2,0,E*.5},{4,W*2,E},Plaster);
	Walls.Quad({-D,-W,E},{-D,W,E},{-D,0,A},{-D,0,A},Timber);
	for (float S : {-1.0f,1.0f}) Walls.Box({D,S*(W+Door)*.5,E*.5},{4,W-Door,E},Timber);
	Walls.Box({D,0,(DoorTop+E)*.5},{4,Door*2,E-DoorTop},Iron);
	Walls.Quad({D,W,E},{D,-W,E},{D,0,A},{D,0,A},Timber);
	// Two tall gridded window bands down both sides, as in DHANGAR. Glazing lives on a
	// separate non-shadowing mesh, allowing actual sun through the window/roof grids.
	for (float S : {-1.0f,1.0f})
	{
		Walls.Box({0,S*W,21},{D*2,4,42},Timber);
		Walls.Box({0,S*W,224},{D*2,7,35},Iron);
		Walls.Box({0,S*W,E-8},{D*2,6,16},Timber);
		for (int32 Bay=0; Bay<8; ++Bay)
		{
			const float X=-D+Bay*95;
			Steel.Box({X,S*(W-2),E*.5},{5,8,E},Beam);
			for (const FVector2D Band : {FVector2D(43,205),FVector2D(243,E-17)})
			{
				const FLinearColor Pane=S>0 ? FLinearColor(.58f,.53f,.39f) : FLinearColor(.16f,.22f,.35f);
				Glass.Quad({X+3,S*W,Band.X},{X+92,S*W,Band.X},{X+92,S*W,Band.Y},{X+3,S*W,Band.Y},Pane);
				// Frames on both faces of the frosted glass remain visible from the apron too.
				for (float Face : {-1.0f,1.0f})
				{
					for (int32 Col=0; Col<=6; ++Col)
						Steel.Box({X+Col*15.83,S*(W+Face*3),(Band.X+Band.Y)*.5},{1.4f,3,Band.Y-Band.X},Timber);
					for (float Z=Band.X; Z<=Band.Y; Z+=18)
						Steel.Box({X+47.5,S*(W+Face*3),Z},{95,3,1.4f},Timber);
				}
			}
		}
	}
	// Planked roof leaves a broad continuous glazed strip along the ridge.
	const auto RoofZ=[&](float Y) { return A-(A-E)*FMath::Abs(Y)/W; };
	for (float S : {-1.0f,1.0f})
	{
		for (float Y=112; Y<W; Y+=8)
		{
			const float Outer=FMath::Min(Y+7.8f,W);
			const float Tone=.75f+FMath::Fmod(Y*17,9)*.045f;
			Roof.Quad({-D,S*Y,RoofZ(Y)},{D,S*Y,RoofZ(Y)},
				{D,S*Outer,RoofZ(Outer)},{-D,S*Outer,RoofZ(Outer)},Timber*Tone);
		}
		for (float X=-D; X<D; X+=95)
		{
			Glass.Quad({X,0,A},{X+95,0,A},{X+95,S*112,RoofZ(112)},{X,S*112,RoofZ(112)},FLinearColor(.22f,.32f,.48f));
			Steel.Bar({X,0,A-1},{X,S*W,E-1},2.2f,Beam);
			Steel.Bar({X,-W,E-8},{X,W,E-8},2,Beam);
			for (int32 I=0; I<4; ++I)
			{
				const float Y0=S*I*95.0f, Y1=S*(I+1)*95.0f;
				Steel.Bar({X,Y0,E-8},{X,Y1,RoofZ(Y1)-4},1.2f,Beam);
				Steel.Bar({X,Y1,E-8},{X,Y1,RoofZ(Y1)-4},1.2f,Beam);
			}
			Steel.Bar({X,0,A},{X+95,S*112,RoofZ(112)},1.1f,Beam);
		}
		for (float Y : {0.0f,56.0f,112.0f,245.0f})
			Steel.Bar({-D,S*Y,RoofZ(Y)-2},{D,S*Y,RoofZ(Y)-2},1.8f,Beam);
	}
	// Two rows of slender I-section pillars establish the long nave and side aisles.
	for (float X : {50.0f,-135.0f,-310.0f}) for (float Y : {-150.0f,150.0f})
	{
		Steel.Box({X,Y,E*.5},{6,2.4f,E},Beam);
		for (float Side : {-1.0f,1.0f}) Steel.Box({X+Side*3,Y,E*.5},{1.2f,11,E},Beam);
		Steel.Box({X,Y,2},{13,16,4},Iron);
		Steel.Box({X,Y,E-2},{13,16,4},Beam);
	}
	// Back-office windows, connecting conduits, gallery, cross braces and stair flight.
	for (float Z : {54.0f,181.0f}) for (float Y=-300; Y<=300; Y+=62)
	{
		Details.Box({-D+5,Y,Z},{1.2f,44,28},Iron);
		for (int32 C=0; C<4; ++C) Details.Box({-D+6,Y-22+C*14.6,Z},{1,1,28},Beam);
		Details.Box({-D+6,Y,Z},{1,44,1},Beam);
	}
	Details.Box({-D+12,0,130},{20,W*2-12,4},Plaster);
	for (float Y : {-345.0f,-190.0f,-35.0f,120.0f,275.0f})
		Details.Box({-D+6,Y,E*.5},{6,5,E},Plaster*1.16f);
	Details.Box({-D+8,0,143},{2,W*2-20,2},Yellow);
	Details.Bar({-D+14,-130,6},{-D+14,180,128},2,Plaster);
	Details.Bar({-D+14,180,6},{-D+14,-130,128},2,Plaster);
	for (float Offset : {0.0f,3.0f,6.0f})
	{
		Details.Bar({-D+12,-205+Offset,18},{-D+12,-205+Offset,223+Offset},.8f,Beam);
		Details.Bar({-D+12,-205+Offset,223+Offset},{-D+12,275,223+Offset},.8f,Beam);
	}
	for (int32 Step=0; Step<15; ++Step)
		Details.Box({-205-Step*10.0,-310,Step*8.5+3},{12,45,4},Iron);
	Details.Bar({-202,-285,35},{-352,-285,162},1.2f,Beam);
	Details.Bar({-202,-335,35},{-352,-335,162},1.2f,Beam);
	// Pendant shades and cables are at the same repeated structural bays as the illustration.
	for (float X : {220.0f,25.0f,-170.0f,-320.0f}) for (float Y : {-125.0f,125.0f})
	{
		Details.Bar({X,Y,RoofZ(Y)-8},{X,Y,E-24},.7f,Iron);
		Details.Cylinder({X,Y,E-29},FVector::UpVector,8,5,Iron);
		Details.Cylinder({X,Y,E-33},FVector::UpVector,5.5f,1,FLinearColor(.58f,.53f,.38f));
	}
	for (const FVector Barrel : {FVector(-230,210,0),FVector(-280,-230,0),FVector(-250,-270,0)})
	{
		Details.Cylinder(Barrel+FVector(0,0,14),FVector::UpVector,9,28,FLinearColor(.16f,.17f,.16f));
		for (float Z : {2.0f,14.0f,26.0f}) Details.Cylinder(Barrel+FVector(0,0,Z),FVector::UpVector,9.5f,1,Iron);
	}
	for (int32 I=0; I<2; ++I)
	{
		FProp Panel;
		Sign(Panel,FVector::ZeroVector,I==0);
		Panel.Place(FVector(54,-150,170-I*22),0,.8f);
		Signs.Append(Panel);
	}
	for (int32 Coil=0; Coil<5; ++Coil)
		for (int32 Segment=0; Segment<20; ++Segment)
		{
			const float T0=Segment*2*PI/20, T1=(Segment+1)*2*PI/20;
			Details.Bar(FVector(235+Coil*3,345,2)+FVector(FMath::Cos(T0)*17,FMath::Sin(T0)*10,0),
				FVector(235+Coil*3,345,2)+FVector(FMath::Cos(T1)*17,FMath::Sin(T1)*10,0),.9f,Iron);
		}
	// Yellow/black floor protection beside the side wall, matching the photograph's car corner.
	for (int32 Stripe=0; Stripe<24; ++Stripe)
		Details.Box({D-Stripe*20.0,W-12,4},{20,12,7},Stripe%2 ? Iron : Yellow);
	Shell.ClearAllMeshSections(); Glazing.ClearAllMeshSections();
	FProp* Parts[]={&Walls,&Roof,&Floor,&Steel,&Details,&Signs};
	for (int32 I=0; I<UE_ARRAY_COUNT(Parts); ++I)
	{
		Parts[I]->Place(FVector(0,0,FloorLiftCm),0,TileSizeCm/400);
		Parts[I]->Apply(Shell,I,Material,I<4);
	}
	Glass.Place(FVector(0,0,FloorLiftCm),0,TileSizeCm/400);
	Glass.Apply(Glazing,0,GlassMaterial,true);
}
