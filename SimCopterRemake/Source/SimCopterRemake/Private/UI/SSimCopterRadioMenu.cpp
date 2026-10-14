#include "SSimCopterRadioMenu.h"
#include "SimCopterFrontEndPage.h"
#include "Game/SimCopterSettings.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SimCopterRadioMenu"
using namespace SimCopterFrontEnd;

namespace
{
const FLinearColor Lettering(0.83f, 0.82f, 0.76f);
const FLinearColor Amber(0.93f, 0.62f, 0.20f);
const FLinearColor Display(0.009f, 0.011f, 0.011f);

const TCHAR* Kind(ESimCopterRadioSlot Slot)
{
	switch (Slot)
	{
	case ESimCopterRadioSlot::Commercial: return TEXT("COMMERCIAL");
	case ESimCopterRadioSlot::Dj: return TEXT("DJ");
	case ESimCopterRadioSlot::Jingle: return TEXT("JINGLE");
	default: return TEXT("SONG");
	}
}

// Draw the same charcoal metal, raised rims, slotted screws and warning-yellow details as
// DASH6-plus-DASH4. Code-native decoration stays sharp at any HUD scale, with no new assets.
class SRadioFaceplate : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRadioFaceplate) {} SLATE_DEFAULT_SLOT(FArguments, Content) SLATE_END_ARGS()
	void Construct(const FArguments& Args) { ChildSlot.Padding(22)[Args._Content.Widget]; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Clip,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override
	{
		const FVector2f Size(Geometry.GetLocalSize());
		auto Box = [&](float X, float Y, float W, float H, FLinearColor Color, const FSlateBrush* Brush)
		{
			FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(W, H),
				FSlateLayoutTransform(FVector2f(X, Y))), Brush, ESlateDrawEffect::None, Color);
		};
		const auto* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
		Box(0, 0, Size.X, Size.Y, FLinearColor(0.045f, 0.046f, 0.049f), White);
		Box(2, 2, Size.X - 4, Size.Y - 4, FLinearColor(0.35f, 0.36f, 0.37f), White);
		Box(4, 4, Size.X - 8, Size.Y - 8, FLinearColor(0.06f, 0.064f, 0.07f), White);
		Box(7, 7, Size.X - 14, Size.Y - 14, FLinearColor(0.16f, 0.165f, 0.17f), White);
		for (int32 I = 10; I < Size.Y - 8; I += 3)
			Box(8, I, Size.X - 16, 1, FLinearColor(0.17f, 0.176f, 0.18f, 0.32f), White);
		for (float X : { 12.0f, Size.X - 12.0f })
			for (float Y : { 12.0f, Size.Y - 12.0f })
			{
				Box(X - 5, Y - 5, 10, 10, FLinearColor(0.015f, 0.017f, 0.018f), &Screw);
				Box(X - 3, Y - 3, 6, 6, FLinearColor(0.34f, 0.35f, 0.36f), &Screw);
				Box(X - 2, Y, 4, 1, FLinearColor::Black, White);
			}
		return SCompoundWidget::OnPaint(Args, Geometry, Clip, Elements, Layer + 1, Style, bEnabled);
	}
private:
	FSlateRoundedBoxBrush Screw{ FLinearColor::White, 5.0f };
};

TSharedRef<SWidget> Inset(TSharedRef<SWidget> Content)
{
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.035f, 0.038f, 0.04f)).Padding(3)
		[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(Display).Padding(6)[Content]];
}
}

void SSimCopterRadioMenu::Construct(const FArguments& Args)
{
	Radio = Args._Radio;
	OnClosed = Args._OnClosed;
	ListStyle = FCoreStyle::Get().GetWidgetStyle<FTableViewStyle>(TEXT("ListView"));
	ListStyle.SetBackgroundBrush(FSlateColorBrush(Display));
	RowStyle = FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>(TEXT("TableView.Row"));
	RowStyle.SetEvenRowBackgroundBrush(FSlateNoResource());
	RowStyle.SetOddRowBackgroundBrush(FSlateNoResource());
	const FSlateColorBrush Selected(FLinearColor(0.28f, 0.20f, 0.07f));
	RowStyle.SetActiveBrush(Selected).SetActiveHoveredBrush(Selected)
		.SetInactiveBrush(Selected).SetInactiveHoveredBrush(Selected)
		.SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FLinearColor(0.11f, 0.12f, 0.12f)))
		.SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FLinearColor(0.11f, 0.12f, 0.12f)))
		.SetTextColor(Lettering).SetSelectedTextColor(Amber);
	ButtonStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("Button"));
	ButtonStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.19f, 0.195f, 0.18f), 2.0f,
		FLinearColor(0.39f, 0.40f, 0.36f), 1.0f));
	ButtonStyle.SetHovered(FSlateRoundedBoxBrush(FLinearColor(0.27f, 0.25f, 0.16f), 2.0f, Amber, 1.0f));
	ButtonStyle.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.08f, 0.085f, 0.075f), 2.0f, Amber, 1.0f));
	TransportSliderStyle=FCoreStyle::Get().GetWidgetStyle<FSliderStyle>(TEXT("Slider"));
	TransportSliderStyle.SetNormalBarImage(FSlateColorBrush(FLinearColor::White))
		.SetHoveredBarImage(FSlateColorBrush(FLinearColor::White)).SetBarThickness(3.f);
	if (const auto* RadioPtr = Radio.Get())
		for (int32 I = 0; I < RadioPtr->GetStationCount(); ++I) Stations.Add(MakeShared<int32>(I));

	SAssignNew(StationList, SListView<TSharedPtr<int32>>)
		.ListItemsSource(&Stations).ListViewStyle(&ListStyle).SelectionMode(ESelectionMode::Single)
		.OnGenerateRow_Lambda([this](TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& Owner)
		{
			const auto* RadioPtr = Radio.Get();
			const FString Sign = RadioPtr && RadioPtr->GetStations().IsValidIndex(*Item)
				? RadioPtr->GetStations()[*Item].CallSign.ToUpper() : FString();
			return SNew(STableRow<TSharedPtr<int32>>, Owner).Style(&RowStyle).Padding(FMargin(8, 5))
			[SNew(STextBlock).Text(FText::FromString(Sign)).Font(PageFont(18, true))];
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<int32> Item, ESelectInfo::Type)
		{ if (Item) SelectStation(*Item); });
	SAssignNew(TrackList, SListView<TSharedPtr<FSimCopterRadioTrack>>)
		.ListItemsSource(&Tracks).ListViewStyle(&ListStyle).SelectionMode(ESelectionMode::Single)
		.OnGenerateRow_Lambda([this](TSharedPtr<FSimCopterRadioTrack> Item, const TSharedRef<STableViewBase>& Owner)
		{
			const FString Label = FString::Printf(TEXT("%02d  %s"), Tracks.IndexOfByKey(Item) + 1, *Item->Title);
			return SNew(STableRow<TSharedPtr<FSimCopterRadioTrack>>, Owner).Style(&RowStyle).Padding(FMargin(7, 5))
			.ToolTipText(FText::FromString(Item->Title))
			[SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Label))
				.Font(PageFont(18)).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
				[SNew(STextBlock).Text(FText::FromString(Kind(Item->Slot))).Font(PageFont(11)).ColorAndOpacity(Amber)]];
		})
		.OnMouseButtonDoubleClick_Lambda([this](TSharedPtr<FSimCopterRadioTrack>) { PlaySelection(); });

	auto Button = [this](FText Text, FOnClicked Action)
	{
		return SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(12, 8)).OnClicked(Action)
			[SNew(STextBlock).Text(Text).Font(PageFont(16, true)).ColorAndOpacity(Lettering)];
	};
	auto PlayButton = Button(LOCTEXT("Play", "PLAY TRACK"), FOnClicked::CreateSP(this, &SSimCopterRadioMenu::PlaySelection));
	PlayButton->SetEnabled(TAttribute<bool>::CreateLambda([this] { return TrackList->GetNumItemsSelected() > 0; }));
	auto TuneButton = Button(LOCTEXT("Tune", "TUNE STATION"), FOnClicked::CreateSP(this, &SSimCopterRadioMenu::TuneStation));
	TuneButton->SetEnabled(TAttribute<bool>::CreateLambda([this] { return Tracks.Num() > 0; }));
	auto TransportButton = [this](const TCHAR* Text, const TCHAR* Tip, FOnClicked Action)
	{
		return SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(8, 6)).OnClicked(Action)
			.ToolTipText(FText::FromString(Tip)).IsEnabled_Lambda([this] { return Radio.IsValid() && Radio->GetStationCount() > 0; })
			[SNew(STextBlock).Text(FText::FromString(Text)).Font(PageFont(13, true)).ColorAndOpacity(Lettering)];
	};
	auto PlaybackControls = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 7, 0, 3)
		[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[SNew(SSlider).Tag(TEXT("RadioSeekBar")).Style(&TransportSliderStyle).SliderBarColor(FLinearColor(.35f,.28f,.13f)).SliderHandleColor(Amber)
				.RequiresControllerLock(false).StepSize(.05f)
				.IsEnabled_Lambda([this] { return Radio.IsValid() && Radio->IsPowered() && Radio->GetPlaybackDuration() > 0; })
				.Value_Lambda([this] { return bSeeking ? SeekFraction : Radio.IsValid() && Radio->GetPlaybackDuration() > 0 ? Radio->GetPlaybackPosition()/Radio->GetPlaybackDuration() : 0.f; })
				.OnMouseCaptureBegin_Lambda([this] { SeekFraction=Radio.IsValid() && Radio->GetPlaybackDuration()>0 ? Radio->GetPlaybackPosition()/Radio->GetPlaybackDuration() : 0; bSeeking=true; })
				.OnValueChanged_Lambda([this](float Value) { SeekFraction=Value; if (!bSeeking) SeekToFraction(Value); })
				.OnMouseCaptureEnd_Lambda([this] { bSeeking=false; SeekToFraction(SeekFraction); })]
			+ SHorizontalBox::Slot().AutoWidth().Padding(10, 0, 0, 0).VAlign(VAlign_Center)
			[SNew(SBox).WidthOverride(100)
				[SNew(STextBlock).Font(PageFont(13)).ColorAndOpacity(Amber).Justification(ETextJustify::Right)
				.Text_Lambda([this]
				{
					const int32 Duration=Radio.IsValid()?FMath::FloorToInt(Radio->GetPlaybackDuration()):0;
					const int32 Position=Radio.IsValid()?FMath::FloorToInt(bSeeking?SeekFraction*Duration:Radio->GetPlaybackPosition()):0;
					return FText::FromString(FString::Printf(TEXT("%02d:%02d / %02d:%02d"),Position/60,Position%60,Duration/60,Duration%60));
				})]]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
		[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[TransportButton(TEXT("|<"),TEXT("Previous track (LB)"),FOnClicked::CreateSP(this,&SSimCopterRadioMenu::StepTrack,-1))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(5,0)[TransportButton(TEXT("-10s"),TEXT("Rewind ten seconds (LT)"),FOnClicked::CreateSP(this,&SSimCopterRadioMenu::SkipSeconds,-10.f))]
			+ SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&ButtonStyle).ContentPadding(FMargin(12,6))
				.OnClicked(this,&SSimCopterRadioMenu::TogglePlayback).ToolTipText(LOCTEXT("PauseTip","Play / pause (Space / Y)"))
				[SNew(STextBlock).Font(PageFont(13,true)).ColorAndOpacity(Amber)
				.Text_Lambda([this] { return Radio.IsValid() && Radio->IsPowered() && !Radio->GetCurrentTitle().IsEmpty() && !Radio->IsPlaybackPaused()?LOCTEXT("Pause","PAUSE"):LOCTEXT("Resume","PLAY"); })]]
			+ SHorizontalBox::Slot().AutoWidth().Padding(5,0)[TransportButton(TEXT("+10s"),TEXT("Forward ten seconds (RT)"),FOnClicked::CreateSP(this,&SSimCopterRadioMenu::SkipSeconds,10.f))]
			+ SHorizontalBox::Slot().AutoWidth()[TransportButton(TEXT(">|"),TEXT("Next track (RB)"),FOnClicked::CreateSP(this,&SSimCopterRadioMenu::StepTrack,1))]
			+ SHorizontalBox::Slot().AutoWidth().Padding(15,0,5,0).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("Volume","VOL")).Font(PageFont(12,true)).ColorAndOpacity(Lettering)]
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[SNew(SSlider).Tag(TEXT("RadioVolume")).Style(&TransportSliderStyle).SliderBarColor(FLinearColor(.35f,.28f,.13f)).SliderHandleColor(Amber).ToolTipText(LOCTEXT("VolumeTip","Radio volume"))
				.RequiresControllerLock(false).StepSize(.05f)
				.Value_Lambda([this] { return Radio.IsValid()?Radio->GetVolume():0.f; })
				.OnValueChanged_Lambda([this](float Value)
				{
					if (auto* R=Radio.Get())
					{
						R->SetVolume(Value);
						if (auto* Settings=USimCopterSettings::Get(R)) Settings->SetRadioVolume(FMath::RoundToInt(Value*USimCopterSettings::VolumeMax));
						bVolumeChanged=true;
					}
				})]];
	auto Canvas = SNew(SConstraintCanvas);
	AddAt(Canvas, FRect{20, 16, 620, 464}, SNew(SRadioFaceplate)
	[SNew(SVerticalBox)
	+ SVerticalBox::Slot().AutoHeight()
	[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1)
		[SNew(STextBlock).Text(LOCTEXT("Title", "RADIO / STATION SELECT")).Font(PageFont(24, true)).ColorAndOpacity(Lettering)]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[SNew(STextBlock).Text(LOCTEXT("Sequential", "SEQUENTIAL")).Font(PageFont(13, true)).ColorAndOpacity(Amber)]]
	+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 10)
	[Inset(SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]
		{
			const auto* R = Radio.Get();
			if (!R || R->GetStationCount() == 0) return LOCTEXT("Unavailable", "NO STATIONS AVAILABLE");
			const FString Title = R->IsPowered() ? (R->GetCurrentTitle().IsEmpty() ? TEXT("READY") : R->GetCurrentTitle()) : TEXT("POWER OFF");
			return FText::FromString(FString::Printf(TEXT("%s: %s  /  %s"), R->IsPlaybackPaused()?TEXT("PAUSED"):TEXT("ON AIR"), *R->GetStationCallSign().ToUpper(), *Title));
		}).Font(PageFont(17)).ColorAndOpacity(Amber).OverflowPolicy(ETextOverflowPolicy::Ellipsis)]
		+SVerticalBox::Slot().AutoHeight()[PlaybackControls])]
	+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 5)
	[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(132)
			[SNew(STextBlock).Text(LOCTEXT("Stations", "STATIONS")).Font(PageFont(13, true)).ColorAndOpacity(Lettering)]]
		+ SHorizontalBox::Slot().FillWidth(1)
			[SNew(STextBlock).Text_Lambda([this]
			{
				const auto* R = Radio.Get();
				const FString Sign = R && R->GetStations().IsValidIndex(SelectedStation) ? R->GetStations()[SelectedStation].CallSign.ToUpper() : TEXT("PLAYLIST");
				return FText::FromString(FString::Printf(TEXT("%s / %d TRACKS / PLAYS IN ORDER"), *Sign, Tracks.Num()));
			}).Font(PageFont(13, true)).ColorAndOpacity(Lettering)]]
	+ SVerticalBox::Slot().FillHeight(1)
	[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
			[SNew(SBox).WidthOverride(122)[Inset(StationList.ToSharedRef())]]
		+ SHorizontalBox::Slot().FillWidth(1)
		[Inset(SNew(SOverlay)
			+ SOverlay::Slot()[TrackList.ToSharedRef()]
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[SNew(STextBlock).Text(LOCTEXT("Empty", "No playable tracks"))
				.Visibility_Lambda([this] { return Tracks.IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.ColorAndOpacity(Lettering)])]]
	+ SVerticalBox::Slot().AutoHeight().Padding(0, 8)
	[SNew(STextBlock).Text_Lambda([this]
		{ return FText::FromString(Status.IsEmpty() ? TEXT("Select a track to start at the beginning. Playlist loops at the end.") : Status); })
		.Font(PageFont(13)).ColorAndOpacity(Amber).WrapTextAt(550)]
	+ SVerticalBox::Slot().AutoHeight()
	[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[PlayButton]
		+ SHorizontalBox::Slot().AutoWidth().Padding(8, 0)
		[TuneButton]
		+ SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right)
		[Button(LOCTEXT("Close", "CLOSE"), FOnClicked::CreateSP(this,&SSimCopterRadioMenu::Close))]]
	+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
	[SNew(STextBlock).Text(LOCTEXT("Help", "D-pad / stick: move   A: select / edit / play   B: back / close\nY: pause   LB / RB: track   LT / RT: seek   View / Esc / Ctrl+R: close"))
		.Font(PageFont(11)).ColorAndOpacity(Lettering)]]);
	ChildSlot[MakeScaledScreen(Canvas)];
	if (Stations.Num() > 0)
		StationList->SetSelection(Stations[FMath::Clamp(Radio->GetStationIndex(), 0, Stations.Num() - 1)]);
	SetControllerSelection(StationList);
}

void SSimCopterRadioMenu::SelectStation(int32 Index)
{
	SelectedStation = Index;
	if (Stations.IsValidIndex(Index)) StationList->RequestScrollIntoView(Stations[Index]);
	Tracks.Reset();
	Status.Reset();
	TrackList->ClearSelection();
	if (const auto* R = Radio.Get())
	{
		for (const auto& Track : R->GetStationPlaylist(Index)) Tracks.Add(MakeShared<FSimCopterRadioTrack>(Track));
		if (R->GetVolume() <= 0.0f) Status = TEXT("Radio volume is muted. Raise the cockpit volume to hear playback.");
	}
	TrackList->RequestListRefresh();
	if (Tracks.Num() > 0)
	{
		int32 Current = 0;
		if (const auto* R = Radio.Get(); R && Index == R->GetStationIndex())
		{
			const int32 Match = Tracks.IndexOfByPredicate([R](const auto& Track)
			{ return Track->Slot == R->GetCurrentSlot() && Track->Title == R->GetCurrentTitle(); });
			if (Match != INDEX_NONE) Current = Match;
		}
		TrackList->SetSelection(Tracks[Current]);
		TrackList->RequestScrollIntoView(Tracks[Current]);
	}
}

FReply SSimCopterRadioMenu::PlaySelection()
{
	const auto Selection = TrackList->GetSelectedItems();
	if (auto* R = Radio.Get(); R && Selection.Num() == 1)
	{
		if (R->PlayStationTrack(SelectedStation, Tracks.IndexOfByKey(Selection[0]))) Status.Reset();
		else Status = TEXT("Unable to play this track. Check the radio media files and try another item.");
	}
	return FReply::Handled();
}

FReply SSimCopterRadioMenu::TuneStation()
{
	// Tuning starts at the first displayed item; choosing a row gives an exact starting point.
	if (Tracks.Num() > 0) { TrackList->SetSelection(Tracks[0]); return PlaySelection(); }
	return FReply::Handled();
}

FReply SSimCopterRadioMenu::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FReply Navigation=SSimCopterNavigableMenu::OnPreviewKeyDown(Geometry,Event);
	if (Navigation.IsEventHandled()) return Navigation;
	const auto Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Left || (Key == EKeys::R && Event.IsControlDown()))
	{
		if (!Event.IsRepeat()) return Close();
		return FReply::Handled();
	}
	// Leave Enter to a focused action button (especially Close). In either list it plays
	// the selected row, matching a track double-click.
	if (Key == EKeys::Enter && (HasKeyboardFocus() || TrackList->HasKeyboardFocus() ||
		TrackList->HasFocusedDescendants() || StationList->HasKeyboardFocus() || StationList->HasFocusedDescendants()))
	{
		if (!Event.IsRepeat()) return PlaySelection();
		return FReply::Handled();
	}
	if (Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Top)
	{ if (!Event.IsRepeat()) TogglePlayback(); return FReply::Handled(); }
	if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder)
	{ if (!Event.IsRepeat()) StepTrack(Key == EKeys::Gamepad_LeftShoulder ? -1 : 1); return FReply::Handled(); }
	if (Key == EKeys::Gamepad_LeftTrigger || Key == EKeys::Gamepad_RightTrigger)
	{ if (!Event.IsRepeat()) SkipSeconds(Key == EKeys::Gamepad_LeftTrigger ? -10.f : 10.f); return FReply::Handled(); }
	return FReply::Unhandled();
}

FReply SSimCopterRadioMenu::OnKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	// The shared controller host sends Enter for an active list and Escape after backing out.
	if (Event.GetKey()==EKeys::Escape) return Event.IsRepeat()?FReply::Handled():Close();
	if (Event.GetKey()==EKeys::Enter) return Event.IsRepeat()?FReply::Handled():PlaySelection();
	return FReply::Unhandled();
}

FReply SSimCopterRadioMenu::TogglePlayback()
{
	if (auto* R=Radio.Get()) R->TogglePlaybackPaused();
	return FReply::Handled();
}

FReply SSimCopterRadioMenu::Close()
{
	if (bVolumeChanged && Radio.IsValid())
		if (auto* Settings=USimCopterSettings::Get(Radio.Get())) Settings->Save();
	bVolumeChanged=false;
	OnClosed.ExecuteIfBound();
	return FReply::Handled();
}

FReply SSimCopterRadioMenu::StepTrack(int32 Delta)
{
	if (auto* R=Radio.Get(); R && R->StepTrack(Delta))
	{
		if (SelectedStation==R->GetStationIndex()) SelectStation(SelectedStation);
		else StationList->SetSelection(Stations[R->GetStationIndex()]);
	}
	return FReply::Handled();
}

FReply SSimCopterRadioMenu::SkipSeconds(float Delta)
{
	if (auto* R=Radio.Get()) R->SeekPlayback(R->GetPlaybackPosition()+Delta);
	return FReply::Handled();
}

void SSimCopterRadioMenu::SeekToFraction(float Fraction)
{
	if (auto* R=Radio.Get()) R->SeekPlayback(FMath::Clamp(Fraction,0.f,1.f)*R->GetPlaybackDuration());
}

#undef LOCTEXT_NAMESPACE
