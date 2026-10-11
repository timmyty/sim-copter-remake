#pragma once

#include "SimCopterFrontEndPage.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;

// Remake-only reference page; the existing Ctrl+Alt+X dialog remains the code entry point.
class SSimCopterCheatGuide : public SCompoundWidget
{
public:
	struct FEntry
	{
		const TCHAR* Code;
		const TCHAR* Effect;
	};
	static TConstArrayView<FEntry> GetEntries();

	SLATE_BEGIN_ARGS(SSimCopterCheatGuide) {}
		SLATE_ARGUMENT(TObjectPtr<USimCopterHangarArt>, Art)
		SLATE_EVENT(FSimpleDelegate, OnClosed)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;

private:
	FSimpleDelegate OnClosed;
	TSharedPtr<SScrollBox> Scroll;
	TArray<TSharedRef<FButtonStyle>> ButtonStyles;
};
