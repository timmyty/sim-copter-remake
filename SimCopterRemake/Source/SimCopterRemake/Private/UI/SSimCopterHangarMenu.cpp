// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSimCopterHangarMenu.h"
#include "SimCopterEquipmentPreview.h"
#include "Game/SimCopterCheats.h"
#include "SimCopterMenuFocusOutline.h"

#include "Audio/SimCopterAudioSubsystem.h"
#include "City/SimCopterDayNight.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterHelicopterRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/SimCopterCareerSubsystem.h"
#include "InputCoreTypes.h"
#include "Math/TransformCalculus2D.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/SimCopterHangarArt.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
using namespace SimCopterHangarLayout;

// Ink on the notebook pages, and the label colour on the hangar backdrop.
const FLinearColor PaperInk(0.06f, 0.06f, 0.08f, 1.0f);
const FLinearColor PaperInkDim(0.24f, 0.24f, 0.28f, 1.0f);
const FLinearColor ShellLabel(0.94f, 0.94f, 0.90f, 1.0f);
const FLinearColor SelectionTint(0.10f, 0.28f, 0.85f, 1.0f);

// SCHOOK: HangarBackdrop 0x0043b6e0 / 0x0043c540 - the shell stores the day/night flag at ui+0x112
// (`*(uint *)(param_1 + 0x112) = (DAT_004f9720 == 0)`, so the field is "is it DAY") and picks
// dhangar.bmp or nhangar.bmp off it. Both the original bitmaps and both reconstructions are here;
// the night pair is used whenever the level's day sequence says it is night.
const TCHAR* const UpscaledHangarBackdropDay = TEXT("DHANGAR-upscaled.png");
const TCHAR* const UpscaledHangarBackdropNight = TEXT("NHANGER-upscaled.png");
const TCHAR* const HangarBackdropDay = TEXT("DHANGAR.BMP");
const TCHAR* const HangarBackdropNight = TEXT("NHANGAR.BMP");

// The original's button strips: button.bmp is three 100x28 frames, cat_btn.bmp three 86x28.
const TCHAR* const ShellButtonStrip = TEXT("BUTTON.BMP");
const TCHAR* const CatalogButtonStrip = TEXT("CAT_BTN.BMP");
constexpr int32 ButtonFrameCount = 3;

// The original prints these readouts as plain integers - no thousands separator.
FString FormatDollars(const int32 Value)
{
	return FString::FromInt(Value);
}

// "12:34" of session time. The original stamps its log with the in-game date (strings 500..521);
// nothing in the remake keeps a calendar, so elapsed time stands in.
FString FormatLogStamp(const float Seconds)
{
	const int32 Whole = FMath::Max(0, FMath::FloorToInt(Seconds));
	return FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60);
}
}

void SSimCopterHangarMenu::Construct(const FArguments& InArgs)
{
	Art = InArgs._Art;
	Shop = InArgs._Shop;
	OnDoneRequested = InArgs._OnDoneRequested;

	// SCHOOK: HangarScreenSound 0x00449cb0 - the hangar builds its own hangar.wav sound object
	// and plays it once on entry (the buffer's Play is (0, 1), so it does not loop).
	if (const UWorld* World = GEngine != nullptr && GEngine->GameViewport != nullptr
			? GEngine->GameViewport->GetWorld()
			: nullptr)
	{
		if (USimCopterAudioSubsystem* Audio = World->GetSubsystem<USimCopterAudioSubsystem>())
		{
			Audio->PlayFile2D(TEXT("hangar"), SimCopterSound::ESoundDir::Root);
		}
	}

	// Open on the row the player is already flying, the way FUN_0042d420 does.
	if (const ASimCopterHelicopterPawn* Helicopter = Shop.Helicopter.Get())
	{
		const int32 Row = SimCopterHangarLayout::GetCatalogRowForTypeIndex(Helicopter->GetHelicopterTypeIndex());
		CatalogRow = Row != INDEX_NONE ? Row : 0;
		LastAircraftCatalogRow = CatalogRow;
		EquipmentPreviewType = Helicopter->IsFixedWingAircraft() ? 4 : Helicopter->GetHelicopterTypeIndex();
	}

	TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);
	PageCanvas = Canvas;

	TSharedRef<SOverlay> BackdropOverlay = SNew(SOverlay);
	Backdrop = BackdropOverlay;

	ChildSlot
	[
		SNew(SOverlay)
		// A black surround so the page letterboxes cleanly on any aspect ratio.
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.03f, 1.0f))
			[
				BackdropOverlay
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFit)
			[
				SNew(SBox)
				.WidthOverride(USimCopterHangarArt::PageWidth)
				.HeightOverride(USimCopterHangarArt::PageHeight)
				[
					Canvas
				]
			]
		]
	];

	RebuildPage();
}

void SSimCopterHangarMenu::ShowPage(const EPage NewPage)
{
	Page = NewPage;
	StatusText.Reset();
	RebuildPage();
}

void SSimCopterHangarMenu::RebuildPage()
{
	if (!PageCanvas.IsValid() || !Backdrop.IsValid())
	{
		return;
	}

	int32 PreviousFocusIndex = INDEX_NONE;
	const int32 PreviousControllerIndex = ControllerFocusableWidgets.IndexOfByKey(GetControllerSelection());
	const int32 PreviousCatalogTab = CatalogTabWidgets.IndexOfByKey(GetControllerSelection());
	if (FSlateApplication::IsInitialized())
	{
		const TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
		PreviousFocusIndex = ControllerFocusableWidgets.IndexOfByKey(FocusedWidget);
	}

	PageCanvas->ClearChildren();
	Backdrop->ClearChildren();
	ButtonStyles.Reset();
	ControllerFocusableWidgets.Reset();
	CatalogTabWidgets.Reset();

	switch (Page)
	{
	case EPage::Hangar:
		BuildHangarPage(*PageCanvas);
		break;
	case EPage::Catalog:
		BuildCatalogPage(*PageCanvas);
		break;
	case EPage::MissionLog:
		BuildMissionLogPage(*PageCanvas);
		break;
	case EPage::Inventory:
		BuildInventoryPage(*PageCanvas);
		break;
	}

	if (!StatusText.IsEmpty())
	{
		AddAt(*PageCanvas, 20.0f, 2.0f, 600.0f, 16.0f,
			MakePageText(StatusText, 11, FLinearColor(1.0f, 0.92f, 0.55f, 1.0f), ETextJustify::Center, /*bBold=*/true, /*bWrap=*/false));
	}

	if (ControllerFocusableWidgets.Num() > 0)
	{
		const int32 FocusIndex = PreviousFocusIndex == INDEX_NONE
			? 0
			: FMath::Clamp(PreviousFocusIndex, 0, ControllerFocusableWidgets.Num() - 1);
		InitialFocusWidget = ControllerFocusableWidgets[FocusIndex];
		if (Page==EPage::Catalog && CatalogRow==INDEX_NONE)
		{
			if (PreviousControllerIndex>=8 && PreviousControllerIndex<=10) SelectEquipmentAction();
			else SetControllerSelection(ControllerFocusableWidgets[2+UpgradeRow]);
		}
		else if (CatalogTabWidgets.IsValidIndex(PreviousCatalogTab))
		{
			// Rebuilding creates new buttons. Preserve the tab's identity even when Buy/Sell
			// availability changes the transaction controls preceding the aircraft list.
			SetControllerSelection(CatalogTabWidgets[PreviousCatalogTab]);
		}
		else
		{
			SetControllerSelection(ControllerFocusableWidgets.IsValidIndex(PreviousControllerIndex)
				? ControllerFocusableWidgets[PreviousControllerIndex] : InitialFocusWidget);
		}
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetAllUserFocus(SharedThis(this), EFocusCause::Navigation);
		}
	}
	else
	{
		InitialFocusWidget.Reset();
	}
}

void SSimCopterHangarMenu::AddAt(
	SConstraintCanvas& Canvas,
	const float X,
	const float Y,
	const float Width,
	const float Height,
	TSharedRef<SWidget> Content)
{
	Canvas.AddSlot()
	.Offset(FMargin(X, Y, Width, Height))
	.Alignment(FVector2D::ZeroVector)
	[
		Content
	];
}

void SSimCopterHangarMenu::AddPageBackground(SConstraintCanvas& Canvas, const TCHAR* FileName)
{
	USimCopterHangarArt* ArtObject = Art.Get();
	const FSlateBrush* Brush = ArtObject != nullptr ? ArtObject->GetBitmap(FileName) : nullptr;
	if (Brush != nullptr)
	{
		AddAt(Canvas, 0.0f, 0.0f, USimCopterHangarArt::PageWidth, USimCopterHangarArt::PageHeight,
			SNew(SImage).Image(Brush));
		return;
	}

	// No original artwork installed: a plain paper-coloured page keeps every page readable.
	AddAt(Canvas, 0.0f, 0.0f, USimCopterHangarArt::PageWidth, USimCopterHangarArt::PageHeight,
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.88f, 0.86f, 0.80f, 1.0f)));
}

TSharedRef<SWidget> SSimCopterHangarMenu::MakeArtButton(
	const TCHAR* StripFileName,
	const int32 FrameCount,
	const FString& Label,
	FOnClicked OnClicked,
	const bool bEnabled,
	const int32 FontSize)
{
	USimCopterHangarArt* ArtObject = Art.Get();
	const FSlateBrush* Normal = ArtObject != nullptr ? ArtObject->GetStripFrame(StripFileName, 2, FrameCount) : nullptr;
	const FSlateBrush* Hovered = ArtObject != nullptr ? ArtObject->GetStripFrame(StripFileName, 1, FrameCount) : nullptr;
	const FSlateBrush* Pressed = ArtObject != nullptr ? ArtObject->GetStripFrame(StripFileName, 0, FrameCount) : nullptr;

	TSharedRef<SButton> Button = SNew(SButton)
		// Focus is required for D-pad navigation and gamepad A activation while the shell owns input.
		.IsFocusable(true)
		.IsEnabled(bEnabled)
		.ContentPadding(FMargin(0.0f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.OnClicked(OnClicked)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Label))
			.Justification(ETextJustify::Center)
			.ColorAndOpacity(bEnabled ? ShellLabel : FLinearColor(0.55f, 0.55f, 0.55f, 1.0f))
			.ShadowOffset(FVector2D(1.0f, 1.0f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FontSize))
		];

	if (Normal != nullptr)
	{
		TSharedRef<FButtonStyle> Style = MakeShared<FButtonStyle>();
		Style->SetNormal(*Normal);
		Style->SetHovered(Hovered != nullptr ? *Hovered : *Normal);
		Style->SetPressed(Pressed != nullptr ? *Pressed : *Normal);
		Style->SetDisabled(*Normal);
		Style->SetNormalPadding(FMargin(0.0f));
		Style->SetPressedPadding(FMargin(0.0f));
		ButtonStyles.Add(Style);
		Button->SetButtonStyle(&Style.Get());
	}

	if (bEnabled)
	{
		ControllerFocusableWidgets.Add(Button);
	}
	Button->AddMetadata(MakeShared<FSimCopterMenuFocusOutline>(FSlateRect(0, 0, 1, 1),
		FSlateRect(0, 0, 1, 1), 0.10f));
	return Button;
}

TSharedRef<SWidget> SSimCopterHangarMenu::MakeReadingPanel(const TSharedRef<SScrollBox>& Scroll, const FString& Name)
{
	TSharedRef<FButtonStyle> Style = MakeShared<FButtonStyle>();
	Style->SetNormal(FSlateNoResource());
	Style->SetHovered(FSlateNoResource());
	Style->SetPressed(FSlateNoResource());
	Style->SetDisabled(FSlateNoResource());
	Style->SetNormalPadding(FMargin(0.0f));
	Style->SetPressedPadding(FMargin(0.0f));
	ButtonStyles.Add(Style);
	// The scroll box is the button's content, so the mouse wheel and scroll bar still reach it.
	TSharedRef<SButton> Panel = SNew(SButton)
		.IsFocusable(false)
		.ButtonStyle(&Style.Get())
		.ContentPadding(FMargin(0.0f))
		.ToolTipText(FText::FromString(Name))
		[
			Scroll
		];
	Panel->AddMetadata(MakeShared<FSimCopterMenuFocusOutline>(FSlateRect(0, 0, 1, 1), FSlateRect(0, 0, 1, 1), 0.04f));
	(Name == TEXT("History") ? HistoryPanel : DescriptionPanel) = Panel;
	return Panel;
}

bool SSimCopterHangarMenu::TryScrollReadingPanel(const FKey& Key)
{
	const TSharedPtr<SWidget> Selected = GetControllerSelection();
	const TSharedPtr<SScrollBox> Scroll = Selected.IsValid() && Selected == HistoryPanel ? HistoryScroll
		: Selected.IsValid() && Selected == DescriptionPanel ? DescriptionScroll : nullptr;
	if (!Scroll.IsValid()) return false;
	const bool bStick = Key == EKeys::Gamepad_RightStick_Up || Key == EKeys::Gamepad_RightStick_Down;
	const bool bUp = Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_RightStick_Up;
	const bool bDown = Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_RightStick_Down;
	if (!bUp && !bDown) return false;
	constexpr float StepPixels = 24.0f;
	const float Offset = Scroll->GetScrollOffset();
	const float End = Scroll->GetScrollOffsetOfEnd();
	// At either end the D-pad leaves the panel, so the rest of the page stays reachable.
	if (bUp && Offset <= 0.5f) return bStick;
	if (bDown && Offset >= End - 0.5f) return bStick;
	Scroll->SetScrollOffset(FMath::Clamp(Offset + (bUp ? -StepPixels : StepPixels), 0.0f, End));
	return true;
}

TSharedRef<SWidget> SSimCopterHangarMenu::MakeHotspot(FOnClicked OnClicked, const FString& ToolTip)
{
	TSharedRef<FButtonStyle> Style = MakeShared<FButtonStyle>();
	Style->SetNormal(FSlateNoResource());
	Style->SetHovered(FSlateNoResource());
	Style->SetPressed(FSlateNoResource());
	Style->SetDisabled(FSlateNoResource());
	Style->SetNormalPadding(FMargin(0.0f));
	Style->SetPressedPadding(FMargin(0.0f));
	ButtonStyles.Add(Style);

	TSharedRef<SButton> Button = SNew(SButton)
		.IsFocusable(false)
		.ButtonStyle(&Style.Get())
		.ButtonColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
		.ContentPadding(FMargin(0.0f))
		.ToolTipText(FText::FromString(ToolTip))
		.OnClicked(OnClicked);
	CatalogTabWidgets.Add(Button);
	return Button;
}

TSharedRef<SWidget> SSimCopterHangarMenu::MakePageText(
	const FString& Text,
	const int32 FontSize,
	const FLinearColor& Color,
	const ETextJustify::Type Justify,
	const bool bBold,
	const bool bWrap,
	const float WrapWidth)
{
	TSharedRef<STextBlock> Block = SNew(STextBlock)
		.Text(FText::FromString(Text))
		.Justification(Justify)
		.ColorAndOpacity(Color)
		.Font(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), FontSize));

	if (bWrap && WrapWidth > 0.0f)
	{
		Block->SetWrapTextAt(WrapWidth);
	}
	else if (bWrap)
	{
		Block->SetAutoWrapText(true);
	}

	return Block;
}

// --- hangar page -----------------------------------------------------------------------------

void SSimCopterHangarMenu::BuildHangarPage(SConstraintCanvas& Canvas)
{
	// The high-resolution reconstruction fills the same viewport backdrop slot as dhangar.bmp.
	// Keep the original available as a fallback for a missing or unreadable bundled image.
	USimCopterHangarArt* ArtObject = Art.Get();

	// The shell is entered from the airport, so the flag is read once on open rather than tracked:
	// the original's was a per-city constant and could not change while the player was inside.
	const UWorld* World = (GEngine != nullptr && GEngine->GameViewport != nullptr)
		? GEngine->GameViewport->GetWorld()
		: nullptr;
	const bool bNight = USimCopterDayNightSubsystem::IsNightForWorld(World);

	const FSlateBrush* Backdrop3D = ArtObject != nullptr
		? ArtObject->GetBundledSlateImage(bNight ? UpscaledHangarBackdropNight : UpscaledHangarBackdropDay)
		: nullptr;
	if (Backdrop3D == nullptr && ArtObject != nullptr)
	{
		Backdrop3D = ArtObject->GetBitmap(bNight ? HangarBackdropNight : HangarBackdropDay);
	}
	if (Backdrop3D != nullptr && Backdrop.IsValid())
	{
		Backdrop->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFill)
			[
				SNew(SImage).Image(Backdrop3D)
			]
		];
	}

	// Strings 125..128.
	const TCHAR* const Labels[4] = { TEXT("Catalog"), TEXT("Mission Log"), TEXT("Inventory"), TEXT("Done") };
	const EPage Targets[3] = { EPage::Catalog, EPage::MissionLog, EPage::Inventory };

	for (int32 Index = 0; Index < 4; ++Index)
	{
		FOnClicked Clicked = Index < 3
			? FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleShowPage, Targets[Index])
			: FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleDone);

		AddAt(Canvas, HangarButtonX[Index], HangarButtonY, ShellButtonWidth, ShellButtonHeight,
			MakeArtButton(ShellButtonStrip, ButtonFrameCount, Labels[Index], Clicked));
	}
	if (const auto* Missions = Shop.Missions.Get(); Missions && Missions->CanAdvanceToNextCareerCity())
	{
		AddAt(Canvas, 245.0f, HangarButtonY - 40.0f, 150.0f, ShellButtonHeight,
			MakeArtButton(ShellButtonStrip, ButtonFrameCount, TEXT("Next Level"),
				FOnClicked::CreateLambda([this]()
				{
					if (auto* Current = Shop.Missions.Get()) Current->RequestNextCareerCity();
					return FReply::Handled();
				})));
	}
}

// --- catalog ---------------------------------------------------------------------------------

void SSimCopterHangarMenu::BuildCatalogPage(SConstraintCanvas& Canvas)
{
	const bool bUpgrades = CatalogRow < 0 || CatalogRow >= CatalogTabCount;
	AddPageBackground(Canvas, bUpgrades ? TEXT("CATALOGE.BMP") : TEXT("CATALOG.BMP"));

	if (bUpgrades)
	{
		BuildCatalogUpgradesPage(Canvas);
	}
	else
	{
		BuildCatalogHelicopterPage(Canvas);
	}

	BuildCatalogTabs(Canvas);
}

void SSimCopterHangarMenu::BuildCatalogTabs(SConstraintCanvas& Canvas)
{
	USimCopterHangarArt* ArtObject = Art.Get();
	if (const FSlateBrush* Strip = ArtObject != nullptr ? ArtObject->GetCatalogTabStrip(CatalogRow) : nullptr)
	{
		AddAt(Canvas, CatalogTabStripX, CatalogTabStripY, CatalogTabStripWidth, CatalogTabStripHeight,
			SNew(SImage).Image(Strip));
	}

	for (int32 Tab = 0; Tab < CivilianCatalogTabCount; ++Tab)
	{
		const int32 TypeIndex = SimCopterHangarLayout::GetTypeIndexForCatalogRow(Tab);
		AddAt(Canvas, CatalogTabLeft[Tab], CatalogTabStripY, CatalogTabRight[Tab] - CatalogTabLeft[Tab], CatalogTabHitHeight,
			MakeHotspot(
				FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleSelectCatalogRow, Tab),
				SimCopterHangarShop::GetModelDisplayName(TypeIndex)));
	}

	const auto MysteryState = SimCopterHangarShop::GetHelicopterRowState(Shop, ApacheCatalogRow);
	const TCHAR* MysteryLabel = MysteryState.bMystery ? TEXT("?  Mystery helicopter") : TEXT("Apache");
	AddAt(Canvas, 330.0f, 443.0f, 215.0f, 16.0f,
		MakePageText(MysteryLabel, 9, CatalogRow == ApacheCatalogRow ? SelectionTint : PaperInk, ETextJustify::Center, true, false));
	AddAt(Canvas, 330.0f, 442.0f, 215.0f, 19.0f,
		MakeHotspot(FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleSelectCatalogRow, ApacheCatalogRow), MysteryLabel));

	// The "Upgrades" tab (string 435) is printed under the model tabs on both catalog pages.
	const bool bUpgrades = CatalogRow < 0 || CatalogRow >= CatalogTabCount;
	AddAt(Canvas, 140.0f, 442.0f, 160.0f, 15.0f,
		MakePageText(TEXT("Upgrades"), 9, bUpgrades ? SelectionTint : PaperInk, ETextJustify::Left, /*bBold=*/true, /*bWrap=*/false));
	AddAt(Canvas, 134.0f, 439.0f, 174.0f, 20.0f,
		MakeHotspot(FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleSelectCatalogRow, int32(INDEX_NONE)), TEXT("Upgrades")));
}

void SSimCopterHangarMenu::BuildCatalogHelicopterPage(SConstraintCanvas& Canvas)
{
	USimCopterHangarArt* ArtObject = Art.Get();
	const int32 TypeIndex = SimCopterHangarLayout::GetTypeIndexForCatalogRow(CatalogRow);
	const SimCopterHangarShop::FRowState State = SimCopterHangarShop::GetHelicopterRowState(Shop, CatalogRow);

	if (TypeIndex == 2)
	{
		AddAt(Canvas, CatalogDrawingX + 12, CatalogDrawingY + 12, CatalogDrawingWidth - 24, 38,
			MakePageText(State.bMystery ? TEXT("MYSTERY HELICOPTER") : TEXT("AH-64 APACHE"), 17, PaperInk, ETextJustify::Center, true, false));
		AddAt(Canvas, CatalogDrawingX + 12, CatalogDrawingY + 62, CatalogDrawingWidth - 24, 80,
			MakePageText(State.bMystery ? TEXT("?") : TEXT("ARMED / READY TO FLY"), State.bMystery ? 64 : 18, PaperInkDim, ETextJustify::Center, true, false));
		AddAt(Canvas, CatalogDrawingX + 20, CatalogDrawingY + 158, CatalogDrawingWidth - 40, 70,
			MakePageText(State.bMystery ? TEXT("A classified aircraft awaits in the late-game cities.") : TEXT("Missiles + machine gun\nNo passenger seats"), 12, PaperInk, ETextJustify::Center, false, true, CatalogDrawingWidth - 40));
	}
	else if (const FSlateBrush* Drawing = ArtObject != nullptr ? ArtObject->GetCatalogDrawing(CatalogRow) : nullptr)
	{
		AddAt(Canvas, CatalogDrawingX, CatalogDrawingY, CatalogDrawingWidth, CatalogDrawingHeight,
			SNew(SImage).Image(Drawing));
	}
	else
	{
		// Without the blueprint the page still has to say which model it is showing.
		AddAt(Canvas, CatalogDrawingX + 12.0f, CatalogDrawingY + 12.0f, CatalogDrawingWidth - 24.0f, 40.0f,
			MakePageText(SimCopterHangarShop::GetModelDisplayName(TypeIndex), 22, PaperInk, ETextJustify::Left, true, false));
	}

	// Expanded details retain the catalog layout and scroll inside their paper panels.
	AddAt(Canvas, CatalogHistoryX + 6.0f, CatalogHistoryY + 3.0f, CatalogHistoryWidth - 12.0f, 101.0f,
		MakeReadingPanel(SAssignNew(HistoryScroll, SScrollBox).ScrollBarAlwaysVisible(true)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[MakePageText(TEXT("History"), 11, PaperInk, ETextJustify::Left, true, false)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 12, 8)[MakePageText(State.bMystery ? TEXT("Identity classified. Visit the city where this helicopter is hidden to uncover its card.") : SimCopterHangarShop::GetCatalogHistory(CatalogRow), 10, PaperInkDim, ETextJustify::Left, false, true, CatalogHistoryWidth - 36)]
			+ SVerticalBox::Slot().AutoHeight()[MakePageText(TEXT("Specialties"), 11, PaperInk, ETextJustify::Left, true, false)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 12, 4)[MakePageText(State.bMystery ? TEXT("Details unlock with the aircraft.") : SimCopterHangarShop::GetCatalogSpecialties(CatalogRow), 10, PaperInkDim, ETextJustify::Left, false, true, CatalogHistoryWidth - 36)]
		], TEXT("History")));
	AddAt(Canvas, CatalogDescriptionX + 6.0f, CatalogDescriptionY + 3.0f, CatalogDescriptionWidth - 12.0f, 16.0f,
		MakePageText(TEXT("Description"), 11, PaperInk, ETextJustify::Left, true, false));
	AddAt(Canvas, CatalogDescriptionX + 8.0f, CatalogDescriptionY + 22.0f, CatalogDescriptionWidth - 16.0f, 82.0f,
		MakeReadingPanel(SAssignNew(DescriptionScroll, SScrollBox).ScrollBarAlwaysVisible(true)
		+ SScrollBox::Slot().Padding(0, 0, 12, 4)
		[MakePageText(State.bMystery ? TEXT("The model, equipment and purchase price remain hidden until you reach its special city.") : SimCopterHangarShop::GetCatalogDescription(CatalogRow), 10, PaperInkDim, ETextJustify::Left, false, true, CatalogDescriptionWidth - 40)], TEXT("Description")));

	// Funds and value readouts (strings 433 / 434).
	AddAt(Canvas, CatalogPanelX, CatalogFundsLabelY, CatalogPanelWidth, 32.0f,
		MakePageText(TEXT("Current\nFunds"), 11, PaperInk, ETextJustify::Center, true, false));
	AddAt(Canvas, CatalogPanelX, CatalogFundsValueY, CatalogPanelWidth, 20.0f,
		MakePageText(FormatDollars(SimCopterHangarShop::GetCurrentFunds(Shop)), 14, PaperInk, ETextJustify::Center, true, false));
	AddAt(Canvas, CatalogPanelX, CatalogValueLabelY, CatalogPanelWidth, 32.0f,
		MakePageText(TEXT("Item\nValue"), 11, PaperInk, ETextJustify::Center, true, false));
	AddAt(Canvas, CatalogPanelX, CatalogValueValueY, CatalogPanelWidth, 20.0f,
		MakePageText(State.bMystery ? TEXT("???") : FormatDollars(State.ItemValue), 14, PaperInk, ETextJustify::Center, true, false));

	// Buy (442) / Sell (443) / Done (444).
	AddAt(Canvas, CatalogPanelX, CatalogButtonY[0], CatalogButtonWidth, CatalogButtonHeight,
		MakeArtButton(CatalogButtonStrip, ButtonFrameCount, TEXT("Buy"),
			FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleBuy), State.bCanBuy));
	AddAt(Canvas, CatalogPanelX, CatalogButtonY[1], CatalogButtonWidth, CatalogButtonHeight,
		MakeArtButton(CatalogButtonStrip, ButtonFrameCount, TEXT("Sell"),
			FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleSell), State.bCanSell));
	AddAt(Canvas, CatalogPanelX, CatalogButtonY[2], CatalogButtonWidth, CatalogButtonHeight,
		MakeArtButton(CatalogButtonStrip, ButtonFrameCount, TEXT("Done"),
			FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleShowPage, EPage::Hangar)));

	if (State.bOwned)
	{
		AddAt(Canvas, CatalogPanelX - 4.0f, CatalogValueValueY + 20.0f, CatalogPanelWidth + 8.0f, 14.0f,
			MakePageText(TEXT("On the books"), 9, SelectionTint, ETextJustify::Center, true, false));
	}
}

void SSimCopterHangarMenu::BuildCatalogUpgradesPage(SConstraintCanvas& Canvas)
{
	const auto Paper=FLinearColor(.86f,.83f,.71f);
	AddAt(Canvas,12,20,610,392,SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(Paper));
	AddAt(Canvas,24,27,580,20,MakePageText(TEXT("HELICOPTER EQUIPMENT"),14,PaperInk,ETextJustify::Left,true,false));
	const auto* Active=Shop.Helicopter.Get();
	const FString ActiveName=Active ? SimCopterHangarShop::GetModelDisplayName(Active->GetHelicopterTypeIndex()) : TEXT("None");
	AddAt(Canvas,24,51,578,18,MakePageText(TEXT("Active helicopter: ")+ActiveName,11,PaperInk,ETextJustify::Left,true,false));
	AddAt(Canvas,24,72,578,16,MakePageText(TEXT("Equipment purchases apply to your fleet. Arrows browse previews."),9,PaperInkDim,ETextJustify::Left,false,false));
	// Explicit paper buttons keep this page readable in both the packaged game's
	// core style and the editor's dark theme.
	auto EquipmentButtonStyle=MakeShared<FButtonStyle>();
	FSlateBrush ButtonBrush=*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	EquipmentButtonStyle->SetNormal(ButtonBrush);
	EquipmentButtonStyle->SetHovered(ButtonBrush);
	ButtonBrush.TintColor=FLinearColor(.78f,.81f,.82f);
	EquipmentButtonStyle->SetPressed(ButtonBrush);
	ButtonBrush.TintColor=FLinearColor(.58f,.59f,.55f);
	EquipmentButtonStyle->SetDisabled(ButtonBrush);
	EquipmentButtonStyle->SetNormalPadding(FMargin(0));
	EquipmentButtonStyle->SetPressedPadding(FMargin(0));
	ButtonStyles.Add(EquipmentButtonStyle);

	auto Text=[&](float X,float Y,float W,float H,TAttribute<FText> Value,int32 Size,FLinearColor Color,bool Bold=false)
	{
		AddAt(Canvas,X,Y,W,H,SNew(STextBlock).Text(Value).ColorAndOpacity(Color)
			.Font(FCoreStyle::GetDefaultFontStyle(Bold ? TEXT("Bold") : TEXT("Regular"),Size)).WrapTextAt(W));
	};
	auto Button=[&](float X,float Y,float W,const TCHAR* Label,FOnClicked Click,TAttribute<bool> Enabled)
	{
		auto B=SNew(SButton).ButtonStyle(&EquipmentButtonStyle.Get()).ButtonColorAndOpacity(FLinearColor(.96f,.94f,.85f))
			.IsFocusable(true).IsEnabled(Enabled).HAlign(HAlign_Center).VAlign(VAlign_Center)
			.ContentPadding(0).OnClicked(Click)[MakePageText(Label,11,PaperInk,ETextJustify::Center,true,false)];
		ControllerFocusableWidgets.Add(B);
		B->AddMetadata(MakeShared<FSimCopterMenuFocusOutline>(FSlateRect(0,0,1,1),FSlateRect(0,0,1,1),.10f));
		AddAt(Canvas,X,Y,W,26,B);
	};
	Button(304,96,28,TEXT("<"),FOnClicked::CreateSP(this,&SSimCopterHangarMenu::CycleEquipmentHelicopter,-1),true);
	Button(582,96,28,TEXT(">"),FOnClicked::CreateSP(this,&SSimCopterHangarMenu::CycleEquipmentHelicopter,1),true);
	Text(339,97,240,20,TAttribute<FText>::CreateLambda([this]()
	{
		const int32 Row=SimCopterHangarLayout::GetCatalogRowForTypeIndex(EquipmentPreviewType);
		const bool bActive=Shop.Helicopter.IsValid() && Shop.Helicopter->GetHelicopterTypeIndex()==EquipmentPreviewType;
		return FText::FromString(FString(SimCopterHangarShop::GetModelDisplayName(EquipmentPreviewType))+
			(bActive ? TEXT(" / ACTIVE") : SimCopterHangarShop::GetHelicopterRowState(Shop,Row).bOwned ? TEXT(" / OWNED") : TEXT(" / CATALOG")));
	}),10,PaperInk,true);
	AddAt(Canvas,304,128,306,206,SNew(SSimCopterEquipmentPreview)
		.TypeIndex_Lambda([this](){return EquipmentPreviewType;})
		.OwnedMask_Lambda([this](){return GetOwnedEquipmentMask();})
		.HighlightRow_Lambda([this](){return UpgradeRow;}));
	Text(310,339,295,15,TAttribute<FText>::CreateLambda([this]()
	{
		return FText::FromString(FString(SimCopterHelicopterRegistry::GetToolDisplayName(SimCopterHangarShop::GetToolForUpgradeRow(UpgradeRow)))+
			(SimCopterHangarShop::GetUpgradeRowState(Shop,UpgradeRow).bOwned ? TEXT(" / PURCHASED") : TEXT(" / PREVIEW")));
	}),10,PaperInk,true);
	Text(310,359,295,34,TAttribute<FText>::CreateLambda([this](){return FText::FromString(SimCopterEquipmentPreview::GetFittingDescription(UpgradeRow));}),9,PaperInkDim);

	const TCHAR* Help[]={TEXT("Drop water on fires."),TEXT("Direct traffic and crowds."),TEXT("Includes ten canisters."),TEXT("Aim water from the bucket."),TEXT("Winch people into the cabin."),TEXT("Recover cars, boats and people."),TEXT("Scope and nonlethal remote fire.")};
	for (int32 Row=0; Row<SimCopterHangarShop::UpgradeRowCount; ++Row)
	{
		const float Y=96+Row*36;
		auto B=SNew(SButton).ButtonStyle(&EquipmentButtonStyle.Get()).IsFocusable(true).ContentPadding(3)
			.ButtonColorAndOpacity_Lambda([this,Row](){return Row==UpgradeRow ? FLinearColor(.98f,.75f,.35f) : FLinearColor(.96f,.94f,.85f);})
			.OnClicked(FOnClicked::CreateSP(this,&SSimCopterHangarMenu::HandleActivateUpgradeRow,Row))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(30).HeightOverride(30)
					[SNew(SSimCopterEquipmentIcon).Row(Row)
					.Owned_Lambda([this,Row](){return SimCopterHangarShop::GetUpgradeRowState(Shop,Row).bOwned;})
					.Highlighted_Lambda([this,Row](){return UpgradeRow==Row;})]]
				+SHorizontalBox::Slot().FillWidth(1).Padding(6,0,0,0)
				[SNew(SVerticalBox)
					+SVerticalBox::Slot().AutoHeight()[MakePageText(SimCopterHelicopterRegistry::GetToolDisplayName(SimCopterHangarShop::GetToolForUpgradeRow(Row)),10,PaperInk,ETextJustify::Left,true,false)]
					+SVerticalBox::Slot().AutoHeight()[MakePageText(Help[Row],8,PaperInkDim,ETextJustify::Left,false,false)]
				]
			];
		ControllerFocusableWidgets.Add(B);
		B->AddMetadata(MakeShared<FSimCopterMenuFocusOutline>(FSlateRect(0,0,1,1),FSlateRect(0,0,1,1),.08f));
		AddAt(Canvas,22,Y,271,34,B);
	}
	Text(24,354,270,16,TAttribute<FText>::CreateLambda([this]()
	{
		const auto State=SimCopterHangarShop::GetUpgradeRowState(Shop,UpgradeRow);
		return FText::FromString(FString::Printf(TEXT("Funds: $%d    %s: $%d"),SimCopterHangarShop::GetCurrentFunds(Shop),State.bOwned ? TEXT("Sell") : TEXT("Buy"),State.ItemValue));
	}),10,PaperInk,true);
	Button(24,377,82,TEXT("Buy"),FOnClicked::CreateSP(this,&SSimCopterHangarMenu::HandleBuy),
		TAttribute<bool>::CreateLambda([this](){return SimCopterHangarShop::GetUpgradeRowState(Shop,UpgradeRow).bCanBuy;}));
	Button(115,377,82,TEXT("Sell"),FOnClicked::CreateSP(this,&SSimCopterHangarMenu::HandleSell),
		TAttribute<bool>::CreateLambda([this](){return SimCopterHangarShop::GetUpgradeRowState(Shop,UpgradeRow).bCanSell;}));
	Button(206,377,82,TEXT("Done"),FOnClicked::CreateSP(this,&SSimCopterHangarMenu::HandleShowPage,EPage::Hangar),true);
	AddAt(Canvas,310,381,300,30,MakePageText(TEXT("Left/Right or LB/RB: helicopter\nA: Buy/Sell/Done    B: back to item"),8,PaperInkDim,ETextJustify::Left,false,false));
}
// --- mission log -----------------------------------------------------------------------------

void SSimCopterHangarMenu::BuildMissionLogPage(SConstraintCanvas& Canvas)
{
	AddPageBackground(Canvas, TEXT("MSSNLOG.BMP"));

	const USimCopterCareerSubsystem* Career = Shop.Career.Get();
	TArray<FSimCopterCareerLogEntry> Entries;
	if (Career != nullptr)
	{
		Entries = Career->GetLogEntries();
	}

	if (LogSort == ELogSort::ByType)
	{
		// String 531: group the lines by the mission they belong to, newest first inside a group.
		Entries.StableSort([](const FSimCopterCareerLogEntry& Left, const FSimCopterCareerLogEntry& Right)
		{
			if (Left.TypeMask != Right.TypeMask)
			{
				return Left.TypeMask < Right.TypeMask;
			}
			return Left.SessionSeconds < Right.SessionSeconds;
		});
	}

	AddAt(Canvas, LogHeaderX + 8.0f, LogHeaderY + 7.0f, LogPageWidth - 16.0f, 16.0f,
		MakePageText(
			FString::Printf(TEXT("Mission Log        %d entries        %s"),
				Entries.Num(),
				LogSort == ELogSort::ByTime ? TEXT("By Time") : TEXT("By Type")),
			11,
			PaperInk,
			ETextJustify::Left,
			true,
			false));

	// The page holds this many ruled lines; the newest entries win when the log is longer.
	const int32 VisibleLines = FMath::FloorToInt(LogPageHeight / LogLineHeight);
	const int32 First = FMath::Max(0, Entries.Num() - VisibleLines);

	for (int32 Index = First; Index < Entries.Num(); ++Index)
	{
		const FSimCopterCareerLogEntry& Entry = Entries[Index];
		const float LineY = LogPageY + 4.0f + (Index - First) * LogLineHeight;

		AddAt(Canvas, LogPageX + 8.0f, LineY, 40.0f, LogLineHeight,
			MakePageText(FormatLogStamp(Entry.SessionSeconds), 9, PaperInkDim, ETextJustify::Left, false, false));
		AddAt(Canvas, LogPageX + 52.0f, LineY, LogPageWidth - 60.0f, LogLineHeight,
			MakePageText(Entry.Text, 9, PaperInk, ETextJustify::Left, false, false));
	}

	if (Entries.Num() == 0)
	{
		AddAt(Canvas, LogPageX + 8.0f, LogPageY + 8.0f, LogPageWidth - 16.0f, 16.0f,
			MakePageText(TEXT("Nothing logged yet."), 10, PaperInkDim, ETextJustify::Left, false, false));
	}

	// Strings 530 / 531 / 532.
	AddAt(Canvas, LogByTimeX, LogButtonY, ShellButtonWidth, ShellButtonHeight,
		MakeArtButton(ShellButtonStrip, ButtonFrameCount, TEXT("By Time"),
			FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleSetLogSort, ELogSort::ByTime), LogSort != ELogSort::ByTime));
	AddAt(Canvas, LogByTypeX, LogButtonY, ShellButtonWidth, ShellButtonHeight,
		MakeArtButton(ShellButtonStrip, ButtonFrameCount, TEXT("By Type"),
			FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleSetLogSort, ELogSort::ByType), LogSort != ELogSort::ByType));
	AddAt(Canvas, LogDoneX, LogButtonY, ShellButtonWidth, ShellButtonHeight,
		MakeArtButton(ShellButtonStrip, ButtonFrameCount, TEXT("Done"),
			FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleShowPage, EPage::Hangar)));
}

// --- inventory -------------------------------------------------------------------------------

void SSimCopterHangarMenu::BuildInventoryPage(SConstraintCanvas& Canvas)
{
	AddPageBackground(Canvas, TEXT("INVNTORY.BMP"));

	// Letterheads (strings 421 / 420 / 422). Unlike the upgrades page these are not printed on the
	// artwork - the clipboard's header band is blank paper.
	AddAt(Canvas, InventoryHeaderLeftX, InventoryHeaderY, InventoryHeaderWidth, InventoryHeaderHeight,
		MakePageText(SimCopterHangarShop::GetInventoryHeaderLeft(), 6, PaperInk, ETextJustify::Left, false, true, InventoryHeaderWidth));
	AddAt(Canvas, InventoryHeaderCentreX, InventoryHeaderY + 4.0f, InventoryHeaderWidth, 26.0f,
		MakePageText(SimCopterHangarShop::GetInventoryHeaderCentre(), 8, PaperInk, ETextJustify::Center, true, false));
	AddAt(Canvas, InventoryHeaderRightX, InventoryHeaderY, InventoryHeaderWidth, InventoryHeaderHeight,
		MakePageText(SimCopterHangarShop::GetInventoryHeaderRight(), 6, PaperInk, ETextJustify::Left, false, true, InventoryHeaderWidth));

	// The five column labels are printed along the clipboard's diagonals, so they are rotated to
	// match (strings 410..414).
	for (int32 Column = 0; Column < InventoryColumnCount; ++Column)
	{
		TSharedRef<SWidget> Label = MakePageText(
			SimCopterHangarShop::GetInventoryColumnName(Column),
			8,
			PaperInk,
			ETextJustify::Left,
			false,
			false);
		Label->SetRenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-45.0f))));
		Label->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));

		AddAt(Canvas, InventoryColumnLeft[Column] + 4.0f, 232.0f, 120.0f, 12.0f, Label);
	}

	USimCopterHangarArt* ArtObject = Art.Get();
	const FSlateBrush* Tick = ArtObject != nullptr ? ArtObject->GetBitmap(TEXT("INVNCHK.BMP"), /*bColorKeyed=*/true) : nullptr;

	const USimCopterCareerSubsystem* Career = Shop.Career.Get();
	const ASimCopterHelicopterPawn* Helicopter = Shop.Helicopter.Get();
	const int32 EquipmentMask = Helicopter != nullptr ? Helicopter->GetEquipmentState().CareerEquipmentMask : 0;

	int32 Row = 0;
	for (const FSimCopterHelicopterDefinition& Definition : SimCopterHelicopterRegistry::GetDefinitions())
	{
		if (Row >= InventoryRowCount)
		{
			// The clipboard is a printed form: it has eleven ruled rows and no more.
			break;
		}
		if (Career == nullptr || !Career->OwnsHelicopter(Definition.InternalTypeIndex))
		{
			continue;
		}

		const float RowY = InventoryFirstRowY + Row * InventoryRowHeight;
		AddAt(Canvas, InventoryNameColumnX + 6.0f, RowY + 2.0f, InventoryNameColumnWidth - 12.0f, InventoryRowHeight - 2.0f,
			MakePageText(SimCopterHangarShop::GetModelDisplayName(Definition.InternalTypeIndex), 10, PaperInk, ETextJustify::Left, false, false));

		// The equipment mask is one career record, not one per airframe, so every row carries the
		// same ticks - which is exactly what career + 0x48 can express.
		for (int32 Column = 0; Column < InventoryColumnCount; ++Column)
		{
			const ESimCopterHelicopterTool Tool = SimCopterHangarShop::GetToolForInventoryColumn(Column);
			const int32 Bit = SimCopterHelicopterRegistry::GetToolCareerBit(Tool);
			if (Bit == 0 || (EquipmentMask & Bit) == 0)
			{
				continue;
			}

			const float CellX = InventoryColumnLeft[Column] + (InventoryColumnWidth - 18.0f) * 0.5f;
			if (Tick != nullptr)
			{
				AddAt(Canvas, CellX, RowY + 1.0f, 18.0f, 15.0f, SNew(SImage).Image(Tick));
			}
			else
			{
				AddAt(Canvas, CellX, RowY + 1.0f, 18.0f, 15.0f,
					MakePageText(TEXT("X"), 10, PaperInk, ETextJustify::Center, true, false));
			}
		}

		++Row;
	}

	AddAt(Canvas, InventoryDoneX, InventoryDoneY, ShellButtonWidth, ShellButtonHeight,
		MakeArtButton(ShellButtonStrip, ButtonFrameCount, TEXT("Done"),
			FOnClicked::CreateSP(this, &SSimCopterHangarMenu::HandleShowPage, EPage::Hangar)));
}

// --- handlers --------------------------------------------------------------------------------

FReply SSimCopterHangarMenu::HandleShowPage(const EPage NewPage)
{
	ShowPage(NewPage);
	return FReply::Handled();
}

FReply SSimCopterHangarMenu::HandleDone()
{
	OnDoneRequested.ExecuteIfBound();
	return FReply::Handled();
}

FReply SSimCopterHangarMenu::HandleSelectCatalogRow(const int32 NewRow)
{
	CatalogRow = NewRow;
	if (NewRow != INDEX_NONE) LastAircraftCatalogRow = NewRow;
	StatusText.Reset();
	RebuildPage();
	if (CatalogTabWidgets.IsValidIndex(CatalogRow)) SetControllerSelection(CatalogTabWidgets[CatalogRow]);
	return FReply::Handled();
}

FReply SSimCopterHangarMenu::HandleSelectUpgradeRow(const int32 NewRow)
{
	UpgradeRow = FMath::Clamp(NewRow, 0, SimCopterHangarShop::UpgradeRowCount - 1);
	StatusText.Reset();
	return FReply::Handled();
}

void SSimCopterHangarMenu::SelectEquipmentAction()
{
	// Confirming an item moves focus only. A fresh press performs a transaction.
	// Read the new row directly; Slate may still have the previous row's enabled
	// attributes cached until the next prepass.
	const auto State=SimCopterHangarShop::GetUpgradeRowState(Shop,UpgradeRow);
	const int32 ActionStart=2+SimCopterHangarShop::UpgradeRowCount;
	const int32 Index=ActionStart+(State.bCanBuy ? 0 : State.bCanSell ? 1 : 2);
	if (ControllerFocusableWidgets.IsValidIndex(Index)) SetControllerSelection(ControllerFocusableWidgets[Index]);
}

FReply SSimCopterHangarMenu::HandleActivateUpgradeRow(const int32 NewRow)
{
	HandleSelectUpgradeRow(NewRow);
	SelectEquipmentAction();
	return FReply::Handled();
}

int32 SSimCopterHangarMenu::GetOwnedEquipmentMask() const
{
	int32 Mask = 0;
	for (int32 Row=0; Row<SimCopterHangarShop::UpgradeRowCount; ++Row)
		if (SimCopterHangarShop::GetUpgradeRowState(Shop,Row).bOwned)
			Mask |= SimCopterHelicopterRegistry::GetToolCareerBit(SimCopterHangarShop::GetToolForUpgradeRow(Row));
	return Mask;
}

FReply SSimCopterHangarMenu::CycleEquipmentHelicopter(int32 Direction)
{
	int32 Row = SimCopterHangarLayout::GetCatalogRowForTypeIndex(EquipmentPreviewType);
	for (int32 Attempt=0; Attempt<CatalogTabCount; ++Attempt)
	{
		Row = (Row + Direction + CatalogTabCount) % CatalogTabCount;
		if (!SimCopterHangarShop::GetHelicopterRowState(Shop,Row).bMystery)
		{
			EquipmentPreviewType = SimCopterHangarLayout::GetTypeIndexForCatalogRow(Row);
			break;
		}
	}
	return FReply::Handled();
}

FReply SSimCopterHangarMenu::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Page == EPage::Catalog && CatalogRow != INDEX_NONE && TryScrollReadingPanel(Event.GetKey()))
	{
		return FReply::Handled();
	}
	if (Page == EPage::Catalog && CatalogRow != INDEX_NONE)
	{
		const FKey Key=Event.GetKey();
		const EUINavigation Direction=Key==EKeys::Left ? EUINavigation::Left : Key==EKeys::Right ? EUINavigation::Right :
			Key==EKeys::Up ? EUINavigation::Up : Key==EKeys::Down ? EUINavigation::Down : EUINavigation::Invalid;
		if (Direction != EUINavigation::Invalid)
		{
			SSimCopterNavigableMenu::OnNavigation(Geometry,FNavigationEvent(FModifierKeysState(),Event.GetUserIndex(),Direction,ENavigationGenesis::Controller));
			return FReply::Handled();
		}
	}
	if (Page == EPage::Catalog && CatalogRow == INDEX_NONE)
	{
		const FKey Key=Event.GetKey();
		// B leaves Upgrades for the aircraft list (OnKeyDown); it no longer stops on the item row.
		if (Key==EKeys::Left
 || Key==EKeys::Gamepad_LeftShoulder) return CycleEquipmentHelicopter(-1);
		if (Key==EKeys::Right || Key==EKeys::Gamepad_RightShoulder) return CycleEquipmentHelicopter(1);
		if (Key==EKeys::Up || Key==EKeys::Down)
			return HandleSelectUpgradeRow((UpgradeRow+(Key==EKeys::Up ? -1 : 1)+SimCopterHangarShop::UpgradeRowCount)%SimCopterHangarShop::UpgradeRowCount);
	}
	return SSimCopterNavigableMenu::OnPreviewKeyDown(Geometry,Event);
}

bool SSimCopterHangarMenu::IsControllerNavigationCandidate(const TSharedPtr<SWidget>& From,
	const TSharedRef<SWidget>& To, FVector2D Direction) const
{
	if (Page != EPage::Catalog) return true;
	const int32 FromTab = CatalogTabWidgets.IndexOfByKey(From);
	const int32 ToTab = CatalogTabWidgets.IndexOfByKey(To);
	// Aircraft occupy one row; the two links underneath are reached deliberately with Down.
	// Apply this before the shared spatial search, for both D-pad and stick navigation.
	if (ToTab >= CivilianCatalogTabCount && Direction.Y <= 0) return false;
	if (FromTab >= 0 && FromTab < CivilianCatalogTabCount && Direction.X != 0)
		return ToTab >= 0 && ToTab < CivilianCatalogTabCount;
	return true;
}

void SSimCopterHangarMenu::OnControllerSelectionChanged(int32 Index)
{
	// The equipment page's two arrow buttons precede its always-enabled item buttons.
	if (Page==EPage::Catalog && CatalogRow==INDEX_NONE && Index>=2 && Index<2+SimCopterHangarShop::UpgradeRowCount)
		HandleSelectUpgradeRow(Index-2);
}

FReply SSimCopterHangarMenu::HandleBuy()
{
	const bool bUpgrades = CatalogRow < 0 || CatalogRow >= CatalogTabCount;
	FString Message;
	if (bUpgrades)
	{
		SimCopterHangarShop::BuyUpgrade(Shop, UpgradeRow, Message);
	}
	else
	{
		SimCopterHangarShop::BuyHelicopter(Shop, CatalogRow, Message);
	}

	StatusText = Message;
	RebuildPage();
	return FReply::Handled();
}

FReply SSimCopterHangarMenu::HandleSell()
{
	const bool bUpgrades = CatalogRow < 0 || CatalogRow >= CatalogTabCount;
	FString Message;
	if (bUpgrades)
	{
		SimCopterHangarShop::SellUpgrade(Shop, UpgradeRow, Message);
	}
	else
	{
		SimCopterHangarShop::SellHelicopter(Shop, CatalogRow, Message);
	}

	StatusText = Message;
	RebuildPage();
	return FReply::Handled();
}

FReply SSimCopterHangarMenu::HandleSetLogSort(const ELogSort NewSort)
{
	LogSort = NewSort;
	RebuildPage();
	return FReply::Handled();
}

FReply SSimCopterHangarMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const int32 CheatType = SimCopterCheats::AircraftTypeForKey(InKeyEvent.GetKey());
	if (Page == EPage::Catalog && CatalogRow != INDEX_NONE && CheatType != INDEX_NONE &&
		Shop.Career.IsValid() && Shop.Career->Cheats.bCEO)
	{
		if (!InKeyEvent.IsRepeat())
		{
			SimCopterHangarShop::DeliverCheatHelicopter(Shop, CheatType, StatusText);
			RebuildPage();
		}
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right || InKeyEvent.GetKey() == EKeys::Escape)
	{
		// Upgrades is a sub-page of the catalog: back steps out to the aircraft list first,
		// and a second back closes the catalog.
		if (Page == EPage::Catalog && CatalogRow == INDEX_NONE)
		{
			return InKeyEvent.IsRepeat() ? FReply::Handled() : HandleSelectCatalogRow(LastAircraftCatalogRow);
		}
		if (InKeyEvent.IsRepeat()) return FReply::Handled();
		return Page == EPage::Hangar

			? HandleDone()
			: HandleShowPage(EPage::Hangar);
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}
