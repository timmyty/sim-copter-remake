#include "SSimCopterPlaneControls.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Input/Reply.h"

void SSimCopterPlaneControls::Construct(const FArguments& Args)
{
	GetPlane = Args._GetPlane; Scale = Args._Scale;
	SetToolTipText(FText::FromString(TEXT("W / S: climb / descend\nSpace / Ctrl: faster / slower\nG or gear knob: raise / lower landing gear\nHold Space to start the engine.")));
}

FVector2D SSimCopterPlaneControls::ComputeDesiredSize(float) const { return FVector2D(94, 125) * Scale; }

int32 SSimCopterPlaneControls::OnPaint(const FPaintArgs&, const FGeometry& Geo, const FSlateRect&,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	const auto* Plane = GetPlane();
	if (!Plane || !Plane->IsFixedWingAircraft()) return Layer;
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	auto Box = [&](float X, float Y, float W, float H, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(Out, ++Layer, Geo.ToPaintGeometry(FVector2f(W*Scale,H*Scale),
			FSlateLayoutTransform(FVector2f(X*Scale,Y*Scale))), White, ESlateDrawEffect::None, Color);
	};
	const FLinearColor Ink(.86f,.82f,.66f), Edge(.46f,.46f,.42f), Dark(.045f,.047f,.042f);
	auto Text = [&](float X, float Y, const FString& Value, FLinearColor Color, int32 Size = 7)
	{
		FSlateDrawElement::MakeText(Out, ++Layer, Geo.ToPaintGeometry(FVector2f(94*Scale,20*Scale),
			FSlateLayoutTransform(FVector2f(X*Scale,Y*Scale))), Value,
			FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(7,FMath::RoundToInt(Size*Scale))), ESlateDrawEffect::None, Color);
	};
	Box(0,0,94,125,Dark); Box(2,2,90,121,Edge); Box(4,4,86,117,FLinearColor(.16f,.17f,.16f));
	for (FVector2D P : {FVector2D(7,7),FVector2D(85,7),FVector2D(7,115),FVector2D(85,115)})
	{ Box(P.X,P.Y,3,3,Edge); Box(P.X,P.Y+1,3,1,Dark); }
	Text(14,9,TEXT("AIRPLANE"),Ink,8);
	Text(10,28,TEXT("GEAR"),Ink); Text(51,28,TEXT("SPEED"),Ink);
	Box(23,49,6,39,Dark);
	const bool Down = Plane->IsPlaneLandingGearDown();
	Text(14,40,TEXT("UP"),Down ? Edge : Ink);
	Text(10,91,TEXT("DOWN"),Down ? FLinearColor(.38f,.88f,.38f) : Edge);
	// Raised square-edged chrome knob with a dark grip and ivory position mark.
	const float Y = Down ? 75 : 51;
	Box(14,Y,24,12,Dark); Box(14,Y,22,10,Edge); Box(17,Y+2,16,6,FLinearColor(.25f,.27f,.26f)); Box(23,Y+3,4,4,Ink);
	Box(62,45,6,46,Dark);
	for (int32 I=0; I<6; ++I) Box(71,46+I*8,4,1,Ink);
	const float ThrottleY = 84 - Plane->GetPlaneThrottle()*38;
	Box(55,ThrottleY,19,6,Edge); Box(59,ThrottleY+2,11,2,Ink);
	Text(52,96,FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Plane->GetPlaneThrottle()*100)),Ink);
	Text(13,108,TEXT("G"),Ink); Text(43,108,TEXT("SPC / CTRL"),Ink,6);
	return Layer;
}

void SSimCopterPlaneControls::SetThrottleFromPointer(const FGeometry& Geo, const FPointerEvent& Event)
{
	if (auto* Plane = GetPlane()) Plane->SetPlaneThrottle((87 - Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()).Y/Scale)/38);
}

FReply SSimCopterPlaneControls::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton()!=EKeys::LeftMouseButton) return FReply::Unhandled();
	if (Geo.AbsoluteToLocal(Event.GetScreenSpacePosition()).X/Scale < 44)
	{
		if (auto* Plane=GetPlane()) Plane->TogglePlaneLandingGear();
		return FReply::Handled();
	}
	SetThrottleFromPointer(Geo,Event);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}
FReply SSimCopterPlaneControls::OnMouseMove(const FGeometry& Geo,const FPointerEvent& Event)
{
	if (!HasMouseCapture()) return FReply::Unhandled();
	SetThrottleFromPointer(Geo,Event); return FReply::Handled();
}
FReply SSimCopterPlaneControls::OnMouseButtonUp(const FGeometry&,const FPointerEvent& Event)
{
	return Event.GetEffectingButton()==EKeys::LeftMouseButton && HasMouseCapture() ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled();
}
FCursorReply SSimCopterPlaneControls::OnCursorQuery(const FGeometry&,const FPointerEvent&) const { return FCursorReply::Cursor(EMouseCursor::Hand); }
