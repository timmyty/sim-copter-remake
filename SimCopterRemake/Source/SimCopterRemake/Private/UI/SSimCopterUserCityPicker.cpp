// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSimCopterUserCityPicker.h"
#include "UI/SimCopterFileBrowser.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "HAL/PlatformProcess.h"

#include "Formats/SimCopterOriginalGamePaths.h"
#include "Formats/SimCity2000Reader.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "UI/SimCopterHangarArt.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SimCopterUserCityPicker"

using namespace SimCopterFrontEnd;
using namespace SimCopterUserCityPickerLayout;

namespace
{
const TCHAR* const PickerPage = TEXT("MENU4.BMP");

// The navy title band takes light text; the pale list panel takes dark.
const FLinearColor TitleText(0.90f, 0.93f, 0.98f, 1.0f);
const FLinearColor ListText(0.08f, 0.08f, 0.09f, 1.0f);

}

FString SSimCopterUserCityPicker::FormatDisplayName(
	const FString& FilePath,
	const FString& EmbeddedCityName)
{
	FString SourceName = EmbeddedCityName.TrimStartAndEnd();
	const bool bUsingEmbeddedName = !SourceName.IsEmpty();
	if (SourceName.IsEmpty())
	{
		SourceName = FPaths::GetCleanFilename(FilePath);
	}

	// Some filenames contain repeated extensions, and TOKYO's embedded CNAM is itself
	// "TOKYO.SC2". Neither form should expose SC2 as part of the player-facing city name.
	while (SourceName.EndsWith(TEXT(".sc2"), ESearchCase::IgnoreCase) ||
		SourceName.EndsWith(TEXT(" sc2"), ESearchCase::IgnoreCase))
	{
		SourceName.LeftChopInline(4);
		SourceName.TrimEndInline();
	}

	FString Result;
	Result.Reserve(SourceName.Len());
	bool bCapitalizeNext = true;
	bool bPendingSpace = false;

	for (const TCHAR Character : SourceName)
	{
		if (FChar::IsAlnum(Character))
		{
			if (bPendingSpace && !Result.IsEmpty() && Result[Result.Len() - 1] != TEXT('-'))
			{
				Result.AppendChar(TEXT(' '));
			}
			Result.AppendChar(bCapitalizeNext ? FChar::ToUpper(Character) : FChar::ToLower(Character));
			bCapitalizeNext = false;
			bPendingSpace = false;
		}
		else if (Character == TEXT('-'))
		{
			Result.TrimEndInline();
			if (!Result.IsEmpty() && Result[Result.Len() - 1] != TEXT('-'))
			{
				Result.AppendChar(TEXT('-'));
			}
			bCapitalizeNext = true;
			bPendingSpace = false;
		}
		else if (Character == TEXT('\'') || Character == 0x2019)
		{
			// Apostrophes are part of authored city names (for example, Kathy's Retreat). They do
			// not begin a new title-case word.
			Result.AppendChar(Character);
			bPendingSpace = false;
		}
		else
		{
			// Whitespace and other punctuation separate words without surviving in the label.
			bPendingSpace = !Result.IsEmpty() && Result[Result.Len() - 1] != TEXT('-');
			bCapitalizeNext = true;
		}
	}

	Result.TrimStartAndEndInline();
	while (Result.EndsWith(TEXT("-")))
	{
		Result.LeftChopInline(1);
	}
	if (Result.IsEmpty() && bUsingEmbeddedName)
	{
		return FormatDisplayName(FilePath);
	}
	return Result;
}

FString SSimCopterUserCityPicker::ReadEmbeddedCityName(const FString& FilePath)
{
	FSimCity2000City City;
	FString Error;
	if (FSimCity2000Reader::LoadCityFromFile(FilePath, City, Error) &&
		City.FindFirstChunk(TEXT("CNAM")) != nullptr)
	{
		return City.CityName;
	}
	return FString();
}

void SSimCopterUserCityPicker::Construct(const FArguments& InArgs)
{
	bAllowExtraControls = InArgs._AllowExtraControls;
	OnAccepted = InArgs._OnAccepted;
	OnCancelled = InArgs._OnCancelled;
	OnOpenWebsite = InArgs._OnOpenWebsite;

	for (const FString& Path : InArgs._CityFilePaths)
	{
		TSharedPtr<FSimCopterUserCityEntry> Entry = MakeShared<FSimCopterUserCityEntry>();
		Entry->FilePath = Path;
		Entry->DisplayName = FormatDisplayName(Path, ReadEmbeddedCityName(Path));
		Entries.Add(MoveTemp(Entry));
	}

	USimCopterHangarArt* ArtObject = InArgs._Art;
	TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);

	const float PageX = FMath::RoundToFloat((ScreenWidth - PageWidth) * 0.5f);
	const float PageY = FMath::RoundToFloat((ScreenHeight - PageHeight) * 0.5f);
	const auto AddAtPage = [&Canvas, PageX, PageY](const FRect& Rect, TSharedRef<SWidget> Widget)
	{
		AddAt(Canvas, FRect{ PageX + Rect.Left, PageY + Rect.Top, PageX + Rect.Right, PageY + Rect.Bottom }, Widget);
	};

	AddAt(Canvas, FRect{ PageX, PageY, PageX + PageWidth, PageY + PageHeight },
		MakePageImage(ArtObject, PickerPage));

	AddAtPage(Menu4PickerTitleRect,
		SNew(SBox)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Title", "Open A SimCity File")) // STRINGTABLE 40
			.Justification(ETextJustify::Center)
			.Font(PageFont(Menu4PickerTitleFontHeight, /*bBold=*/true))
			.ColorAndOpacity(FSlateColor(TitleText))
		]);

	if (Entries.Num() > 0)
	{
		// The page prints a pale panel behind the list, so the list and its rows have to be
		// transparent; the highlight is a translucent tint over the same paper.
		ListStyle = MakeShared<FTableViewStyle>();
		ListStyle->SetBackgroundBrush(FSlateNoResource());

		const FSlateColorBrush Highlight(FLinearColor(0.20f, 0.34f, 0.55f, 0.45f));
		RowStyle = MakeShared<FTableRowStyle>();
		RowStyle->SetEvenRowBackgroundBrush(FSlateNoResource());
		RowStyle->SetEvenRowBackgroundHoveredBrush(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.12f)));
		RowStyle->SetOddRowBackgroundBrush(FSlateNoResource());
		RowStyle->SetOddRowBackgroundHoveredBrush(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.12f)));
		RowStyle->SetActiveBrush(Highlight);
		RowStyle->SetActiveHoveredBrush(Highlight);
		RowStyle->SetInactiveBrush(Highlight);
		RowStyle->SetInactiveHoveredBrush(Highlight);
		RowStyle->SetSelectorFocusedBrush(FSlateNoResource());
		RowStyle->SetTextColor(FSlateColor(ListText));
		RowStyle->SetSelectedTextColor(FSlateColor(ListText));

		AddAtPage(ListRect,
			SAssignNew(ListView, SListView<TSharedPtr<FSimCopterUserCityEntry>>)
			.ListViewStyle(ListStyle.Get())
			.ListItemsSource(&Entries)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SSimCopterUserCityPicker::MakeRow)
			.OnMouseButtonDoubleClick_Lambda([this](TSharedPtr<FSimCopterUserCityEntry>) { Accept(); }));

		ListView->SetSelection(Entries[0]);
	}
	else
	{
		// The "no cities" message is the one thing on this page that shows when MENU4.BMP itself is
		// missing - both want the same folder - so it lands on MakePageImage's dark fallback plate,
		// where ListText would be invisible.
		const bool bHasPage = HasPageBitmap(ArtObject, PickerPage);
		AddAtPage(ListRect,
			SNew(STextBlock)
			.Text(FText::Format(
				LOCTEXT("NoCities", "No .sc2 files found.\n\n{0}"),
				SimCopterOriginalGame::GetMissingDataHint()))
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
			.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
			.LineHeightPercentage(1.15f)
			.Font(PageFont(ListFontHeight))
			.ColorAndOpacity(FSlateColor(bHasPage ? ListText : PlateTextColor)));
	}

	if (bAllowExtraControls)
	{
	const bool bHasPage = HasPageBitmap(ArtObject, PickerPage);
	const FLinearColor LinkColor = bHasPage ? FLinearColor(0.04f, 0.18f, 0.42f) : FLinearColor(0.50f, 0.76f, 1.0f);
	AddAtPage(FRect{ 70, 275, 444, 293 }, SNew(STextBlock)
		.Text(LOCTEXT("MoreCities", "Find more cities online (opens browser)"))
		.Font(PageFont(14, true)).ColorAndOpacity(bHasPage ? ListText : PlateTextColor));
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FString Url = GetCityWebsiteUrl(Index);
		TSharedRef<SWidget> Link = SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0)
			.HAlign(HAlign_Left).VAlign(VAlign_Center)
			.ToolTipText(FText::FromString(Url))
			.OnClicked_Lambda([this, Index]() { RequestWebsite(Index); return FReply::Handled(); })
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(STextBlock).Text(FText::FromString(Url.RightChop(8)))
					.Font(PageFont(14)).ColorAndOpacity(LinkColor)]
				+ SVerticalBox::Slot().AutoHeight()
				[SNew(SBox).HeightOverride(1)
					[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(LinkColor)]]
			];
		WebsiteLinks.Add(Link);
		AddAtPage(FRect{ 70, 295.f + Index * 19, 444, 313.f + Index * 19 }, Link);
	}

	}

	AddAtPage(FRect{ AcceptButtonX, ButtonY, AcceptButtonX + ButtonWidth, ButtonY + ButtonHeight },
		MakeButton(
			ArtObject,
			LOCTEXT("Ok", "OK"), // STRINGTABLE 20
			ButtonFontHeight,
			FOnClicked::CreateLambda([this]() { Accept(); return FReply::Handled(); }),
			ButtonStyles));

	AddAtPage(FRect{ CancelButtonX, ButtonY, CancelButtonX + ButtonWidth, ButtonY + ButtonHeight },
		MakeButton(
			ArtObject,
			LOCTEXT("Cancel", "Cancel"), // STRINGTABLE 21
			ButtonFontHeight,
			FOnClicked::CreateLambda([this]()
			{
				OnCancelled.ExecuteIfBound();
				return FReply::Handled();
			}),
			ButtonStyles));

	if (bAllowExtraControls) AddAtPage(FRect{ 70, ButtonY, 70 + ButtonWidth, ButtonY + ButtonHeight },
		MakeButton(ArtObject, LOCTEXT("Browse", "Browse..."), ButtonFontHeight,
			FOnClicked::CreateLambda([this]() { Browse(); return FReply::Handled(); }), ButtonStyles));
	PickerScreen = MakeScaledScreen(Canvas);
	WebsiteConfirmationScreen = BuildWebsiteConfirmation();
	ChildSlot[PickerScreen.ToSharedRef()];
}

FString SSimCopterUserCityPicker::GetCityWebsiteUrl(int32 Index)
{
	if (Index == 0) return TEXT("https://cities.opencity2k.org/");
	if (Index == 1) return TEXT("https://patcoston.com/sc2k/cities.asp");
	return FString();
}

TSharedRef<SWidget> SSimCopterUserCityPicker::BuildWebsiteConfirmation()
{
	WebsiteCancelButton = MakeButton(nullptr, LOCTEXT("WebsiteCancel", "Cancel"), ButtonFontHeight,
		FOnClicked::CreateLambda([this]() { CompleteWebsiteConfirmation(false); return FReply::Handled(); }), ButtonStyles);
	WebsiteOpenButton = MakeButton(nullptr, LOCTEXT("WebsiteOpen", "Open site"), ButtonFontHeight,
		FOnClicked::CreateLambda([this]() { CompleteWebsiteConfirmation(true); return FReply::Handled(); }), ButtonStyles);
	return MakeScaledScreen(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[SNew(SBox).WidthOverride(460)
			[MakeFallbackPlate(SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(24, 22, 24, 12)
				[SNew(STextBlock).Text(LOCTEXT("WebsiteTitle", "Open external website?"))
					.Font(PageFont(22, true)).ColorAndOpacity(PlateTextColor).Justification(ETextJustify::Center)]
				+ SVerticalBox::Slot().AutoHeight().Padding(24, 0, 24, 20)
				[SNew(STextBlock).Text_Lambda([this]()
					{
						return FText::Format(LOCTEXT("WebsiteWarning",
							"You're about to leave the game for a third-party website in your browser.\n\n"
							"We don't control this site or its content.\n\n{0}"), FText::FromString(GetCityWebsiteUrl(PendingWebsiteIndex)));
					})
					.Font(PageFont(17)).ColorAndOpacity(PlateTextColor).WrapTextAt(406)
					.WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
					.Justification(ETextJustify::Center)]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(24, 0, 24, 22)
				[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 16, 0)
					[SNew(SBox).WidthOverride(ButtonWidth).HeightOverride(ButtonHeight)[WebsiteCancelButton.ToSharedRef()]]
					+ SHorizontalBox::Slot().AutoWidth()
					[SNew(SBox).WidthOverride(ButtonWidth).HeightOverride(ButtonHeight)[WebsiteOpenButton.ToSharedRef()]]
				])]]);
}

void SSimCopterUserCityPicker::RequestWebsite(int32 Index)
{
	if (!bAllowExtraControls || PendingWebsiteIndex != INDEX_NONE || GetCityWebsiteUrl(Index).IsEmpty()) return;
	PendingWebsiteIndex = Index;
	ChildSlot[WebsiteConfirmationScreen.ToSharedRef()];
	SetControllerSelection(WebsiteCancelButton);
	FSlateApplication::Get().SetKeyboardFocus(WebsiteCancelButton, EFocusCause::SetDirectly);
}

void SSimCopterUserCityPicker::CompleteWebsiteConfirmation(bool bOpen)
{
	if (PendingWebsiteIndex == INDEX_NONE) return;
	const int32 Index = PendingWebsiteIndex;
	const FString Url = GetCityWebsiteUrl(Index);
	PendingWebsiteIndex = INDEX_NONE;
	ChildSlot[PickerScreen.ToSharedRef()];
	SetControllerSelection(WebsiteLinks[Index]);
	FSlateApplication::Get().SetKeyboardFocus(WebsiteLinks[Index], EFocusCause::SetDirectly);
	if (!bOpen) return;
	if (OnOpenWebsite.IsBound()) OnOpenWebsite.Execute(Url);
	else
	{
		FString Error;
		FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
		if (!Error.IsEmpty()) FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("WebsiteError", "Unable to open your browser.\n{0}"), FText::FromString(Error)));
	}
}

TSharedRef<ITableRow> SSimCopterUserCityPicker::MakeRow(
	TSharedPtr<FSimCopterUserCityEntry> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FSimCopterUserCityEntry>>, OwnerTable)
		.Style(RowStyle.Get())
		.Padding(FMargin(6.0f, 1.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item.IsValid() ? Item->DisplayName : FString()))
			.Font(PageFont(ListFontHeight))
			.ColorAndOpacity(FSlateColor(ListText))
		];
}

void SSimCopterUserCityPicker::Browse()
{
	if (!bAllowExtraControls || PendingWebsiteIndex != INDEX_NONE) return;
	FString Path;
	if (!SimCopterFileBrowser::Open(AsShared(), false, TEXT("Open a SimCity 2000 city"), Path)) return;
	FSimCity2000City City;
	FString Error;
	if (!FPaths::GetExtension(Path).Equals(TEXT("sc2"), ESearchCase::IgnoreCase) ||
		!FSimCity2000Reader::LoadCityFromFile(Path, City, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Unable to open this city.\n") + Error));
		return;
	}
	OnAccepted.ExecuteIfBound(Path);
}

void SSimCopterUserCityPicker::Accept()
{
	if (PendingWebsiteIndex != INDEX_NONE || !ListView.IsValid())
	{
		return;
	}

	const TArray<TSharedPtr<FSimCopterUserCityEntry>> Selected = ListView->GetSelectedItems();
	if (Selected.Num() > 0 && Selected[0].IsValid())
	{
		OnAccepted.ExecuteIfBound(Selected[0]->FilePath);
	}
}

FReply SSimCopterUserCityPicker::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (PendingWebsiteIndex != INDEX_NONE)
	{
		if (Key == EKeys::Escape || Key == EKeys::Enter)
		{
			if (!InKeyEvent.IsRepeat()) CompleteWebsiteConfirmation(
				Key == EKeys::Enter && GetControllerSelection() == WebsiteOpenButton);
			return FReply::Handled();
		}
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
	if (Key == EKeys::Escape)
	{
		OnCancelled.ExecuteIfBound();
		return FReply::Handled();
	}
	if (Key == EKeys::Enter)
	{
		Accept();
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
