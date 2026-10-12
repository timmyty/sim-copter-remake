#pragma once

#include "Formats/MaxisProceduralMeshBuilder.h"
#include "Widgets/SLeafWidget.h"

// Read-only catalog geometry: never spawns or modifies a flyable aircraft.
namespace SimCopterEquipmentPreview
{
struct FModel
{
	FMaxisMeshSection Mesh;
	FBox Bounds = FBox(ForceInit);
	FString Error;
	int32 HighlightTriangleCount = 0;
};
FModel Build(int32 TypeIndex, int32 OwnedMask, int32 HighlightRow);
const TCHAR* GetFittingDescription(int32 Row);
}

class SSimCopterEquipmentPreview : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCopterEquipmentPreview) {}
		SLATE_ATTRIBUTE(int32, TypeIndex)
		SLATE_ATTRIBUTE(int32, OwnedMask)
		SLATE_ATTRIBUTE(int32, HighlightRow)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual void Tick(const FGeometry&, double, float) override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(300, 230); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&,
		FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
	TAttribute<int32> TypeIndex, OwnedMask, HighlightRow;
	int32 LastType = INDEX_NONE, LastMask = INDEX_NONE, LastRow = INDEX_NONE;
	struct FTriangle { FVector2f Points[3]; FLinearColor Color; double Depth; };
	TArray<FTriangle> Triangles;
	FBox2D ProjectedBounds;
	FString Error;
};

class SSimCopterEquipmentIcon : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCopterEquipmentIcon) {}
		SLATE_ARGUMENT(int32, Row)
		SLATE_ATTRIBUTE(bool, Owned)
		SLATE_ATTRIBUTE(bool, Highlighted)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(34, 34); }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&,
		FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
	int32 Row = 0;
	TAttribute<bool> Owned, Highlighted;
};
