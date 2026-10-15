#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "GameFramework/InputSettings.h"
#include "Components/InputComponent.h"
#include "Flight/SimCopterControllerInput.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterControllerContextsTest, "SimCopter.Controller.Contexts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterControllerContextsTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	ASimCopterHelicopterPawn* Pawn = World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Input = NewObject<UInputComponent>(Pawn);
	Pawn->SetupPlayerInputComponent(Input);
	const auto Key = [&](FKey Button, EInputEvent Event = IE_Pressed)
	{
		for (auto& Binding : Input->KeyBindings)
			if (Binding.Chord.Key == Button && Binding.KeyEvent == Event) Binding.KeyDelegate.Execute(Button);
	};
	Key(EKeys::Gamepad_FaceButton_Bottom);
	TestTrue(TEXT("Actual A binding holds the selected tool action"), Pawn->bPrimaryToolUseHeld);
	TestFalse(TEXT("A no longer climbs"), Pawn->bControllerClimbHeld);
	Key(EKeys::Gamepad_FaceButton_Bottom, IE_Released);
	TestFalse(TEXT("Releasing A stops continuous tools"), Pawn->bPrimaryToolUseHeld);
	Key(EKeys::Gamepad_FaceButton_Bottom);
	Key(EKeys::Gamepad_RightThumbstick);
	TestFalse(TEXT("Opening options stops a held tool"), Pawn->bPrimaryToolUseHeld);
	Pawn->MissionPassengerSlots.AddDefaulted();
	const FVector2D PassengerDirection = SimCopterControllerInput::GetRadialSlotDirection(Pawn->GetControllerPassengersSlot(), Pawn->GetControllerToolWheelSlotCount());
	Pawn->ControllerRightX(PassengerDirection.X); Pawn->ControllerRightY(PassengerDirection.Y);
	Pawn->UpdateControllerRadialSelection();
	Pawn->ControllerRightX(0); Pawn->ControllerRightY(0);
	Key(EKeys::Gamepad_RightThumbstick, IE_Released);
	TestTrue(TEXT("Releasing R3 selects Passengers from Tools"), Pawn->ControllerMode == ESimCopterControllerMode::PassengerSelect);
	Pawn->ControllerToolWheelReleased();
	TestTrue(TEXT("Releasing LB leaves passengers open"), Pawn->ControllerMode == ESimCopterControllerMode::PassengerSelect);
	Pawn->ControllerPassengerSlot = 0;
	Pawn->ControllerPrimaryPressed();
	TestTrue(TEXT("A selects the passenger action"), Pawn->ControllerMode == ESimCopterControllerMode::PassengerConfirm);
	TestFalse(TEXT("Passenger A never climbs"), Pawn->bControllerClimbHeld);
	Pawn->ControllerPassengerConfirmChoice = 1;
	Pawn->ControllerPrimaryPressed();
	TestTrue(TEXT("A confirms the chosen Cancel action"), Pawn->ControllerMode == ESimCopterControllerMode::PassengerSelect);
	Pawn->ControllerCancelPressed();
	TestTrue(TEXT("B exits passengers"), Pawn->ControllerMode == ESimCopterControllerMode::None);
	TestFalse(TEXT("B used to close a menu never descends"), Pawn->bControllerDescendHeld);
	Pawn->ControllerCancelReleased();
	Pawn->ControllerCancelPressed();
	TestTrue(TEXT("Fresh B holds descent in flight"), Pawn->bControllerDescendHeld);
	Pawn->ControllerCancelReleased();
	Pawn->ControllerDispatchWheelPressed();
	Pawn->ControllerCancelPressed();
	Pawn->ControllerDispatchWheelReleased();
	TestTrue(TEXT("Cancel plus release leaves dispatch closed"), Pawn->ControllerMode == ESimCopterControllerMode::None);
	Pawn->ControllerDispatchWheelPressed();
	TestEqual(TEXT("Wheel opens with no implicit dispatch"), Pawn->ControllerRadialIndex, INDEX_NONE);
	Pawn->ControllerRightY(-1.0f);
	Pawn->UpdateControllerRadialSelection();
	TestEqual(TEXT("Pointing up highlights Fire Truck"), Pawn->ControllerRadialIndex, 0);
	Pawn->ControllerRightY(0.0f);
	Pawn->UpdateControllerRadialSelection();
	TestEqual(TEXT("Centring retains the old dispatch highlight"), Pawn->ControllerRadialIndex, 0);
	Pawn->ControllerDispatchWheelReleased();
	TestTrue(TEXT("Confirming the retained choice closes dispatch"),
		Pawn->ControllerMode == ESimCopterControllerMode::None);
	Pawn->ControllerDispatchWheelPressed();
	TestEqual(TEXT("Opening a fresh wheel clears the previous choice"), Pawn->ControllerRadialIndex, INDEX_NONE);
	Pawn->ControllerRightY(-1.0f);
	Pawn->UpdateControllerRadialSelection();
	Pawn->ControllerRightY(0.0f);
	Pawn->ControllerPrimaryPressed();
	TestTrue(TEXT("A confirms dispatch without holding a bumper"),
		Pawn->ControllerMode == ESimCopterControllerMode::None);
	TestFalse(TEXT("Held dispatch A does not become climb"), Pawn->bControllerClimbHeld);
	Pawn->ControllerDispatchWheelReleased();
	TestTrue(TEXT("Later RB release remains outside dispatch context"),
		Pawn->ControllerMode == ESimCopterControllerMode::None);
	Pawn->ControllerPrimaryReleased();
	Pawn->ControllerRightX(0); Pawn->ControllerRightY(0);
	Key(EKeys::Gamepad_RightThumbstick);
	TestTrue(TEXT("R3 opens Tools"), Pawn->ControllerMode == ESimCopterControllerMode::ToolWheel);
	Key(EKeys::Gamepad_FaceButton_Left);
	TestTrue(TEXT("X alone switches to Dispatch"), Pawn->ControllerMode == ESimCopterControllerMode::DispatchWheel);
	Key(EKeys::Gamepad_FaceButton_Left);
	TestTrue(TEXT("X alone switches back to Tools"), Pawn->ControllerMode == ESimCopterControllerMode::ToolWheel);
	Key(EKeys::Gamepad_RightThumbstick, IE_Released);
	TestTrue(TEXT("R3 release confirms Tools"), Pawn->ControllerMode == ESimCopterControllerMode::None);
	TestFalse(TEXT("Confirming Tools cannot climb"), Pawn->bControllerClimbHeld);
	TestFalse(TEXT("Confirming an option cannot also use a tool"), Pawn->bPrimaryToolUseHeld);
	Pawn->ControllerPrimaryReleased();
	Pawn->bControllerCameraAdjustHeld=true;
	Pawn->ControllerCancelPressed();
	TestFalse(TEXT("R3+B exit shortcut cannot descend"), Pawn->bControllerDescendHeld);
	Pawn->bControllerCameraAdjustHeld=false;
	Pawn->GroundClearanceCm = 0;
	Pawn->bIsLanded = false;
	TestTrue(TEXT("Pilot can deliberately exit an airborne helicopter"), Pawn->CanExitHelicopter());
	Pawn->bIsLanded = true;
	TestTrue(TEXT("Landed helicopter can offer exit"), Pawn->CanExitHelicopter());
	Pawn->ControllerMode = ESimCopterControllerMode::None;
	Pawn->bSpotlightTargetFrozen = true; // Test aim accumulation without a city trace.
	Pawn->SpotlightAimPitchInput = Pawn->SpotlightAimYawInput = 0;
	Pawn->SpotlightAimPitch1616 = Pawn->SpotlightAimYaw1616 = 0;
	for (bool bCamera : {false, true})
	{
		Pawn->bControllerCameraAdjustHeld = bCamera;
		Pawn->ControllerDPadUpPressed();
		Pawn->ControllerDPadRightPressed();
		Pawn->UpdateControllerToolManipulation();
		Pawn->UpdateSpotlightTarget(0.05f);
		TestEqual(TEXT("D-pad never changes spotlight pitch"), Pawn->SpotlightAimPitch1616, 0);
		TestEqual(TEXT("D-pad never changes spotlight yaw"), Pawn->SpotlightAimYaw1616, 0);
		Pawn->ControllerDPadUpReleased();
		Pawn->ControllerDPadRightReleased();
	}
	Pawn->SpotlightAimPitchInput = 1;
	Pawn->UpdateSpotlightTarget(0.05f);
	TestTrue(TEXT("Keyboard spotlight input still adjusts aim"), Pawn->SpotlightAimPitch1616 != 0);
	Pawn->bControllerCameraAdjustHeld = false;
	Pawn->SelectedTool = ESimCopterHelicopterTool::Megaphone;
	Pawn->SelectedMegaphoneMessage = static_cast<ESimCopterMegaphoneMessage>(0);
	Pawn->ControllerDPadUpPressed();
	Pawn->ControllerDPadDownPressed();
	TestEqual(TEXT("Vertical D-pad does not change megaphone message"), static_cast<int32>(Pawn->SelectedMegaphoneMessage), 0);
	Pawn->ControllerDPadRightPressed();
	TestEqual(TEXT("Right selects next message"), static_cast<int32>(Pawn->SelectedMegaphoneMessage), 1);
	Pawn->ControllerDPadLeftPressed();
	TestEqual(TEXT("Left selects previous message"), static_cast<int32>(Pawn->SelectedMegaphoneMessage), 0);
	Pawn->ControllerDPadLeftPressed();
	TestEqual(TEXT("Left wraps to last message"), static_cast<int32>(Pawn->SelectedMegaphoneMessage), static_cast<int32>(ESimCopterMegaphoneMessage::Count) - 1);
	// Drive the real bumper bindings through the flight input adapter at cruising pitch.
	Pawn->MissionPassengerSlots.Reset();
	using namespace SimCopterFixed;
	for (int32 Type=0; Type<9; ++Type) for (bool Boost : {false,true})
	{
		TestTrue(TEXT("Cruise test loads the requested airframe"),Pawn->SwitchHelicopterModel(Type));
		Pawn->ResetTransientInputState(); Pawn->bEngineRunning=true;
		Pawn->ControllerLeftY(1); Pawn->ControllerLeftX(0);
		auto Cruise=Pawn->GetFlightModel(); Cruise.ResetOnSurface(0,0,0);
		Cruise.State=ESimCopterFlightState::Flying; Cruise.Altitude=FromFloat(500);
		Cruise.RotorSpeed=FSimCopterFlightModel::RotorTopSpeed; Cruise.Heading=FromFloat(1800);
		Cruise.PitchTarget=Cruise.PitchSmoothed=Cruise.ForwardSpeed=Cruise.Tuning.MaxPitch;
		for (bool ForwardHeld : {true,false}) for (bool Right : {false,true})
		{
			Pawn->ControllerLeftY(ForwardHeld ? 1 : 0);
			const FKey Bumper=Right ? EKeys::Gamepad_RightShoulder : EKeys::Gamepad_LeftShoulder;
			Key(Bumper); auto FlightInput=Pawn->BuildFlightInputs(); FlightInput.bTravelBoost=Boost;
			TestTrue(TEXT("Pressed bumper reaches coordinated turn"),Right ? FlightInput.bTurnRightKey : FlightInput.bTurnLeftKey);
			auto Model=Cruise; FSimCopterFlightEvents Events; FSimCopterFlightEnvironment Env;
			for (int32 Frame=0; Frame<60; ++Frame) Model.Step(1.f/60,FlightInput,Env,Events);
			const float Change=ToFloat(Model.Heading-Cruise.Heading)/10;
			TestTrue(*FString::Printf(TEXT("Type %d %s turns at cruise, forward=%d boost=%d (%.2f deg)"),Type,Right?TEXT("RB"):TEXT("LB"),ForwardHeld,Boost,Change),Right ? Change>5 : Change<-5);
			Key(Bumper,IE_Released);
		}
	}
	for (bool RightFirst : {false,true})
	{
		Pawn->ResetTransientInputState(); Pawn->bEngineRunning=true; Pawn->ControllerLeftY(1);
		const FKey First=RightFirst ? EKeys::Gamepad_RightShoulder : EKeys::Gamepad_LeftShoulder;
		const FKey Second=RightFirst ? EKeys::Gamepad_LeftShoulder : EKeys::Gamepad_RightShoulder;
		Key(First); Key(Second);
		TestTrue(TEXT("Overlapping turn bumpers never open the tool wheel"),Pawn->ControllerMode==ESimCopterControllerMode::None);
		Key(First,IE_Released);
		const auto InputAfterSwitch=Pawn->BuildFlightInputs();
		TestTrue(TEXT("Remaining bumper turns immediately after direction change"),RightFirst ? InputAfterSwitch.bTurnLeftKey : InputAfterSwitch.bTurnRightKey);
		Key(Second,IE_Released);
	}
	World->DestroyWorld(false);

	// The hint must retain every keyboard alternative as well as the controller shortcut.
	UInputSettings* Settings = UInputSettings::GetInputSettings();
	const FInputAxisKeyMapping Extra(TEXT("SimCopterCollective"), EKeys::F10, 1.0f);
	Settings->AddAxisMapping(Extra, false);
	const FString Hint = ASimCopterHelicopterPawn::GetCollectiveUpKeyDisplayName().ToString();
	TestTrue(TEXT("Hint includes a second live climb binding"), Hint.Contains(EKeys::F10.GetDisplayName().ToString()));
	TestFalse(TEXT("Keyboard hint excludes controller A shortcut"), Hint.Contains(TEXT(" / A")));
	const FString PadHint = ASimCopterHelicopterPawn::GetCollectiveUpKeyDisplayName(true).ToString();
	TestEqual(TEXT("Controller climb hint names the trigger, not tool A"), PadHint, FString(TEXT("RT")));
	TestFalse(TEXT("Controller hint excludes keyboard alternatives"), PadHint.Contains(TEXT("F10")));
	TestEqual(TEXT("Controller exit hint names Y"), ASimCopterHelicopterPawn::GetExitHelicopterKeyDisplayName(true).ToString(), FString(TEXT("Y")));
	Settings->RemoveAxisMapping(Extra, false);
	return true;
}
#endif
