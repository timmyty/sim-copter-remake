#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class USimCopterHangarArt;
class USimCopterSettings;
struct FSlateBrush;

// Original DASH2 wiring and continuous cockpit trim, behind the existing control layout.
class SSimCopterCockpitFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCopterCockpitFrame) : _Art(nullptr), _Settings(nullptr), _Scale(1.0f) {}
		SLATE_ARGUMENT(USimCopterHangarArt*, Art)
		SLATE_ARGUMENT(USimCopterSettings*, Settings)
		SLATE_ARGUMENT(float, Scale)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	static FVector2D WiringSize(float Scale, const FVector2D& ViewportSize);
	bool IsCollapsed() const { return bCollapsed; }
	FReply ToggleWiring();
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool Enabled) const override;
private:
	TWeakObjectPtr<USimCopterSettings> Settings;
	const FSlateBrush* Wiring = nullptr;
	float Scale = 1.0f;
	bool bCollapsed = false;
};
