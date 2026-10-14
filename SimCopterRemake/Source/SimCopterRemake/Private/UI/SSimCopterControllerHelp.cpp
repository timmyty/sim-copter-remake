#include "SSimCopterControllerHelp.h"
#include "SSimCopterStatusPlate.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterRegistry.h"
#include "Game/SimCopterPlayerController.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Kismet/GameplayStatics.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

using namespace SimCopterControllerHelp;

FState SSimCopterControllerHelp::ReadState(APawn* Pawn, bool bIncludeInstructions)
{
	FState State;
	if (!Pawn) return State;
	const auto* PC = Cast<ASimCopterPlayerController>(Pawn->GetController());
	State.bExpanded = PC && PC->IsControllerHelpExpanded();
	State.bKeyboard = !PC || !PC->IsUsingGamepadInput();
	State.bTurboEnabled = PC && PC->IsControllerTurboEnabled();
	const auto* Aircraft = Cast<ASimCopterHelicopterPawn>(Pawn);
	State.bTurboAvailable = (Aircraft && !Aircraft->IsFixedWingAircraft()) || Pawn->IsA<ASimCopterOnFootPawn>();
	if (Pawn->IsA<ASimCopterOnFootPawn>()) State.Context = EContext::OnFoot;
	if (!State.bExpanded || !bIncludeInstructions) return State;
	if (const auto* Helicopter = Cast<ASimCopterHelicopterPawn>(Pawn))
	{
		State.Instructions = Helicopter->GetToolInstructions();
		State.bHasPassengers = !Helicopter->GetMissionPassengerSlots().IsEmpty();
		State.bCanExit = Helicopter->CanExitHelicopter();
		const ESimCopterHelicopterTool Tool = Helicopter->GetActiveTool();
		State.bHasTool = Helicopter->IsToolAvailable(Tool);
		State.bRope = Tool == ESimCopterHelicopterTool::WaterBucket || Tool == ESimCopterHelicopterTool::RescueHarness;
		State.bMegaphone = Tool == ESimCopterHelicopterTool::Megaphone;
		State.Message = SimCopterHelicopterRegistry::GetMegaphoneMessageName(Helicopter->GetSelectedMegaphoneMessage());
		switch (Tool)
		{
		case ESimCopterHelicopterTool::CaptureCage: State.ToolAction = TEXT("Deploy cage / auto capture"); State.bRope=true; break;
		case ESimCopterHelicopterTool::WaterBucket: State.ToolAction = TEXT("Dump water bucket"); break;
		case ESimCopterHelicopterTool::WaterCannon: State.ToolAction = TEXT("Hold to spray water"); break;
		case ESimCopterHelicopterTool::TearGas: State.ToolAction = TEXT("Fire tear gas"); break;
		case ESimCopterHelicopterTool::ApacheMachineGun: State.ToolAction = TEXT("Hold to fire machine gun"); break;
		case ESimCopterHelicopterTool::ApacheMissile: State.ToolAction = TEXT("Fire missile"); break;
		default: State.ToolAction = TEXT("Use rescue harness"); break;
		}
		switch (Helicopter->GetControllerMode())
		{
		case ESimCopterControllerMode::ToolWheel: State.Context = EContext::ToolWheel; break;
		case ESimCopterControllerMode::DispatchWheel: State.Context = EContext::DispatchWheel; break;
		case ESimCopterControllerMode::PassengerSelect: State.Context = EContext::Passengers; break;
		case ESimCopterControllerMode::PassengerConfirm: State.Context = EContext::ConfirmPassenger; break;
		default: State.Context = Helicopter->IsControllerCameraAdjustHeld() ? EContext::Camera : EContext::Flight; break;
		}
		if (Helicopter->GetControllerMode() == ESimCopterControllerMode::None && Helicopter->GetAirOperations()->IsRemoteTaserActive()) State.Context = EContext::RemoteTaser;
	}
	else if (const auto* OnFoot = Cast<ASimCopterOnFootPawn>(Pawn))
	{
		State.Instructions = TEXT("Walk up to carry downed people. Return patients to a hospital with room for one medic.");
		State.Context = EContext::OnFoot;
		State.bCarryingPerson = OnFoot->IsCarryingMissionPerson();
		State.bCanBoard = OnFoot->CanBoardNearbyHelicopter();
	}
	if (const auto* Missions = Cast<ASimCopterMissionSystemActor>(UGameplayStatics::GetActorOfClass(Pawn, ASimCopterMissionSystemActor::StaticClass()));
		Missions && Missions->UsesCitywideMissionPlacement())
		State.Instructions += TEXT("\n") + Missions->GetCustomCityProgressionText();
	return State;
}

TSharedRef<SWidget> SSimCopterControllerHelp::ForPawn(TWeakObjectPtr<APawn> Pawn, float Scale)
{
	return SNew(SSimCopterControllerHelp).Scale(Scale)
		.StandaloneStatus(Pawn.IsValid() && Pawn->IsA<ASimCopterOnFootPawn>())
		.State_Lambda([Pawn] { return ReadState(Pawn.Get()); })
		.IconStyle_Lambda([Pawn]
		{
			const auto* PC = Pawn.IsValid() ? Cast<ASimCopterPlayerController>(Pawn->GetController()) : nullptr;
			return PC ? PC->GetControllerIconStyle() : EStyle::Xbox;
		})
		.Visibility_Lambda([Pawn]
		{
			const auto* PC = Pawn.IsValid() ? Cast<ASimCopterPlayerController>(Pawn->GetController()) : nullptr;
			return PC && (PC->IsControllerHelpExpanded() || Pawn->IsA<ASimCopterOnFootPawn>()) &&
				!PC->IsPhotoModeActive() && !PC->IsPaused() && GEngine && GEngine->GameViewport &&
				!GEngine->GameViewport->IgnoreInput() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		});
}

void SSimCopterControllerHelp::Construct(const FArguments& Args)
{
	State = Args._State;
	IconStyle = Args._IconStyle;
	Scale = Args._Scale;
	auto Content = SNew(SVerticalBox);
	Content->AddSlot().AutoHeight()
	[
		SNew(SBorder).Visibility_Lambda([this] { return State.Get().bExpanded ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor(0.015f, 0.02f, 0.03f, 0.78f)).Padding(FMargin(10, 6) * Scale)
		[
			SAssignNew(Rows, SVerticalBox)
		]
	];
	// On foot there is no minimap or Turbo indicator; retain only the help shortcut.
	if (Args._StandaloneStatus)
	{
		Content->AddSlot().AutoHeight().HAlign(HAlign_Left)
		[SNew(SSimCopterStatusPlate).State(State).Scale(2 * Scale)];
	}
	ChildSlot[Content];
	Refresh();
}

const FSlateBrush* SSimCopterControllerHelp::GetIcon(const FString& Name)
{
	const FString Key = FString(IconStyle.Get() == EStyle::PlayStation ? TEXT("playstation_") : TEXT("xbox_")) + Name;
	TSharedPtr<FSlateDynamicImageBrush>& Brush = Icons.FindOrAdd(Key);
	if (!Brush)
	{
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Slate/ControllerIcons") / (Key + TEXT(".png")));
		Brush = MakeShared<FSlateDynamicImageBrush>(FName(*Path), FVector2D(32, 32));
	}
	return Brush.Get();
}

void SSimCopterControllerHelp::Refresh()
{
	const auto Current = State.Get();
	const TArray<FPrompt> Prompts = BuildPrompts(Current);
	FString Layout = FString::FromInt(static_cast<int32>(IconStyle.Get()));
	for (const FPrompt& Prompt : Prompts) Layout += TEXT("|") + Prompt.Icon + TEXT(":") + Prompt.Label;
	if (Current.bExpanded) Layout += Current.Instructions;
	if (Layout == PreviousLayout) return;
	PreviousLayout = MoveTemp(Layout);
	Rows->ClearChildren();
	const auto MakeRow = [this, &Prompts](const FPrompt& Prompt) -> TSharedRef<SWidget>
	{
		const bool bToggle = Prompt.Icon == TEXT("l3") && !Prompt.Label.StartsWith(TEXT("+ D-pad"));
		return SNew(SBox).Padding(FMargin(0, bToggle && Prompts.Num() > 1 ? 6 * Scale : 0))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).Visibility(Prompt.Icon.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible).WidthOverride(32 * Scale).HeightOverride(32 * Scale)
				[SNew(SImage).Image(Prompt.Icon.IsEmpty() ? nullptr : GetIcon(Prompt.Icon))]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8 * Scale, 0)
			[
				SNew(STextBlock).Text(FText::FromString(Prompt.Label))
				.Font(FCoreStyle::GetDefaultFontStyle(bToggle ? TEXT("Bold") : TEXT("Regular"), FMath::RoundToInt(12 * Scale)))
				.ColorAndOpacity(bToggle ? FLinearColor(1, 0.83f, 0.42f) : FLinearColor::White)
			]
		];
	};
	// Longer expanded lists grow sideways so the flight basics still fit above the map.
	if (Prompts.Num() > 9)
	{
		TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
		TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);
		const int32 Split = (Prompts.Num() + 1) / 2;
		for (int32 Index = 0; Index < Prompts.Num(); ++Index)
			(Index < Split ? Left : Right)->AddSlot().AutoHeight()[MakeRow(Prompts[Index])];
		Rows->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[Left]
			+ SHorizontalBox::Slot().AutoWidth().Padding(20 * Scale, 0, 0, 0)[Right]
		];
	}
	else for (const FPrompt& Prompt : Prompts) Rows->AddSlot().AutoHeight()[MakeRow(Prompt)];
	if (Current.bExpanded && !Current.Instructions.IsEmpty())
		Rows->AddSlot().AutoHeight().Padding(0, 8 * Scale)
		[SNew(STextBlock).Text(FText::FromString(Current.Instructions)).WrapTextAt(650 * Scale)
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), FMath::RoundToInt(12 * Scale)))];
}

void SSimCopterControllerHelp::Tick(const FGeometry& Geometry, double Time, float Delta)
{
	SCompoundWidget::Tick(Geometry, Time, Delta);
	Refresh();
}
