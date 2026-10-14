#pragma once

#include "UI/SimCopterControllerHelp.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

// Live, code-drawn cockpit hardware. Both devices use exactly the same lamp and switch;
// only the engraved shortcut changes. No focus or hit testing is taken from gameplay.
class SSimCopterStatusPlate : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCopterStatusPlate) : _Scale(1.0f), _Indicator(false) {}
		SLATE_ATTRIBUTE(SimCopterControllerHelp::FState, State)
		SLATE_ARGUMENT(float, Scale)
		SLATE_ARGUMENT(bool, Indicator)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args)
	{
		State = Args._State; Scale = Args._Scale; bIndicator = Args._Indicator;
		SetVisibility(EVisibility::HitTestInvisible);
	}
	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return (bIndicator ? FVector2D(23, 49) : FVector2D(185, 22)) * Scale;
	}
	virtual bool ComputeVolatility() const override { return true; }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override
	{
		const auto Current = State.Get();
		const auto* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		const FLinearColor Silver(0.30f, 0.32f, 0.33f), Edge(0.055f, 0.060f, 0.065f);
		const FLinearColor Yellow(1.0f, 0.76f, 0.12f);
		auto Box = [&](float X, float Y, float W, float H, FLinearColor Color, const FSlateBrush* Brush = nullptr)
		{
			FSlateDrawElement::MakeBox(Elements, Layer++, Geometry.ToPaintGeometry(FVector2f(W * Scale, H * Scale),
				FSlateLayoutTransform(FVector2f(X * Scale, Y * Scale))), Brush ? Brush : White,
				ESlateDrawEffect::None, Color * Style.GetColorAndOpacityTint());
		};
		auto Text = [&](const FString& Label, float CenterX, float Y, float Size, FLinearColor Color, bool bStencil = false)
		{
			auto Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::Max(1, FMath::RoundToInt(Size * Scale)));
			Font.LetterSpacing = bStencil ? 80 : 0;
			const FVector2D Extent = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font);
			const float X = CenterX * Scale - Extent.X * 0.5f;
			FSlateDrawElement::MakeText(Elements, Layer++, Geometry.ToPaintGeometry(FVector2f(Extent),
				FSlateLayoutTransform(FVector2f(X, Y * Scale))), Label, Font, ESlateDrawEffect::None,
				Color * Style.GetColorAndOpacityTint());
			// Stencil bridges only divide enclosed counters; leave the rest of each letter intact.
			if (bStencil) for (int32 Index = 0; Index < Label.Len(); ++Index)
			{
				if (!FString(TEXT("ABDOPQR0689")).Contains(FString::Chr(Label[Index]))) continue;
				const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
				const float Start = Measure->Measure(Label.Left(Index), Font).X;
				const float End = Measure->Measure(Label.Left(Index + 1), Font).X;
				Box((X + (Start + End) * 0.5f) / Scale, Y + Extent.Y / Scale * 0.18f,
					0.5f, Extent.Y / Scale * 0.67f, Edge);
			}
		};
		const float W = bIndicator ? 23 : 185, H = bIndicator ? 49 : 22;
		Box(0, 0, W, H, Edge);
		Box(0.7f, 0.7f, W - 1.4f, H - 1.4f, FLinearColor(0.56f, 0.58f, 0.59f));
		Box(1.5f, 1.5f, W - 3, H - 3, Silver);
		for (float Y = 3; Y < H - 2; Y += 2)
			Box(2, Y, W - 4, 0.3f, FLinearColor(0.40f, 0.42f, 0.43f, 0.25f));
		if (!bIndicator)
		{
			Box(8, 4, W - 16, H - 8, Edge);
			Text(SimCopterControllerHelp::GetHelpShortcut(Current).ToUpper(), W * 0.5f, 4.5f, 8.0f, Yellow, true);
			for (float X : {3.5f, W - 3.5f})
			{
				Box(X - 1.5f, 9.5f, 3, 3, Edge, &Round);
				Box(X - 1, 10.6f, 2, 0.5f, FLinearColor(0.7f, 0.72f, 0.73f));
			}
			return Layer;
		}
		Box(2.5f, 3, 18, 7, Edge);
		Text(TEXT("TURBO"), 11.5f, 3, 5.0f, Yellow);
		const bool bOn = Current.bTurboAvailable && Current.bTurboEnabled;
		Box(6.5f, 11, 10, 10, Edge, &Round);
		Box(7.5f, 12, 8, 8, FLinearColor(0.64f, 0.65f, 0.66f), &Round);
		Box(8.3f, 12.8f, 6.4f, 6.4f, bOn ? FLinearColor(1, 0.004f, 0.001f) : FLinearColor(0.055f, 0.003f, 0.002f), &Round);
		Box(9.5f, 13.8f, 2.4f, 1.3f, bOn ? FLinearColor(1, 0.52f, 0.35f) : FLinearColor(0.24f, 0.06f, 0.04f), &Round);
		Text(Current.bTurboAvailable ? (bOn ? TEXT("ON") : TEXT("OFF")) : TEXT("N/A"), 11.5f, 20, 5.0f, Yellow);
		Box(5, 28, 13, 18, Edge);
		Box(6, 29, 11, 16, FLinearColor(0.61f, 0.63f, 0.64f));
		Box(7.5f, 32.5f, 8, 8, Edge, &Round);
		Box(9.8f, bOn ? 31 : 36, 3.4f, 6, FLinearColor(0.72f, 0.74f, 0.75f));
		Box(8, bOn ? 29 : 39, 7, 5, FLinearColor(0.025f, 0.028f, 0.031f), &Round);
		Box(8.7f, bOn ? 29.7f : 39.7f, 5.6f, 1.2f, FLinearColor(0.44f, 0.46f, 0.48f), &Round);
		return Layer;
	}
private:
	TAttribute<SimCopterControllerHelp::FState> State;
	float Scale = 1;
	bool bIndicator = false;
	FSlateRoundedBoxBrush Round{FLinearColor::White};
};
