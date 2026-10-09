#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"

DECLARE_DELEGATE_RetVal_OneParam(FString, FOnSimCopterCheatEntered, const FString&);
class USimCopterHangarArt;

class SSimCopterCheatDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimCopterCheatDialog) : _Art(nullptr) {}
		SLATE_ARGUMENT(USimCopterHangarArt*, Art)
		SLATE_EVENT(FOnSimCopterCheatEntered, OnEntered)
		SLATE_EVENT(FSimpleDelegate, OnClosed)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	TSharedPtr<SWidget> GetInitialFocusWidget() { return AsShared(); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply OnKeyChar(const FGeometry& Geometry, const FCharacterEvent& Event) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
	FString EntryText;
	bool bDismissed = false;
	double OpenedAt = 0;
	TArray<TSharedRef<FButtonStyle>> ButtonStyles;
	FOnSimCopterCheatEntered OnEntered;
	FSimpleDelegate OnClosed;
	void Submit();
	void Cancel();
};
