#include "Ground/SimCopterRescueDeck.h"
#include "ProceduralMeshComponent.h"

bool SimCopterRescueDeck::FindSurface(const UProceduralMeshComponent* Mesh, const FVector& Candidate, FVector& OutFeet)
{
	if (!IsValid(Mesh)) return false;
	bool bFound = false;
	double Top = -DBL_MAX;
	const FTransform& Transform = Mesh->GetComponentTransform();
	for (int32 SectionIndex = 0; SectionIndex < Mesh->GetNumSections(); ++SectionIndex)
	{
		const FProcMeshSection* Section = const_cast<UProceduralMeshComponent*>(Mesh)->GetProcMeshSection(SectionIndex);
		if (!Section) continue;
		for (int32 I = 0; I + 2 < Section->ProcIndexBuffer.Num(); I += 3)
		{
			const FVector A = Transform.TransformPosition(Section->ProcVertexBuffer[Section->ProcIndexBuffer[I]].Position);
			const FVector B = Transform.TransformPosition(Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+1]].Position);
			const FVector C = Transform.TransformPosition(Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+2]].Position);
			const double D = (B.Y-C.Y)*(A.X-C.X) + (C.X-B.X)*(A.Y-C.Y);
			if (FMath::Abs(D) < UE_SMALL_NUMBER) continue;
			const double U = ((B.Y-C.Y)*(Candidate.X-C.X) + (C.X-B.X)*(Candidate.Y-C.Y)) / D;
			const double V = ((C.Y-A.Y)*(Candidate.X-C.X) + (A.X-C.X)*(Candidate.Y-C.Y)) / D;
			if (U < 0 || V < 0 || U + V > 1) continue;
			const double Z = U*A.Z + V*B.Z + (1-U-V)*C.Z;
			if (Z > Top) { Top = Z; bFound = true; }
		}
	}
	if (bFound) OutFeet = FVector(Candidate.X, Candidate.Y, Top + 1.0);
	return bFound;
}
