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

void SimCopterPilotEquipment::BuildTaser(FMaxisMeshSection& Out)
{
	Out = FMaxisMeshSection();
	const FLinearColor Yellow(0.95f,0.58f,0.015f), Black(0.025f,0.03f,0.035f), Steel(0.38f,0.43f,0.47f);
	Box(Out,FVector(0,0,2),FVector(9,2.8,3.2),Yellow); // yellow receiver
	Box(Out,FVector(8,0,2.4),FVector(2.5,3.2,3),Black); // replaceable dual cartridge
	Box(Out,FVector(-4,0,-5),FVector(2.7,2.4,5),Black,0.28); // raked grip
	Box(Out,FVector(-4,0,-10),FVector(3.4,2.65,0.9),Yellow); // battery heel
	Box(Out,FVector(2,0,-6),FVector(4,0.65,0.65),Black); // open trigger guard
	Box(Out,FVector(5.5,0,-3.4),FVector(0.6,0.65,2),Black);
	Box(Out,FVector(-0.5,0,-2.5),FVector(0.65,0.6,1.7),Steel,-0.3); // trigger
	for (double Side : {-1.0,1.0})
	{
		Box(Out,FVector(10.6,Side*1.6,2.5),FVector(0.3,0.8,1.5),Yellow);
		Cable(Out,FVector(10.9,Side*1.6,2.5),FVector(11.4,Side*1.6,2.5),0.45,Steel); // electrodes
		Box(Out,FVector(-5,Side*2.85,2),FVector(1.3,0.25,0.7),Black); // safety switch
		for (int32 I=0; I<4; ++I) Box(Out,FVector(-5.5+I*0.8,Side*2.85,4.5),FVector(0.18,0.2,0.8),Black);
	}
	Box(Out,FVector(7,0,5.8),FVector(0.5,0.5,0.5),Steel); // front sight
	Box(Out,FVector(-7,0,5.8),FVector(0.5,1.4,0.5),Black); // rear sight
	Box(Out,FVector(-7,0,5.9),FVector(0.3,0.5,0.5),FLinearColor(0.03f,0.7f,0.12f));
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
