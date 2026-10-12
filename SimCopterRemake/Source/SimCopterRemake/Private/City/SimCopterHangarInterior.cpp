#include "SimCopterHangarInterior.h"
#include "SimCopterHangarGeometry.h"

#include "City/SimCopterHangar.h"
#include "Flight/SimCopterHelicopterRegistry.h"
#include "Flight/SimCopterHelicopterPresentation.h"
#include "Formats/MaxisMeshLibrary.h"
#include "Formats/MaxisProceduralMeshBuilder.h"

namespace
{
const FLinearColor Steel(0.055f, 0.070f, 0.080f);
const FLinearColor Rubber(0.018f, 0.022f, 0.027f);
const FLinearColor Chrome(0.52f, 0.57f, 0.61f);
const FLinearColor Glass(0.018f, 0.026f, 0.043f);
const FLinearColor Brown(0.18f, 0.105f, 0.044f);
const FLinearColor Red(0.28f, 0.035f, 0.025f);

using SimCopterHangarGeometry::FProp;

FProp Car()
{
	FProp P;
	// Tapered body and rounded bonnet shoulders, following the illustration's classic sedan.
	const FVector Rows[]={FVector(-112,39,46),FVector(-88,44,51),FVector(-60,45,54),
		FVector(32,45,56),FVector(80,44,54),FVector(111,39,47)};
	for (int32 Row=0; Row<UE_ARRAY_COUNT(Rows)-1; ++Row)
	{
		const FVector A=Rows[Row],B=Rows[Row+1];
		for (float Side : {-1.0f,1.0f})
		{
			P.Quad({A.X,Side*A.Y,20},{B.X,Side*B.Y,20},{B.X,Side*B.Y,B.Z-9},{A.X,Side*A.Y,A.Z-9},Brown*.7f);
			P.Quad({A.X,Side*A.Y,A.Z-9},{B.X,Side*B.Y,B.Z-9},{B.X,Side*(B.Y-6),B.Z},{A.X,Side*(A.Y-6),A.Z},Brown);
			P.Quad({A.X,Side*(A.Y-6),A.Z},{B.X,Side*(B.Y-6),B.Z},{B.X,0,B.Z+2},{A.X,0,A.Z+2},Brown*1.1f);
		}
	}
	P.Box({110,0,35},{3,79,25},Brown);
	P.Box({-112,0,33},{3,78,26},Brown);
	// Sloping windscreen and rear glass distinguish the brown sedan from a box.
	P.Quad({34,-42,56},{34,42,56},{10,35,86},{10,-35,86},Glass);
	P.Quad({-75,42,56},{-75,-42,56},{-54,-35,86},{-54,35,86},Glass);
	P.Box({-22,0,87},{67,74,5},Brown);
	for (float Side : {-1.0f,1.0f})
	{
		P.Quad({-73,Side*43,56},{32,Side*43,56},{9,Side*36,84},{-53,Side*36,84},Glass);
		P.Box({-20,Side*40,71},{4,4,28},Brown);
		P.Box({-29,Side*46,51},{13,2,3},Chrome);
		P.Bar({32,Side*42,57},{9,Side*36,85},1.3f,Brown);
		P.Bar({-73,Side*42,57},{-53,Side*36,85},1.3f,Brown);
		P.Bar({32,Side*42,57},{-73,Side*42,57},.6f,Chrome*.7f);
		P.Box({19,Side*48,61},{9,7,4},Chrome*.6f);
		for (float X : {-70.0f,70.0f})
		{
			P.Cylinder({X,Side*44,19},FVector::YAxisVector,19,13,Rubber);
			P.Cylinder({X,Side*51,19},FVector::YAxisVector,16,2,Chrome*.75f);
			P.Cylinder({X,Side*52.2f,19},FVector::YAxisVector,13.5f,1,Rubber);
			P.Cylinder({X,Side*53,19},FVector::YAxisVector,10.5f,1,Chrome*.7f);
			P.Cylinder({X,Side*53.7f,19},FVector::YAxisVector,6,1,Steel);
			for (int32 Spoke=0; Spoke<8; ++Spoke)
			{
				const float Angle=Spoke*PI*.25f;
				P.Bar({X,Side*54,19},{X+FMath::Cos(Angle)*10,Side*54,19+FMath::Sin(Angle)*10},.45f,Chrome);
			}
		}
		// The reference is a low brown four-headlight sedan, not a square-fronted van.
		for (float Y : {25.0f,39.0f})
		{
			P.Cylinder({114,Side*Y,46},FVector::XAxisVector,7.5f,3,Chrome);
			P.Cylinder({116,Side*Y,46},FVector::XAxisVector,6,1,FLinearColor(.30f,.32f,.33f));
		}
		P.Box({-113,Side*32,44},{3,20,9},Red);
	}
	P.Box({114,0,26},{7,96,7},Chrome);
	P.Box({-114,0,26},{7,96,7},Chrome);
	P.Box({114,0,44},{3,37,12},Rubber);
	for (int32 I=0; I<4; ++I) P.Box({116,0,39.0+I*3},{1,36,1},Chrome);
	return P;
}

FProp ToolCabinet()
{
	FProp P;
	P.Box({0,0,38},{38,48,66},Red);
	P.Box({0,0,72},{41,51,5},Rubber);
	P.Box({-3,0,89},{31,44,30},Red);
	for (int32 I=0; I<8; ++I)
	{
		const float Z=I<4 ? 17+I*13 : 79+(I-4)*6;
		const float X=I<4 ? 20 : 13.5f;
		P.Box({X,0,Z},{2,41,3},Rubber);
		P.Box({X+1,0,Z+2},{2,31,2},Chrome);
	}
	for (float X : {-13.0f,13.0f}) for (float Y : {-18.0f,18.0f})
		P.Cylinder({X,Y,4},FVector::YAxisVector,4,5,Rubber);
	return P;
}

FProp Office()
{
	FProp P;
	P.Box({0,0,59},{60,120,5},Steel);
	P.Box({0,-41,30},{52,33,54},Steel);
	for (float Y : {-53.0f,53.0f}) P.Box({-23,Y,29},{4,4,58},Chrome);
	for (int32 I=0; I<3; ++I)
	{
		P.Box({27,-41,15.0+I*16},{2,30,1},Rubber);
		P.Box({29,-41,23.0+I*16},{2,12,2},Chrome);
	}
	P.Box({-6,24,63},{28,26,2},FLinearColor(0.77f,0.71f,0.56f));
	P.Cylinder({-8,-28,66},FVector::UpVector,8,7,FLinearColor(0.50f,0.32f,0.08f));
	P.Cylinder({-8,-28,78},FVector::UpVector,2,21,Chrome);
	P.Cylinder({-8,-28,89},FVector::UpVector,9,3,FLinearColor(0.50f,0.32f,0.08f));
	for (float Y : {0.0f,16.0f})
	{
		P.Box({-16,Y,63},{10,10,3},Rubber);
		P.Cylinder({-16,Y,73},FVector::UpVector,2,19,Chrome);
		P.Cylinder({-16,Y,83},FVector::UpVector,5,5,FLinearColor(.37f,.25f,.09f));
	}
	P.Box({25,-173,30},{62,54,60},FLinearColor(.018f,.022f,.025f));
	P.Box({57,-173,30},{2,49,55},Steel);
	P.Cylinder({59,-166,36},FVector::XAxisVector,4,2,Chrome);
	P.Box({61,-181,28},{3,3,12},Chrome);
	return P;
}

FProp Chair()
{
	FProp P;
	const FLinearColor Fabric(0.06f,0.09f,0.19f);
	P.Box({0,0,33},{33,34,7},Fabric);
	P.Box({15,0,55},{6,34,37},Fabric);
	P.Cylinder({0,0,18},FVector::UpVector,3,28,Chrome);
	P.Box({0,0,6},{43,5,4},Rubber); P.Box({0,0,6},{5,43,4},Rubber);
	for (float Side : {-1.0f,1.0f})
	{
		P.Box({0,Side*21,3},{7,6,6},Rubber);
		P.Box({Side*21,0,3},{7,6,6},Rubber);
		P.Box({4,Side*19,44},{25,3,3},Steel);
		P.Box({13,Side*19,38},{3,3,12},Steel);
	}
	return P;
}

FProp Shelves()
{
	FProp P;
	P.Box({-13,0,69},{4,95,138},Steel);
	for (float Side : {-1.0f,1.0f}) P.Box({0,Side*48,69},{29,4,138},Steel);
	P.Box({0,0,20},{29,94,38},Steel);
	P.Box({15,0,21},{1,1,36},Rubber);
	for (float Y : {-6.0f,6.0f}) P.Box({16,Y,25},{2,2,7},Chrome);
	for (int32 Row=1; Row<5; ++Row)
	{
		P.Box({0,0,3.0+Row*33},{29,99,4},Steel);
		if (Row==4) continue;
		for (int32 Book=0; Book<11; ++Book)
		{
			const float Height=21+(Book*7+Row*3)%9;
			const FLinearColor Color=Book%3==0 ? FLinearColor(0.22f,0.07f,0.035f) : FLinearColor(0.42f,0.31f,0.14f);
			P.Box({1,-40.0+Book*8,6+Row*33+Height*0.5},{21,6,Height},Color);
		}
	}
	return P;
}

FProp Aircraft(const FMaxisMeshLibrary& Library, bool bHelicopter)
{
	FProp P;
	// Reuse the game's original airframe and stationary rotor geometry, without spawning a
	// flyable pawn, crew, audio, collision, or another entry in the player's saved fleet.
	const auto& Heli=SimCopterHelicopterRegistry::GetDefinitions()[0];
	auto AddObject = [&](const FString& Name, FVector Offset=FVector::ZeroVector)
	{
		const TArray<FColor>* Palette=nullptr;
		if (const auto* Object=Library.FindObjectByTableName(Name,&Palette))
		{
			TArray<FColor> Paint;
			if (Palette) SimCopterHelicopterPresentation::MakePaintPalette(*Palette,Paint,INDEX_NONE);
			if (Name==Heli.BodyObjectName && Paint.Num()>=177)
			{
				// The menu's parked Jet Ranger has a silver cabin and muted green trim.
				Paint[16]=FColor(100,128,126); Paint[160]=FColor(173,179,178); Paint[32]=FColor(87,108,105);
			}
			FMaxisMeshSection Solid,DiscardedBlur;
			FMaxisProceduralMeshBuilder::BuildPaletteColoredSections(*Object,Palette ? &Paint : nullptr,262144,1,true,
				Steel,Solid,&DiscardedBlur);
			P.Append(Solid,Offset);
		}
	};
	AddObject(bHelicopter ? Heli.BodyObjectName : TEXT("PLANE1"));
	if (bHelicopter && !P.IsEmpty())
	{
		AddObject(Heli.MainRotorObjectName);
		// GEO metres use 262144 units; the registry's offsets use four original units/metre.
		AddObject(TEXT("ROTORTL"),Heli.ToTailRotorOffsetCm(0.25f));
	}
	return P;
}
}

void SimCopterHangarInterior::Build(UProceduralMeshComponent& Mesh, UMaterialInterface* Material,
	const FString& OriginalRoot, float TileSizeCm)
{
	Mesh.ClearAllMeshSections();
	const float Scale=TileSizeCm/400.0f;
	int32 Section=0;
	auto Add = [&](FProp Prop, FVector Position, float Yaw=0, float FitSize=0, float ObjectScale=1)
	{
		if (Prop.IsEmpty()) return;
		if (FitSize>0)
		{
			const FBox Bounds=Prop.Bounds();
			const float Fit=FitSize/Bounds.GetSize().GetMax();
			Prop.Place(-FVector(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Min.Z)*Fit,0,Fit);
		}
		Prop.Place(FVector::ZeroVector,0,ObjectScale);
		Prop.Place(Position*Scale+FVector(0,0,SimCopterHangarPlacement::FloorLiftCm),Yaw,Scale);
		Mesh.CreateMeshSection_LinearColor(Section,Prop.Vertices,Prop.Triangles,Prop.Normals,
			Prop.UVs,Prop.VertexColors,Prop.Tangents,false);
		Mesh.SetMaterial(Section++,Material);
	};
	// +X is the entrance; +Y is on the left when looking in. Keep a clear central aisle,
	// with the front props recessed behind the jambs so the exterior has genuine parallax.
	Add(Car(),{250,112,0},-22,0,.42f);
	Add(ToolCabinet(),{252,67,0},-4,0,.33f);
	Add(Office(),{223,-101,0},8,0,.36f);
	Add(Chair(),{249,-84,0},24,0,.40f);
	Add(Shelves(),{164,-157,0},12,0,.42f);
	FProp Crates;
	Crates.Box({0,0,18},{43,55,36},FLinearColor(0.39f,0.26f,0.11f));
	Crates.Box({-4,0,43},{34,44,14},FLinearColor(0.47f,0.33f,0.16f));
	// Low freight cart and boxes to the left of the aircraft, with a clear centre aisle.
	Crates.Box({10,0,4},{85,51,6},Steel);
	for (float X : {-22.0f,40.0f}) for (float Y : {-23.0f,23.0f})
		Crates.Cylinder({X,Y,2},FVector::YAxisVector,6,4,Rubber);
	Add(Crates,{-55,185,3},8,0,.65f);
	FString Error;
	if (auto Library=FMaxisMeshLibrary::GetShared(OriginalRoot,Error))
	{
		Add(Aircraft(*Library,false),{-268,130,0},70,185);
		Add(Aircraft(*Library,true),{-155,-83,0},58,270);
	}
}
