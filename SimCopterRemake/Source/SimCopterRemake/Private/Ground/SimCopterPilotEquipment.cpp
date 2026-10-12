#include "Ground/SimCopterPilotEquipment.h"

namespace
{
void Quad(FMaxisMeshSection& Out, FVector A, FVector B, FVector C, FVector D, FLinearColor Color, bool bTwoSided = false)
{
	const FVector N = FVector::CrossProduct(B-A,C-A).GetSafeNormal();
	const int32 First = Out.Vertices.Num();
	for (const FVector& V : {A,B,C,D})
	{
		Out.Vertices.Add(V); Out.Normals.Add(N); Out.UVs.Add(FVector2D::ZeroVector);
		Out.VertexColors.Add(Color); Out.Tangents.Add(FProcMeshTangent((B-A).GetSafeNormal(),false)); Out.LocalBounds += V;
	}
	Out.Triangles.Append({First,First+1,First+2,First,First+2,First+3});
	if (bTwoSided) Out.Triangles.Append({First+2,First+1,First,First+3,First+2,First});
}
void Box(FMaxisMeshSection& Out, FVector Center, FVector Half, FLinearColor Color, double Rake = 0)
{
	for (const FVector& N : {FVector(1,0,0),FVector(-1,0,0),FVector(0,1,0),FVector(0,-1,0),FVector(0,0,1),FVector(0,0,-1)})
	{
		const FVector U = FMath::Abs(N.Z)<0.5 ? FVector(0,0,1) : FVector(1,0,0);
		const FVector V = FVector::CrossProduct(N,U);
		auto P = [&](FVector Corner) { FVector Result = Center + Corner*Half; Result.X += (Result.Z-Center.Z)*Rake; return Result; };
		Quad(Out,P(N-U-V),P(N+U-V),P(N+U+V),P(N-U+V),Color);
	}
}
void Cable(FMaxisMeshSection& Out, FVector A, FVector B, double Radius, FLinearColor Color)
{
	const FVector Axis = (B-A).GetSafeNormal();
	const FVector U = FVector::CrossProduct(Axis,FMath::Abs(Axis.Z)<0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	const FVector V = FVector::CrossProduct(Axis,U);
	for (int32 I=0; I<8; ++I)
	{
		const double T=I*UE_TWO_PI/8, S=(I+1)*UE_TWO_PI/8;
		const FVector P=(U*FMath::Cos(T)+V*FMath::Sin(T))*Radius, Q=(U*FMath::Cos(S)+V*FMath::Sin(S))*Radius;
		Quad(Out,A+P,A+Q,B+Q,B+P,Color);
	}
}
}

void SimCopterPilotEquipment::BuildTaser(FMaxisMeshSection& Out, float Deployment)
{
	Out = FMaxisMeshSection();
	const FLinearColor Shell(.16f,.22f,.25f), Black(.018f,.024f,.029f), Steel(.48f,.58f,.60f);
	const float Open=FMath::Clamp(Deployment,0.f,1.f);
	// Remake energy weapon: graphite grip, layered ceramic receiver and four hinged emitters.
	// Keep the original grip anchor so the shoulder pose still holds the actual handle.
	Box(Out,FVector(-1,0,2),FVector(8,3,3.2),Shell);
	Box(Out,FVector(-6,0,2),FVector(2.8,3.5,3.7),Black);
	Box(Out,FVector(-4,0,-5),FVector(2.7,2.4,5),Black,.28);
	Box(Out,FVector(-4,0,-10),FVector(3.4,2.65,.9),Steel);
	Box(Out,FVector(2,0,-6),FVector(4,.65,.65),Steel);
	Box(Out,FVector(5.5,0,-3.4),FVector(.6,.65,2),Steel);
	Box(Out,FVector(-.5,0,-2.5),FVector(.65,.6,1.7),Steel,-.3);
	for (int32 Ring=0; Ring<3; ++Ring)
		for (int32 Segment=0; Segment<12; ++Segment)
		{
			const double A=Segment*UE_TWO_PI/12, B=(Segment+1)*UE_TWO_PI/12;
			Cable(Out,FVector(5+Ring*1.2,3.3*FMath::Cos(A),2+3.3*FMath::Sin(A)),
				FVector(5+Ring*1.2,3.3*FMath::Cos(B),2+3.3*FMath::Sin(B)),.32,Steel);
		}
	for (int32 Petal=0; Petal<4; ++Petal)
	{
		const double Angle=Petal*UE_HALF_PI+UE_PI/4;
		const FVector Radial(0,FMath::Cos(Angle),FMath::Sin(Angle));
		const FVector Hinge=FVector(7,0,2)+Radial*2.5;
		const FVector Tip=FVector(14-2*Open,0,2)+Radial*(2.5+5.2*Open);
		Cable(Out,Hinge,Tip,1.45,Shell);
		Cable(Out,Hinge+Radial*.9,Tip+Radial*.9,.35,Steel);
		Cable(Out,Tip,Tip+FVector(1.6,0,0)-Radial*.6,.6,Black);
	}
	for (double Side : {-1.0,1.0})
	{
		Box(Out,FVector(-1,Side*3.05,2),FVector(4.4,.15,1.8),Black);
		for (int32 I=0; I<4; ++I) Box(Out,FVector(-4,Side*2.45,-3-I*1.4),FVector(2,.15,.24),Shell);
	}
	Box(Out,FVector(-2,0,5.5),FVector(4.5,1,.5),Black);
}

void SimCopterPilotEquipment::BuildTaserCircuits(FMaxisMeshSection& Out, float Deployment, float Energy, float Phase)
{
	Out=FMaxisMeshSection();
	const FLinearColor Color=FMath::Lerp(FLinearColor(.018f,.07f,.065f),FLinearColor(.035f,.85f,.65f),Energy);
	for (double Side : {-1.0,1.0})
	{
		for (int32 Path=0; Path<3; ++Path)
		{
			const double Z=.8+Path*1.1;
			Cable(Out,FVector(-5,Side*3.24,Z),FVector(-2+Path,Side*3.24,Z),.13,Color);
			Cable(Out,FVector(-2+Path,Side*3.24,Z),FVector(-1+Path,Side*3.24,Z+.6),.13,Color);
			Cable(Out,FVector(-1+Path,Side*3.24,Z+.6),FVector(4.5,Side*3.24,Z+.6),.13,Color);
		}
		for (int32 Lamp=0; Lamp<4; ++Lamp)
			Box(Out,FVector(-6.5+Lamp*.65,Side*3.57,3.5),FVector(.19,.08,.35),Color);
	}
	for (int32 Petal=0; Petal<4; ++Petal)
	{
		const double Angle=Petal*UE_HALF_PI+UE_PI/4;
		const FVector R(0,FMath::Cos(Angle),FMath::Sin(Angle));
		const FVector Tip=FVector(14-2*Deployment,0,2)+R*(2.5+5.2*Deployment);
		Cable(Out,FVector(7,0,2)+R*1.2,Tip-R*1.3,.22,Color);
		if (Energy>.01f && Deployment>.5f)
		{
			const FVector Center(14,0,2);
			const FVector Mid=(Tip+Center)*.5+FVector(0,FMath::Sin(Phase*27+Petal),FMath::Cos(Phase*21+Petal))*.7;
			Cable(Out,Tip-R*1.4,Mid,.10,Color); Cable(Out,Mid,Center,.10,Color);
		}
	}
	for (auto& UV:Out.UVs) UV=FVector2D(.5,.5);
}

void SimCopterPilotEquipment::BuildTaserOrb(FMaxisMeshSection& Out, float Radius, float Phase, float Energy, bool bCore)
{
	Out=FMaxisMeshSection();
	if (Radius<=.001f || Energy<=.001f) return;
	const FLinearColor Color=FMath::Lerp(FLinearColor(.12f,.18f,.19f),FLinearColor(.04f,.9f,.72f),Energy);
	if (bCore) for (int32 Ring=0; Ring<8; ++Ring) for (int32 Slice=0; Slice<16; ++Slice)
	{
		const auto P=[&](int32 R,int32 S)
		{
			const double Lat=-UE_HALF_PI+R*UE_PI/8, Lon=S*UE_TWO_PI/16;
			return FVector(FMath::Sin(Lat),FMath::Cos(Lat)*FMath::Cos(Lon),FMath::Cos(Lat)*FMath::Sin(Lon))*Radius*.70;
		};
		Quad(Out,P(Ring,Slice),P(Ring,Slice+1),P(Ring+1,Slice+1),P(Ring+1,Slice),Color,true);
	}
	for (int32 Ring=0; Ring<3; ++Ring) for (int32 Segment=0; Segment<32; ++Segment)
	{
		const auto P=[&](int32 K)
		{
			const float A=K*UE_TWO_PI/32+Phase*(Ring%2 ? -3 : 2);
			const float R=Radius*(bCore ? 1.f : 1.f+.08f*FMath::Sin(K*9.f+Phase*35));
			const FVector V(FMath::Sin(A)*R,FMath::Cos(A)*R,0);
			return FRotator(Ring*60,0,35+Phase*25).RotateVector(V);
		};
		Cable(Out,P(Segment),P(Segment+1),Radius*.035,FLinearColor(.15f,.7f,1.f)*Energy);
	}
	for (auto& UV:Out.UVs) UV=FVector2D(.5,.5);
}

void SimCopterPilotEquipment::BuildParachute(FMaxisMeshSection& Out)
{
	Out = FMaxisMeshSection();
	constexpr int32 Panels=16, Rings=6;
	auto Point = [](double R, double Angle)
	{
		return FVector(75*R*FMath::Cos(Angle),75*R*FMath::Sin(Angle),82+35*FMath::Sqrt(FMath::Max(0.0,1-R*R)));
	};
	for (int32 Panel=0; Panel<Panels; ++Panel)
	{
		const double A=Panel*UE_TWO_PI/Panels, B=(Panel+1)*UE_TWO_PI/Panels;
		const FLinearColor Color=Panel%2 ? FLinearColor(0.93f,0.88f,0.75f) : FLinearColor(0.95f,0.18f,0.035f);
		for (int32 Ring=0; Ring<Rings; ++Ring)
		{
			const double R0=0.06+0.94*Ring/Rings, R1=0.06+0.94*(Ring+1)/Rings;
			Quad(Out,Point(R0,A),Point(R1,A),Point(R1,B),Point(R0,B),Color,true);
		}
		Cable(Out,FVector(0,FMath::Sin(A)>0 ? 7 : -7,9),Point(1,A),0.22,FLinearColor(0.08f,0.08f,0.06f));
		Cable(Out,Point(1,A),Point(1,B),0.35,FLinearColor(0.7f,0.55f,0.35f));
	}
	Box(Out,FVector(-6,0,6),FVector(3,8,10),FLinearColor(0.08f,0.09f,0.05f)); // pack
}
