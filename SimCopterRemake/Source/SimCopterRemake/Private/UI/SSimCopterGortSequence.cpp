#include "UI/SSimCopterGortSequence.h"
#include "UI/SimCopterGortSequence.h"
#include "UI/SimCopterFrontEndPage.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SSimCopterGortSequence::Construct(const FArguments& Args)
{
	Audio = Args._Audio;
	OnClosed = Args._OnClosed;
	using namespace SimCopterFrontEnd;
	auto Canvas = SNew(SConstraintCanvas);
	// SCHOOK: AlienEndingDraw 0x00446a20. A palette fade reveals the still
	// martian.bmp; there are no mouth-animation frames in this renderer.
	AddAt(Canvas, {0, 0, 640, 480}, SNew(SImage).Image(Args._Art)
		.ColorAndOpacity_Lambda([this]
		{
			const float Gain = FMath::Clamp(float(ElapsedSeconds / SimCopterGort::FadeSeconds), 0.f, 1.f);
			return FLinearColor(Gain, Gain, Gain, 1);
		}));
	for (int32 Speaker = 0; Speaker < 2; ++Speaker)
	{
		// FUN_004455e0: the original left/right subtitle rectangles. Default
		// font height is 12 (FUN_00460a70); keep long bonus lines inside them.
		const FRect Rect = Speaker == 0 ? FRect{30, 340, 272, 478} : FRect{352, 340, 610, 478};
		AddAt(Canvas, Rect, SNew(STextBlock)
			.Text_Lambda([this, Speaker] { return GetSubtitle(Speaker); })
			.Font(PageFont(12)).ColorAndOpacity(FLinearColor(FColor(0xe8, 0xe8, 0xe8)))
			.WrapTextAt(Rect.Width()).AutoWrapText(false)
			.ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(FLinearColor::Black)
			.Clipping(EWidgetClipping::ClipToBounds));
	}
	ChildSlot[SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor::Black)
		[SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
				[SNew(SBox).WidthOverride(640).HeightOverride(480)[Canvas]]]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(8)
			[SNew(SButton).ContentPadding(FMargin(16, 5)).Text(FText::FromString(TEXT("Return to game (Esc / B)")))
				.OnClicked_Lambda([this] { Finish(); return FReply::Handled(); })]]];
}

SSimCopterGortSequence::~SSimCopterGortSequence() { StopVoice(); }

void SSimCopterGortSequence::StopVoice()
{
	if (Audio.IsValid() && Voice.IsValid()) Audio->StopStandaloneSound(Voice.Get());
	Voice.Reset();
}

void SSimCopterGortSequence::Finish()
{
	if (bClosed) return;
	const TSharedRef<SWidget> KeepAlive = AsShared();
	bClosed = true;
	StopVoice(); // Stop immediately, even while Slate retains the old widget for a frame.
	OnClosed.ExecuteIfBound();
}

void SSimCopterGortSequence::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
	if (bClosed || !FMath::IsFinite(DeltaTime)) return;
	// Slate time keeps advancing while the city is paused. One cue change sets
	// BOTH subtitle and voice on this frame, so separate audio/text timers cannot drift.
	ElapsedSeconds += FMath::Max(DeltaTime, 0.f);
	if (ElapsedSeconds >= SimCopterGort::EndSeconds) { Finish(); return; }
	const int32 NextCue = SimCopterGort::FindCue(ElapsedSeconds);
	if (NextCue == CurrentCue) return;
	CurrentCue = NextCue;
	StopVoice();
	if (CurrentCue != INDEX_NONE && Audio.IsValid())
	{
		const auto& Cue = SimCopterGort::GetCues()[CurrentCue];
		Voice = Audio->PlayGortVoice(Cue.Voice, Cue.Speaker);
	}
}

FText SSimCopterGortSequence::GetSubtitle(int32 Speaker) const
{
	if (!bClosed && CurrentCue != INDEX_NONE)
	{
		const auto& Cue = SimCopterGort::GetCues()[CurrentCue];
		if (Cue.Speaker == Speaker) return FText::FromString(Cue.Subtitle);
	}
	return FText::GetEmpty();
}

FReply SSimCopterGortSequence::OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		Finish();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}
