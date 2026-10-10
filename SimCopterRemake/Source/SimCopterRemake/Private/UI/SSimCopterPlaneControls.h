#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class ASimCopterHelicopterPawn;

// Compact metal plate beside the original gauges; coordinates are dashboard pixels.
class SSimCopterPlaneControls : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCopterPlaneControls) : _Scale(1) {}
		SLATE_ARGUMENT(TFunction<ASimCopterHelicopterPawn*()>, GetPlane)
		SLATE_ARGUMENT(float, Scale)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
	virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override;
private:
	TFunction<ASimCopterHelicopterPawn*()> GetPlane;
	float Scale = 1;
	void SetThrottleFromPointer(const FGeometry&, const FPointerEvent&);
};
