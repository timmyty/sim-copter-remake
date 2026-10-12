// Shared native geometry for the reference-matched warehouse and its furnishings.
#pragma once
#include "ProceduralMeshComponent.h"
#include "Formats/MaxisProceduralMeshBuilder.h"

namespace SimCopterHangarGeometry
{
struct FProp : FMaxisMeshSection
{
	void Quad(FVector A, FVector B, FVector C, FVector D, FLinearColor Color)
	{
		const FVector Normal = FVector::CrossProduct(B-A, C-A).GetSafeNormal();
		// Give the inside of roof/window sheets its own inward normal. Reversing only the
		// triangle winding leaves the underside lit as if it still faced the outdoor sun.
		for (int32 Side=0; Side<2; ++Side)
		{
			const int32 Base=Vertices.Num();
			Vertices.Append({A,B,C,D});
			UVs.Append({FVector2D(0,0),FVector2D(1,0),FVector2D(1,1),FVector2D(0,1)});
			for (int32 I=0; I<4; ++I)
			{
				Normals.Add(Side ? -Normal : Normal); VertexColors.Add(Color);
				Tangents.Add(FProcMeshTangent((B-A).GetSafeNormal(), false));
			}
			// UE's visible clockwise face has the opposite winding from the cross-product
			// convention above. Pair each visible side with the normal facing that side.
			if (Side) Triangles.Append({Base,Base+1,Base+2});
			else Triangles.Append({Base,Base+2,Base+1});
			// Gables and cylinder caps pass a repeated fourth corner: do not submit a
			// zero-area triangle to Chaos when those faces also form the building collision.
			if (!FVector::CrossProduct(C-A,D-A).IsNearlyZero())
			{
				if (Side) Triangles.Append({Base,Base+2,Base+3});
				else Triangles.Append({Base,Base+3,Base+2});
			}
		}
	}

	void Box(FVector Center, FVector Size, FLinearColor Color)
	{
		const FVector A=Center-Size*0.5, B=Center+Size*0.5;
		Quad({A.X,A.Y,A.Z},{A.X,A.Y,B.Z},{A.X,B.Y,B.Z},{A.X,B.Y,A.Z},Color);
		Quad({B.X,A.Y,A.Z},{B.X,B.Y,A.Z},{B.X,B.Y,B.Z},{B.X,A.Y,B.Z},Color);
		Quad({A.X,A.Y,A.Z},{B.X,A.Y,A.Z},{B.X,A.Y,B.Z},{A.X,A.Y,B.Z},Color);
		Quad({A.X,B.Y,A.Z},{A.X,B.Y,B.Z},{B.X,B.Y,B.Z},{B.X,B.Y,A.Z},Color);
		Quad({A.X,A.Y,B.Z},{B.X,A.Y,B.Z},{B.X,B.Y,B.Z},{A.X,B.Y,B.Z},Color);
		Quad({A.X,A.Y,A.Z},{A.X,B.Y,A.Z},{B.X,B.Y,A.Z},{B.X,A.Y,A.Z},Color);
	}

	void Cylinder(FVector Center, FVector Axis, float Radius, float Length, FLinearColor Color)
	{
		FVector U,V; Axis.FindBestAxisVectors(U,V);
		const FVector A=Center-Axis*Length*0.5, B=Center+Axis*Length*0.5;
		for (int32 I=0; I<12; ++I)
		{
			const float T0=2*PI*I/12, T1=2*PI*(I+1)/12;
			const FVector R0=Radius*(U*FMath::Cos(T0)+V*FMath::Sin(T0));
			const FVector R1=Radius*(U*FMath::Cos(T1)+V*FMath::Sin(T1));
			Quad(A+R0,A+R1,B+R1,B+R0,Color);
			Quad(A,A+R1,A+R0,A,Color);
			Quad(B,B+R0,B+R1,B,Color);
		}
	}

	void Append(const FMaxisMeshSection& Other, FVector Offset=FVector::ZeroVector)
	{
		const int32 Base=Vertices.Num();
		for (const FVector& P : Other.Vertices) Vertices.Add(P+Offset);
		for (int32 I : Other.Triangles) Triangles.Add(Base+I);
		Normals.Append(Other.Normals); UVs.Append(Other.UVs);
		VertexColors.Append(Other.VertexColors); Tangents.Append(Other.Tangents);
	}

	void Place(FVector Center, float Yaw, float Scale)
	{
		const FQuat Rotation(FVector::UpVector,FMath::DegreesToRadians(Yaw));
		for (FVector& P : Vertices) P=Rotation.RotateVector(P)*Scale+Center;
		for (FVector& N : Normals) N=Rotation.RotateVector(N);
		for (auto& T : Tangents) T.TangentX=Rotation.RotateVector(T.TangentX);
	}

	void Bar(FVector A, FVector B, float Radius, FLinearColor Color)
	{
		FProp Segment;
		Segment.Box(FVector::ZeroVector,FVector(Radius*2,Radius*2,FVector::Distance(A,B)),Color);
		const FQuat Rotation=FQuat::FindBetweenNormals(FVector::UpVector,(B-A).GetSafeNormal());
		for (FVector& P : Segment.Vertices) P=Rotation.RotateVector(P)+(A+B)*0.5;
		for (FVector& N : Segment.Normals) N=Rotation.RotateVector(N);
		for (auto& T : Segment.Tangents) T.TangentX=Rotation.RotateVector(T.TangentX);
		Append(Segment);
	}

	void Apply(UProceduralMeshComponent& Mesh, int32 Section, UMaterialInterface* Material, bool bCollision=false) const
	{
		Mesh.CreateMeshSection_LinearColor(Section,Vertices,Triangles,Normals,UVs,VertexColors,Tangents,bCollision);
		Mesh.SetMaterial(Section,Material);
	}

	FBox Bounds() const
	{
		FBox Result(ForceInit);
		for (const FVector& P : Vertices) Result+=P;
		return Result;
	}
};

}
