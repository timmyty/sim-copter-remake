#include "SSimCopterCockpitFrame.h"

#include "Game/SimCopterSettings.h"
#include "UI/SimCopterHangarArt.h"
#include "UI/SSimCopterDashboard.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

FVector2D SSimCopterCockpitFrame::WiringSize(float InScale, const FVector2D& ViewportSize)
{
	// SCHOOK: CockpitCreate 0x00412440 uses DASH2's 84x291 page at the right edge.
	// Keep half its share of the original 640x480 screen, while bridging the current
	// top-right controls to the instruments instead of leaving a floating wiring box.
	const double Height = FMath::Max(1.0,ViewportSize.Y - SSimCopterDashboard::GetPanelScreenHeight(InScale) + 14*InScale);
	const double Area = ViewportSize.X * ViewportSize.Y * (84.0*291.0/(640.0*480.0)) * 0.5;
	return FVector2D(Area/Height,Height);
}

void SSimCopterCockpitFrame::Construct(const FArguments& InArgs)
{
	Scale = FMath::Max(0.5f,InArgs._Scale);
	Settings = InArgs._Settings;
	bCollapsed = Settings.IsValid() && Settings->IsCockpitWiringCollapsed();
	if (InArgs._Art)
	{
		Wiring = InArgs._Art->GetBitmap(TEXT("DASH2.BMP"));
	}
	SetVisibility(TAttribute<EVisibility>::CreateLambda([this] { return Settings.IsValid() && !Settings->AreCockpitDecorationsVisible()
		? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; }));
	SetClipping(EWidgetClipping::ClipToBounds);
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom)
		.Padding(FMargin(0,0,3*Scale,SSimCopterDashboard::GetPanelScreenHeight(Scale)+8*Scale))
		[
			SNew(SBox).WidthOverride(18*Scale).HeightOverride(18*Scale)
			[
				SNew(SButton).IsFocusable(false).ContentPadding(0)
				.ButtonColorAndOpacity(FLinearColor(0.16f,0.17f,0.17f))
				.ToolTipText_Lambda([this] { return FText::FromString(bCollapsed ? TEXT("Extend cockpit wiring") : TEXT("Retract cockpit wiring")); })
				.OnClicked(this,&SSimCopterCockpitFrame::ToggleWiring)
				.HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text_Lambda([this] { return FText::FromString(bCollapsed ? TEXT("\u2039") : TEXT("\u203a")); })
					.Font(FCoreStyle::GetDefaultFontStyle("Bold",FMath::RoundToInt(15*Scale)))
					.ColorAndOpacity(FLinearColor(0.96f,0.89f,0.61f))
				]
			]
		]
	];
}

FReply SSimCopterCockpitFrame::ToggleWiring()
{
	bCollapsed = !bCollapsed;
	if (Settings.IsValid()) Settings->SetCockpitWiringCollapsed(bCollapsed);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

int32 SSimCopterCockpitFrame::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& Cull, FSlateWindowElementList& Elements, int32 Layer,
	const FWidgetStyle& Style, bool Enabled) const
{
	const FVector2f Size = Geometry.GetLocalSize();
	const auto Box = [&](FVector2f At, FVector2f Extent, const FSlateBrush* Brush, FLinearColor Color)
	{
		if (Brush && Extent.X > 0 && Extent.Y > 0)
			FSlateDrawElement::MakeBox(Elements,Layer,Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(At)),Brush,
				ESlateDrawEffect::None,Color * Style.GetColorAndOpacityTint());
	};
	const auto* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	// A continuous sill joins the map, passenger well and instruments at their existing anchors.
	Box(FVector2f(0,Size.Y-10*Scale),FVector2f(Size.X,10*Scale),White,FLinearColor(0.055f,0.058f,0.06f));
	Box(FVector2f(0,Size.Y-10*Scale),FVector2f(Size.X,2*Scale),White,FLinearColor(0.31f,0.32f,0.31f));
	Box(FVector2f(Size.X-5*Scale,0),FVector2f(5*Scale,Size.Y),White,FLinearColor(0.10f,0.11f,0.11f));
	const FVector2f WireSize(WiringSize(Scale,FVector2D(Size)));
	const float VisibleWidth = bCollapsed ? 6*Scale : WireSize.X;
	Box(FVector2f(Size.X-VisibleWidth,0),WireSize,Wiring,FLinearColor::White);
	return SCompoundWidget::OnPaint(Args,Geometry,Cull,Elements,Layer+1,Style,Enabled);
}
