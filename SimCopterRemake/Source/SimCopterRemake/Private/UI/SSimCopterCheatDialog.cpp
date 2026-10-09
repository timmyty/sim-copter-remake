#include "UI/SSimCopterCheatDialog.h"
#include "InputCoreTypes.h"
#include "UI/SSimCopterMessageBox.h"
#include "UI/SimCopterHangarArt.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

void SSimCopterCheatDialog::Construct(const FArguments& Args)
{
	OnEntered = Args._OnEntered;
	OnClosed = Args._OnClosed;
	OpenedAt = FPlatformTime::Seconds();
	// SCHOOK: OpenCheatDialog 0x004354c0 / TextEntryDialog 0x004426c0.
	// This is the original 465x353 bitmap page, not an operating-system edit box.
	using namespace SimCopterFrontEnd;
	using namespace SimCopterMessageBoxLayout;
	USimCopterHangarArt* Art = Args._Art;
	const float X = FMath::RoundToFloat((ScreenWidth - PageWidth) * .5f);
	const float Y = FMath::RoundToFloat((ScreenHeight - PageHeight) * .5f);
	const bool bPrinted = HasPageBitmap(Art, TEXT("MBox.bmp"));
	const FLinearColor Ink = bPrinted ? FLinearColor(FColor(0x10, 0x15, 0x1f)) : PlateTextColor;
	auto Canvas = SNew(SConstraintCanvas);
	AddAt(Canvas, {X, Y, X + PageWidth, Y + PageHeight}, MakePageImage(Art, TEXT("MBox.bmp")));
	// STRINGTABLE 35; the title is above the editable rectangle (FUN_0043d850).
	AddAt(Canvas, {X + 100, Y + 78, X + 368, Y + 100},
		SNew(STextBlock).Text(FText::FromString(TEXT("Enter your cheat code here:")))
		.Font(PageFont(TextFontHeight)).ColorAndOpacity(Ink));
	const FRect Well{X + TextRect.Left, Y + TextRect.Top, X + TextRect.Right, Y + TextRect.Bottom};
	if (Art && Art->GetBitmap(TEXT("MBoxCht.bmp")))
		AddAt(Canvas, Well, MakePageImage(Art, TEXT("MBoxCht.bmp")));
	AddAt(Canvas, Well, SNew(STextBlock)
		.Text_Lambda([this]
		{
			// FUN_00441a70 / 00441e40: trailing underscore, toggled every 700 ms.
			const bool bCaret = (int64((FPlatformTime::Seconds() - OpenedAt) / .7) & 1) == 0;
			return FText::FromString(EntryText + (bCaret ? TEXT("_") : TEXT(" ")));
		})
		.Font(PageFont(TextFontHeight)).ColorAndOpacity(Ink).AutoWrapText(true)
		.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
		.Clipping(EWidgetClipping::ClipToBounds));
	if (Art && Art->GetBitmap(TEXT("MBoxl.bmp")))
	{
		// FUN_0043d0c0 / 0043dd90: nine 149x93 lever frames, 55 ms ping-pong.
		TArray<const FSlateBrush*> Frames;
		for (int32 Frame = 0; Frame < 9; ++Frame)
			Frames.Add(Art->GetStripFrame(TEXT("MBoxl.bmp"), Frame, 9));
		AddAt(Canvas, {X + 52, Y + 211, X + 201, Y + 304}, SNew(SImage)
			.Image_Lambda([this, Frames]
			{
				const int32 Step = int64((FPlatformTime::Seconds() - OpenedAt) / .055) % 18;
				return Frames[Step < 9 ? Step : 17 - Step];
			}));
	}
	const auto AddButton = [&](float ButtonX, const TCHAR* Label, bool bAccept)
	{
		auto Button = MakeButton(Art, FText::FromString(Label), ButtonFontHeight,
			FOnClicked::CreateLambda([this, bAccept]
			{ if (bAccept) Submit(); else Cancel(); return FReply::Handled(); }), ButtonStyles, false);
		AddAt(Canvas, {X + ButtonX, Y + ButtonY, X + ButtonX + ButtonWidth, Y + ButtonY + ButtonHeight}, Button);
	};
	// Flags 0x10001: STRINGTABLE 20 (OK) and 21 (Cancel).
	AddButton(LeftButtonX, TEXT("OK"), true);
	AddButton(RightButtonX, TEXT("Cancel"), false);
	ChildSlot[MakeScaledScreen(Canvas)];
}

// SCHOOK: CheatDialogResult 0x0044bf70
void SSimCopterCheatDialog::Submit()
{
	if (bDismissed) return;
	const TSharedRef<SWidget> KeepAlive = AsShared();
	bDismissed = true;
	// Retail dismisses the entry page before executing the command.
	OnClosed.ExecuteIfBound();
	if (OnEntered.IsBound()) OnEntered.Execute(EntryText);
}

void SSimCopterCheatDialog::Cancel()
{
	if (bDismissed) return;
	const TSharedRef<SWidget> KeepAlive = AsShared();
	bDismissed = true;
	OnClosed.ExecuteIfBound();
}

FReply SSimCopterCheatDialog::OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		Cancel();
		return FReply::Handled();
	}
	if (bDismissed) return FReply::Unhandled();
	if (Event.GetKey() == EKeys::Enter)
	{
		Submit();
		return FReply::Handled();
	}
	if (Event.GetKey() == EKeys::BackSpace)
	{
		// FUN_00442230: editing removes from the end, before the underscore.
		if (!EntryText.IsEmpty()) EntryText.LeftChopInline(1);
		return FReply::Handled();
	}
	if (Event.IsControlDown() && Event.GetKey() == EKeys::V)
	{
		// Remake convenience for video filenames; retain the original capacity.
		FString Clipboard;
		FPlatformApplicationMisc::ClipboardPaste(Clipboard);
		for (TCHAR Character : Clipboard)
			if (Character >= TEXT(' ') && EntryText.Len() < 127) EntryText.AppendChar(Character);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SSimCopterCheatDialog::OnKeyChar(const FGeometry&, const FCharacterEvent& Event)
{
	if (bDismissed) return FReply::Unhandled();
	// FUN_00441f60 / 004428f0: 128 characters INCLUDING the trailing caret.
	if (!Event.IsControlDown() && !Event.IsAltDown() && Event.GetCharacter() >= TEXT(' ') && EntryText.Len() < 127)
		EntryText.AppendChar(Event.GetCharacter());
	return FReply::Handled();
}

FReply SSimCopterCheatDialog::OnMouseButtonDown(const FGeometry&, const FPointerEvent&)
{
	return FReply::Handled().SetUserFocus(AsShared());
}
