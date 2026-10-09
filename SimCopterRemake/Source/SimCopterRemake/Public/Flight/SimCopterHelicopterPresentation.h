#pragma once
#include "CoreMinimal.h"
#include "Formats/MaxisProceduralMeshBuilder.h"

namespace SimCopterHelicopterPresentation
{
	void ApplyCatalogPaintRegions(FMaxisMeshObject& Body, int32 TypeIndex);
	// Palette-ramp bases are pre-lighting material selectors, not black paint.
	void MakePaintPalette(const TArray<FColor>& Source, TArray<FColor>& Out, int32 TypeIndex = INDEX_NONE);
	bool IsAgustaWindow(const FMaxisMeshObject& Object, const FMaxisMeshFace& Face);
	bool IsCabinWindow(const FMaxisMeshObject& Object, const FMaxisMeshFace& Face, int32 TypeIndex);
	FVector CabinSeat(int32 TypeIndex, int32 Index);
	FVector AgustaSeat(int32 Index);
	void AppendSeatedOccupant(const FVector& Seat, const FLinearColor& Shirt, bool bPatient, FMaxisMeshSection& Out);
}
