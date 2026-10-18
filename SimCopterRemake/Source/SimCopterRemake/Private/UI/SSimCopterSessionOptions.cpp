#include "SSimCopterSessionOptions.h"
#include "SimCopterFrontEndPage.h"
#include "Game/SimCopterSettings.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SimCopterSessionOptions"

void SSimCopterSessionOptions::Construct(const FArguments& Args)
{
	using namespace SimCopterFrontEnd;
	OnBack = Args._OnBack;
	const TWeakObjectPtr<USimCopterSettings> Settings = Args._Settings;
	ChildSlot
	[
		MakeScaledScreen(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
		[SNew(SBox).WidthOverride(480)
		[MakeFallbackPlate(SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(26, 24, 26, 16)
		[SNew(STextBlock).Text(LOCTEXT("Title", "Game Options"))
		.Font(PageFont(22, true)).ColorAndOpacity(PlateTextColor)]
		+ SVerticalBox::Slot().AutoHeight().Padding(26, 0, 26, 16)
		[SNew(SCheckBox)
		.IsChecked_Lambda([Settings] { return !Settings.IsValid() || Settings->WantsRemakeFeatures() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([Settings](ECheckBoxState State)
		{
			if (Settings.IsValid())
			{
				Settings->SetRemakeFeaturesPreference(State == ECheckBoxState::Checked);
				Settings->SaveConfig();
			}
		})
		[SNew(STextBlock).Text_Lambda([Settings]
		{ return !Settings.IsValid() || Settings->WantsRemakeFeatures()
			? LOCTEXT("Enabled", "Extra Remake Features: On") : LOCTEXT("Disabled", "Extra Remake Features: Off"); })
		.Font(PageFont(16, true)).ColorAndOpacity(PlateTextColor)]]
		+ SVerticalBox::Slot().AutoHeight().Padding(26, 0, 26, 22)
		[SNew(STextBlock).WrapTextAt(428).LineHeightPercentage(1.2f)
		.Text(LOCTEXT("Explanation", "On adds tasers, cargo recovery, helper aircraft, planes, parachutes, expanded rescue, baseball and progression.\n\nOff keeps original-style gameplay with bug fixes and visual improvements.\n\nYour selection is remembered. Change it here or in Game Options > Gameplay. Existing saves require the mode they were created with."))
		.Font(PageFont(13)).ColorAndOpacity(PlateTextColor)]
		+ SVerticalBox::Slot().AutoHeight().Padding(26, 0, 26, 24)
		[SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 8, 0)
		[SNew(SButton).HAlign(HAlign_Center).ContentPadding(12)
		.OnClicked_Lambda([Back=Args._OnBack] { Back.ExecuteIfBound(); return FReply::Handled(); })
		[SNew(STextBlock).Text(LOCTEXT("Back", "Back")).Font(PageFont(14))]]
		+ SHorizontalBox::Slot().FillWidth(1).Padding(8, 0, 0, 0)
		[SNew(SButton).HAlign(HAlign_Center).ContentPadding(12)
		.OnClicked_Lambda([Continue=Args._OnContinue] { Continue.ExecuteIfBound(); return FReply::Handled(); })
		[SNew(STextBlock).Text(LOCTEXT("Continue", "Continue")).Font(PageFont(14))]]]
		)]])
	];
}

FReply SSimCopterSessionOptions::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		OnBack.ExecuteIfBound();
		return FReply::Handled();
	}
	return SSimCopterNavigableMenu::OnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
