#pragma once

#include "Formats/MaxisMeshReader.h"

// Small authored mesh helpers in the original model's X/Y-up/Z, 16.16 units.
// Built once per model, then reused by the city's existing instancing/collision path.
namespace SimCopterLandmarkGeometry
{
struct FBuilder
{
	FMaxisMeshObject& Mesh;
	void Face(const TArray<FVector>& Points, uint8 Finish)
	{
		FMaxisMeshFace F;
		F.FaceType = 15; F.MaterialIndex = Finish; F.VertexCount = Points.Num();
		const FVector N = FVector::CrossProduct(Points[1]-Points[0], Points[2]-Points[0]).GetSafeNormal().GetAbs();
		for (const FVector& P : Points)
		{
			F.VertexIndices.Add(Mesh.Vertices.Num());
			Mesh.Vertices.Add({int32(FMath::RoundToInt(P.X*65536)), int32(FMath::RoundToInt(P.Y*65536)), int32(FMath::RoundToInt(P.Z*65536))});
			const FVector2D UV = N.Y > 0.7 ? FVector2D(P.X,P.Z) : N.X > N.Z ? FVector2D(P.Z,P.Y) : FVector2D(P.X,P.Y);
			F.RawUVs.Add(FIntPoint(FMath::RoundToInt(UV.X*65536/16), FMath::RoundToInt((1-UV.Y/16)*65536)));
		}
		Mesh.Faces.Add(MoveTemp(F));
	}
	void Box(FVector C, FVector S, uint8 Finish, int32 TopFinish=INDEX_NONE)
	{
		const FVector A=C-S*.5, B=C+S*.5;
		Face({{A.X,A.Y,A.Z},{B.X,A.Y,A.Z},{B.X,B.Y,A.Z},{A.X,B.Y,A.Z}},Finish);
		Face({{B.X,A.Y,B.Z},{A.X,A.Y,B.Z},{A.X,B.Y,B.Z},{B.X,B.Y,B.Z}},Finish);
		Face({{A.X,A.Y,B.Z},{A.X,A.Y,A.Z},{A.X,B.Y,A.Z},{A.X,B.Y,B.Z}},Finish);
		Face({{B.X,A.Y,A.Z},{B.X,A.Y,B.Z},{B.X,B.Y,B.Z},{B.X,B.Y,A.Z}},Finish);
		Face({{A.X,B.Y,A.Z},{B.X,B.Y,A.Z},{B.X,B.Y,B.Z},{A.X,B.Y,B.Z}},TopFinish==INDEX_NONE?Finish:uint8(TopFinish));
		Face({{A.X,A.Y,B.Z},{B.X,A.Y,B.Z},{B.X,A.Y,A.Z},{A.X,A.Y,A.Z}},Finish);
	}
	void Ring(FVector C, double R0, double R1, double Height, uint8 Finish, int32 Sides=24, bool bCap=false)
	{
		for (int32 I=0; I<Sides; ++I)
		{
			const double A=2*PI*I/Sides, B=2*PI*(I+1)/Sides;
			const FVector P0=C+FVector(R0*cos(A),0,R0*sin(A)), P1=C+FVector(R0*cos(B),0,R0*sin(B));
			const FVector P2=C+FVector(R1*cos(B),Height,R1*sin(B)), P3=C+FVector(R1*cos(A),Height,R1*sin(A));
			if (R1 < .001) Face({P0,P1,C+FVector(0,Height,0)},Finish);
			else Face({P0,P1,P2,P3},Finish);
			if (bCap && R1 > .001) Face({C+FVector(0,Height,0),P3,P2},Finish);
		}
	}
	void Beam(FVector A, FVector B, double Radius, uint8 Finish)
	{
		const FVector D=(B-A).GetSafeNormal();
		const FVector U=FVector::CrossProduct(D,FMath::Abs(D.Y)>.9?FVector(1,0,0):FVector(0,1,0)).GetSafeNormal()*Radius;
		const FVector V=FVector::CrossProduct(D,U).GetSafeNormal()*Radius;
		Face({A-U-V,B-U-V,B+U-V,A+U-V},Finish);
		Face({A+U-V,B+U-V,B+U+V,A+U+V},Finish);
		Face({A+U+V,B+U+V,B-U+V,A-U+V},Finish);
		Face({A-U+V,B-U+V,B-U-V,A-U-V},Finish);
	}
	void Finish()
	{
		Mesh.Header.VertexCount=Mesh.Vertices.Num(); Mesh.Header.FaceCount=Mesh.Faces.Num();
	}
};
}
