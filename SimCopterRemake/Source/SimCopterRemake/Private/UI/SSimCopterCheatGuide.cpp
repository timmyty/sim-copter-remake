#include "SSimCopterCheatGuide.h"
#include "InputCoreTypes.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SimCopterCheatGuide"
using namespace SimCopterFrontEnd;

TConstArrayView<SSimCopterCheatGuide::FEntry> SSimCopterCheatGuide::GetEntries()
{
	// Keep these examples executable and complete; the guide test checks them against Parse.
	static const FEntry Entries[] = {
		{ TEXT("superpowermultiply"), TEXT("Hold Shift for helicopter turbo, or to become a fast dog while on foot. Release Shift to return to the pilot.") },
		{ TEXT("Shields up"), TEXT("Toggle protection from aircraft collision and fire damage. Does not repair existing damage.") },
		{ TEXT("Gas does grow on trees"), TEXT("Refill your current aircraft and toggle unlimited fuel.") },
		{ TEXT("I'm the CEO of McDonnell Douglas"), TEXT("Enable free aircraft delivery. Open the hangar helicopter catalog, then press 1-9: 1 Jet Ranger, 2 MD 500, 3 Apache, 4 Bell 212, 5 Schweizer 300, 6 Agusta, 7 Dauphin, 8 MD Explorer, 9 MD 520. Requires a clear pad; owned types cannot be duplicated.") },
		{ TEXT("There's no place like home"), TEXT("While on foot, teleport to the hangar. Your aircraft stays where you left it.") },
		{ TEXT("I love my helicopter"), TEXT("While on foot, return to your helicopter.") },
		{ TEXT("Been there, done that"), TEXT("Complete the current career city's score target. Return to the hangar to advance.") },
		{ TEXT("The map, please"), TEXT("Toggle the city map while on foot.") },
		{ TEXT("Warp me to career: 12"), TEXT("Travel to a career level; replace 12 with a number from 1 to 30. Retains cash, equipment and aircraft ownership.") },
		{ TEXT("Give me bucks or give me death: 1000"), TEXT("Gamble for cash; replace 1000 with an amount from 1 to 49999. Failure chance: 2% for 1-16666, 4% for 16667-33333, and 7% for 33334-49999. A win awards the requested amount. Losing ends the current game and returns to the main menu; existing saves remain.") },
		{ TEXT("A megaphone in the hand is worth two in the bush"), TEXT("Toggle on-foot megaphone commands with F6-F10.") },
		{ TEXT("Out on a Sunday drive"), TEXT("Toggle a camera riding a city vehicle.") },
		{ TEXT("PAMCAREYGOLDMAN"), TEXT("Reveal hidden portrait faces on applicable city signs.") },
		{ TEXT("Radioactivity"), TEXT("Create a nuclear flash and blast, destroying roughly three quarters of eligible buildings. Roads, airport, hospital, police and fire services survive.") },
		{ TEXT("Stop and ask for directions"), TEXT("Export the city building grid to Saved/dump_bm.txt. The HUD shows the full file path.") },
		{ TEXT("Gort"), TEXT("Play the original alien conversation with voices and subtitles. Esc, controller B or Return to game resumes play; the sequence also ends automatically.") },
		{ TEXT("HSI"), TEXT("HSI or hsi toggles the Humanist Superintelligence music video on the city's drive-in screens, with the usual cheat on/off sound.") },
		{ TEXT("Lights, Camera, Action!"), TEXT("Toggle the supplied original SimCopter movie on the city's drive-in screens.") },
		{ TEXT("Play video: My Movie.mp4"), TEXT("Play an MP4 from the DriveInVideos folder beside the game launcher. Replace My Movie.mp4 with the file name.") },
		{ TEXT("Stop video"), TEXT("Stop the drive-in picture and sound.") },
	};
	return MakeArrayView(Entries);
}

void SSimCopterCheatGuide::Construct(const FArguments& Args)
{
	OnClosed = Args._OnClosed;
	SAssignNew(Scroll, SScrollBox).AnimateWheelScrolling(false);
	for (const FEntry& Entry : GetEntries())
	{
		Scroll->AddSlot().Padding(0, 0, 12, 14)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Entry.Code)).Font(PageFont(18, true))
				.ColorAndOpacity(ItemSelectedColor).WrapTextAt(490)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
			[
				SNew(STextBlock).Text(FText::FromString(Entry.Effect)).Font(PageFont(16))
				.ColorAndOpacity(PlateTextColor).WrapTextAt(490)
			]
		];
	}
	auto Canvas = SNew(SConstraintCanvas);
	AddAt(Canvas, FRect{40, 24, 600, 456}, MakeFallbackPlate(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 10, 16, 6)
		[
			SNew(STextBlock).Text(LOCTEXT("Title", "Cheat Codes"))
			.Font(PageFont(26, true)).ColorAndOpacity(ItemSelectedColor)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 0, 16, 12)
		[
			SNew(STextBlock).Text(LOCTEXT("Instructions", "Resume the game, then press Ctrl + Alt + X to enter a code. Match spelling, case and punctuation; HSI also accepts hsi. Enter toggles again to turn them off."))
			.Font(PageFont(16)).ColorAndOpacity(PlateTextColor).WrapTextAt(510)
		]
		+ SVerticalBox::Slot().FillHeight(1).Padding(16, 0, 16, 0)
		[ Scroll.ToSharedRef() ]
		+ SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 10)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(LOCTEXT("Scroll", "Scroll: wheel / arrows / D-pad\nBack: Esc / B"))
				.Font(PageFont(14)).ColorAndOpacity(PlateTextColor)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(ButtonWidth).HeightOverride(ButtonHeight)
				[ MakeButton(Args._Art, LOCTEXT("Back", "Back"), 18,
					FOnClicked::CreateLambda([this]() { OnClosed.ExecuteIfBound(); return FReply::Handled(); }),
					ButtonStyles, false) ]
			]
		]));
	ChildSlot [ MakeScaledScreen(Canvas) ];
}

FReply SSimCopterCheatGuide::OnPreviewKeyDown(const FGeometry&, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		if (!Event.IsRepeat()) OnClosed.ExecuteIfBound();
		return FReply::Handled();
	}
	if (Key == EKeys::Home) { Scroll->ScrollToStart(); return FReply::Handled(); }
	if (Key == EKeys::End) { Scroll->ScrollToEnd(); return FReply::Handled(); }
	float Delta = 0;
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up) Delta = -48;
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down) Delta = 48;
	if (Key == EKeys::PageUp) Delta = -240;
	if (Key == EKeys::PageDown) Delta = 240;
	if (Delta != 0)
	{
		Scroll->SetScrollOffset(FMath::Max(0.0f, Scroll->GetScrollOffset() + Delta));
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
