#pragma once

#include "Audio/SimCopterRadio.h"
#include "SSimCopterNavigableMenu.h"
#include "Widgets/Views/SListView.h"

/** Remake radio browser, styled as a removable cockpit instrument panel. */
class SSimCopterRadioMenu : public SSimCopterNavigableMenu
{
public:
	SLATE_BEGIN_ARGS(SSimCopterRadioMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<USimCopterRadioSubsystem>, Radio)
		SLATE_EVENT(FSimpleDelegate, OnClosed)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;

private:
	friend class FSimCopterRadioMenuWidgetTest;
	TWeakObjectPtr<USimCopterRadioSubsystem> Radio;
	FSimpleDelegate OnClosed;
	int32 SelectedStation = INDEX_NONE;
	TArray<TSharedPtr<int32>> Stations;
	TArray<TSharedPtr<FSimCopterRadioTrack>> Tracks;
	TSharedPtr<SListView<TSharedPtr<int32>>> StationList;
	TSharedPtr<SListView<TSharedPtr<FSimCopterRadioTrack>>> TrackList;
	FTableViewStyle ListStyle;
	FTableRowStyle RowStyle;
	FButtonStyle ButtonStyle;
	FSliderStyle TransportSliderStyle;
	FString Status;
	bool bSeeking = false;
	bool bVolumeChanged = false;
	float SeekFraction = 0;
	FReply Close();
	FReply TogglePlayback();
	FReply StepTrack(int32 Delta);
	FReply SkipSeconds(float Delta);
	void SeekToFraction(float Fraction);
	void SelectStation(int32 Index);
	FReply PlaySelection();
	FReply TuneStation();
};
