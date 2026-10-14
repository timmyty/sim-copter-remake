// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSimCopterSettingsMenu.h"

#include "InputCoreTypes.h"
#include "UI/SimCopterHangarArt.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SimCopterSettingsMenu"

using namespace SimCopterFrontEnd;
using namespace SimCopterSettingsMenuLayout;

namespace
{
const TCHAR* const SettingsPage = TEXT("PLAYMENU.BMP");

// The page's own selection tick, the same standalone sound object the main menu builds.
const TCHAR* const SelectionSound = TEXT("menu");
}

ESimCopterSettingsItem SSimCopterSettingsMenu::GetItemForRow(const int32 Row, const bool bHasCitySettings)
{
	// Remake menu order: resume is always first; preserve the original command IDs.
	if (Row == -1) return ESimCopterSettingsItem::CheatCodes;
	if (Row <= 0) return ESimCopterSettingsItem::Continue;
	if (Row == 1) return ESimCopterSettingsItem::PhotoMode;
	return static_cast<ESimCopterSettingsItem>(
		FMath::Clamp(Row - (bHasCitySettings ? 2 : 1), 0, FullItemCount - 2));
}

const FText& SSimCopterSettingsMenu::GetItemLabel(const ESimCopterSettingsItem Item)
{
	static const FText PhotoLabel = LOCTEXT("PhotoMode", "Photo Mode");
	if (Item == ESimCopterSettingsItem::PhotoMode) return PhotoLabel;
	static const FText CheatLabel = LOCTEXT("CheatCodes", "Cheat Codes");
	if (Item == ESimCopterSettingsItem::CheatCodes) return CheatLabel;
	// STRINGTABLE 60..67, English.
	static const FText Labels[FullItemCount] = {
		LOCTEXT("CitySettings", "City Settings"), // 60
		LOCTEXT("Options", "Options"),            // remake label for original STRINGTABLE 61
		LOCTEXT("Sound", "Sound"),                // 62
		LOCTEXT("Controls", "Controls"),          // 63
		LOCTEXT("SaveGame", "Save Game"),         // 64
		LOCTEXT("SaveGameAs", "Save Game As"),    // 65
		LOCTEXT("LeaveCity", "Leave City"),       // 66
		LOCTEXT("Continue", "Continue"),          // 67
	};

	static const FText Empty = FText::GetEmpty();
	const int32 Index = static_cast<int32>(Item);
	return (Index >= 0 && Index < FullItemCount) ? Labels[Index] : Empty;
}

void SSimCopterSettingsMenu::Construct(const FArguments& InArgs)
{
	Art = InArgs._Art;
	OnItemChosen = InArgs._OnItemChosen;
	bHasCitySettings = InArgs._AllowCitySettings;
	VisibleCount = GetVisibleCount(bHasCitySettings);

	USimCopterHangarArt* ArtObject = Art;
	TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);

	AddAt(Canvas, FRect{ PageX, PageY, PageX + PageWidth, PageY + PageHeight },
		MakePageImage(ArtObject, SettingsPage));

	// Reuse the original plate, lever and background pixels, preserving the surrounding
	// panel and its first/last end caps. No differently styled buttons are introduced.
	if (const FSlateBrush* Original = ArtObject ? ArtObject->GetBitmap(SettingsPage, true) : nullptr)
	{
		const int32 Count = VisibleCount + 1;
		const float Stride = GetItemStride(bHasCitySettings);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			// Keep the artwork's top-to-bottom lighting gradient; wrapping to an earlier
			// plate would leave a bright horizontal seam near the bottom of the panel.
			const int32 SourceRow = FMath::RoundToInt(Index * 7.0f / (Count - 1));
			const float Top = PageY + 60.0f + Index * Stride;
			// Resize only the lever and recessed plates. The broad metal background remains
			// one uninterrupted image, so repeated rows cannot leave seams across the panel.
			for (const FVector2f Band : {FVector2f(52, 71), FVector2f(88, 304)})
			{
				auto Brush = MakeShared<FSlateBrush>(*Original);
				Brush->SetUVRegion(FBox2f(FVector2f(Band.X / PageWidth, (60.0f + SourceRow * 40.0f) / PageHeight),
					FVector2f(Band.Y / PageWidth, (100.0f + SourceRow * 40.0f) / PageHeight)));
				PlateBrushes.Add(Brush);
				AddAt(Canvas, FRect{PageX + Band.X, Top, PageX + Band.Y, Top + Stride}, SNew(SImage).Image(&Brush.Get()));
			}
		}
	}

	// Labels first, hit rows after, so the transparent rows sit on top and get the pointer.
	// Append the hidden plate after the normal controls so Continue keeps initial focus.
	const int32 ControlCount = VisibleCount + 1;
	for (int32 Index = 0; Index < ControlCount; ++Index)
	{
		const int32 Row = Index < VisibleCount ? Index : -1;
		const FRect Text = GetRowTextRect(Row, bHasCitySettings);
		AddAt(
			Canvas,
			FRect{ PageX + Text.Left, PageY + Text.Top, PageX + Text.Right, PageY + Text.Bottom },
			SNew(STextBlock)
			.Text(GetItemLabel(GetItemForRow(Row, bHasCitySettings)))
			.Visibility_Lambda([this, Row]()
			{
				return Row != -1 || SelectedRow == Row ? EVisibility::HitTestInvisible : EVisibility::Hidden;
			})
			.Font(PageFont(ItemFontHeight, /*bBold=*/true))
			.ColorAndOpacity_Lambda([this, Row]()
			{
				return FSlateColor(SelectedRow == Row ? ItemSelectedColor : ItemColor);
			}));
	}

	for (int32 Index = 0; Index < ControlCount; ++Index)
	{
		const int32 Row = Index < VisibleCount ? Index : -1;
		const FRect Hit = GetRowHitRect(Row, bHasCitySettings);
		AddAt(
			Canvas,
			FRect{ PageX + Hit.Left, PageY + Hit.Top, PageX + Hit.Right, PageY + Hit.Bottom },
			MakeInvisibleHitButton(
				FOnClicked::CreateLambda([this, Row]()
				{
					SetSelectedRow(Row);
					ActivateSelected();
					return FReply::Handled();
				}),
				FSimpleDelegate::CreateLambda([this, Row]() { SetSelectedRow(Row); }),
				ButtonStyles, MakeFocusOutline(Hit, GetRowFocusRect(Row, bHasCitySettings), 8.0f)));
	}

	ChildSlot
	[
		MakeScaledScreen(Canvas)
	];
}

void SSimCopterSettingsMenu::SetSelectedRow(const int32 Row)
{
	if (Row < -1 || Row >= VisibleCount || Row == SelectedRow)
	{
		return;
	}

	SelectedRow = Row;
	PlayScreenSound(SelectionSound);
}

void SSimCopterSettingsMenu::ActivateSelected()
{
	OnItemChosen.ExecuteIfBound(GetItemForRow(SelectedRow, bHasCitySettings));
}

bool SSimCopterSettingsMenu::SelectByMnemonic(const TCHAR Character)
{
	// FUN_0045eed0 stops at the first item whose text starts with the key, so on S "Save Game"
	// wins over "Save Game As" - and over "Sound", which is above both.
	for (int32 Row = 0; Row < VisibleCount; ++Row)
	{
		const FString Label = GetItemLabel(GetItemForRow(Row, bHasCitySettings)).ToString();
		if (Label.Len() > 0 && FChar::ToUpper(Label[0]) == FChar::ToUpper(Character))
		{
			SetSelectedRow(Row);
			return true;
		}
	}

	return false;
}

FReply SSimCopterSettingsMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (const ENavigation Navigation = GetNavigationForKey(Key); Navigation != ENavigation::None)
	{
		const int32 Offset = 1;
		const int32 Target = GetNavigationTarget(Navigation, SelectedRow + Offset, VisibleCount + Offset);
		if (Target != INDEX_NONE)
		{
			SetSelectedRow(Target - Offset);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Enter)
	{
		ActivateSelected();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape)
	{
		// FUN_0044c9e0's 0x3ea branch resumes the sim and closes the page - byte for byte what
		// item 7 does, so Escape here is Continue, not a cancel.
		OnItemChosen.ExecuteIfBound(ESimCopterSettingsItem::Continue);
		return FReply::Handled();
	}

	const FString KeyName = Key.GetFName().ToString();
	if (KeyName.Len() == 1 && SelectByMnemonic(KeyName[0]))
	{
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
