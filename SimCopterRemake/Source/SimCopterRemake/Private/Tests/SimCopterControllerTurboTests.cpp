#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerInput.h"
#include "Game/SimCopterCheats.h"
#include "Game/SimCopterPlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "../UI/SSimCopterMapPanel.h"
#include "Missions/SimCopterMissionSystemActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterControllerTurboTest,"SimCopter.Controller.TurboMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterControllerTurboTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* PC=World->SpawnActor<ASimCopterPlayerController>();
	PC->PlayerInput=NewObject<UPlayerInput>(PC);
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	PC->Possess(Heli); Heli->bEngineRunning=true;
	TestFalse(TEXT("Flight starts without turbo"),Heli->BuildFlightInputs().bTravelBoost);
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, true);
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, true, true);
	PC->HandleKeyboardTurboKey(EKeys::Up, true, true);
	TestFalse(TEXT("Shift waits for release to distinguish recall chords"), Heli->BuildFlightInputs().bTravelBoost);
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, false);
	TestTrue(TEXT("Shift release keeps flight turbo enabled"), Heli->BuildFlightInputs().bTravelBoost);
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, false);
	TestTrue(TEXT("Duplicate release does not toggle twice"), PC->IsControllerTurboEnabled());
	PC->HandleKeyboardTurboKey(EKeys::RightShift, true);
	PC->HandleKeyboardTurboKey(EKeys::F2, true);
	PC->HandleKeyboardTurboKey(EKeys::RightShift, false);
	TestTrue(TEXT("Responder recall leaves turbo unchanged"), PC->IsControllerTurboEnabled());
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("Controller can turn off keyboard turbo"), PC->IsControllerTurboEnabled());
	PC->HandleKeyboardTurboKey(EKeys::RightShift, true);
	PC->HandleKeyboardTurboKey(EKeys::RightShift, false);
	TestTrue(TEXT("Right Shift toggles turbo on"), PC->IsControllerTurboEnabled());
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, true);
	PC->HandleKeyboardTurboKey(EKeys::RightShift, true);
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, false);
	PC->HandleKeyboardTurboKey(EKeys::RightShift, false);
	TestFalse(TEXT("Two Shift keys form one toggle gesture"), PC->IsControllerTurboEnabled());
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, true);
	PC->PushPause(); PC->PopPause();
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, false);
	TestFalse(TEXT("Pause cancels pending keyboard turbo"), PC->IsControllerTurboEnabled());
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestTrue(TEXT("L3 reaches the real flight boost input"),Heli->BuildFlightInputs().bTravelBoost);
	TestFalse(TEXT("Ordinary L3 does not toggle help"),PC->IsControllerHelpExpanded());
	PC->HandleControllerLeftStickPressed(true);
	TestTrue(TEXT("Repeat events leave Turbo Mode on"),Heli->BuildFlightInputs().bTravelBoost);
	Heli->bControllerRadialHeld=true;
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestTrue(TEXT("R3+L3 toggles helicopter help"),PC->IsControllerHelpExpanded());
	TestTrue(TEXT("Help chord leaves Turbo Mode unchanged"),PC->IsControllerTurboEnabled());
	Heli->bControllerRadialHeld=false;
	PC->HandleControllerLeftStickPressed(false);
	TestTrue(TEXT("L3 then R3 consumes the help chord"), PC->HandleControllerRightStickPressed(false));
	TestFalse(TEXT("L3 then R3 also toggles help"), PC->IsControllerHelpExpanded());
	PC->HandleControllerRightStickPressed(true);
	TestFalse(TEXT("Holding the help chord does not repeat"), PC->IsControllerHelpExpanded());
	PC->HandleControllerLeftStickReleased();
	TestTrue(TEXT("L3-first help leaves Turbo enabled"), PC->IsControllerTurboEnabled());
	PC->ToggleControllerHelp();
	Heli->ControllerMode=ESimCopterControllerMode::ToolWheel;
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestTrue(TEXT("L3 does not change turbo while selecting a tool"),PC->IsControllerTurboEnabled());
	Heli->ControllerMode=ESimCopterControllerMode::None;
	if (auto* Cheats=SimCopterCheats::Get(Heli))
	{
		Cheats->bSuperpower=true;
		const auto Inputs=Heli->BuildFlightInputs();
		TestTrue(TEXT("L3 shares Shift's existing cheat turbo"),Inputs.bTurbo);
		TestFalse(TEXT("Normal boost never stacks with cheat turbo"),Inputs.bTravelBoost);
		Cheats->bSuperpower=false;
	}
	else AddError(TEXT("Cheat subsystem missing"));
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("Second L3 click restores normal flight"),Heli->BuildFlightInputs().bTravelBoost);
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	Heli->bEngineRunning=false;
	TestFalse(TEXT("Turbo cannot operate a stopped helicopter"),Heli->BuildFlightInputs().bTravelBoost);
	Heli->bEngineRunning=true;
	Heli->ResetTransientInputState();
	TestFalse(TEXT("Save restore input reset clears turbo"),PC->IsControllerTurboEnabled());
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
	Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay=false;
	PC->Possess(Pilot);
	TestFalse(TEXT("Exiting the helicopter clears turbo"),PC->IsControllerTurboEnabled());
	PC->HandleKeyboardTurboKey(EKeys::LeftShift, true); PC->HandleKeyboardTurboKey(EKeys::LeftShift, false);
	TestTrue(TEXT("Shift also toggles turbo on foot"), PC->IsControllerTurboEnabled());
	PC->Possess(Heli);
	TestFalse(TEXT("Boarding clears on-foot turbo"),PC->IsControllerTurboEnabled());
	Heli->MapPanel = SNew(SSimCopterMapPanel).Pawn(Heli);
	auto Map = Heli->MapPanel;
	PC->HandleControllerLeftStickPressed(false);
	TestFalse(TEXT("Holding L3 does not toggle turbo before a chord"),PC->IsControllerTurboEnabled());
	TestTrue(TEXT("L3 up is consumed before tool controls"),PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Up,false));
	TestFalse(TEXT("L3 up hides mission markers"),Map->Settings.bShowMissionBlips);
	PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Up,true);
	TestFalse(TEXT("Held D-pad cannot repeatedly toggle markers"),Map->Settings.bShowMissionBlips);
	PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("L3 map chord leaves turbo off"),PC->IsControllerTurboEnabled());
	TestTrue(TEXT("Chord release is consumed even after L3 release"),PC->HandleControllerMapKeyReleased(EKeys::Gamepad_DPad_Up));
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	PC->HandleControllerLeftStickPressed(false);
	PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Down,false);
	PC->HandleControllerMapKeyReleased(EKeys::Gamepad_DPad_Down);
	PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("L3 down hides service markers"),Map->Settings.bShowServiceBlips);
	TestTrue(TEXT("L3 map chord leaves enabled turbo on"),PC->IsControllerTurboEnabled());
	PC->ResetControllerTurbo();
	Heli->bControllerRadialHeld=true;
	const auto Dpad = [&](FKey Key) { TestTrue(TEXT("Map chord consumes D-pad"),PC->HandleControllerMapKeyPressed(Key,false)); PC->HandleControllerMapKeyReleased(Key); };
	for (int32 I=0;I<8;++I) Dpad(EKeys::Gamepad_DPad_Up);
	TestEqual(TEXT("R3 up zooms to maximum"),Map->Settings.Zoom,SimCopterMap::MaxZoom);
	for (int32 I=0;I<8;++I) Dpad(EKeys::Gamepad_DPad_Down);
	TestEqual(TEXT("R3 down zooms to minimum"),Map->Settings.Zoom,0);
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	struct FMapWorld : SimCopterMissions::ISimCopterMissionWorld
	{
		int32 GetXbldTileId(int32,int32) const override { return 0x70; }
		int32 GetBuildingFootprintSize(int32,int32) const override { return 1; }
		bool GetCameraTile(int32& X,int32& Y) const override { X=Y=20; return true; }
		bool GetPlayerTile(int32& X,int32& Y) const override { X=Y=20; return true; }
		bool TrySpawnMissionPerson(int32,int32,int32,int32,int32) override { return true; }
	} MapWorld;
	Missions->MissionSystem.Initialize(&MapWorld,1);
	Missions->MissionSystem.EnsureBaseLocationRecord(10,10);
	TestTrue(TEXT("Map mission fixture created"),Missions->MissionSystem.CreateEventAt(20,20,SimCopterMissions::TYPE_Robber)!=INDEX_NONE);
	Dpad(EKeys::Gamepad_DPad_Right);
	TestEqual(TEXT("R3 right selects next map mission"),Missions->GetMapFocusRecordIndex(),1);
	Dpad(EKeys::Gamepad_DPad_Left);
	TestEqual(TEXT("R3 left selects previous map mission"),Missions->GetMapFocusRecordIndex(),0);
	Heli->bControllerRadialHeld=false;
	TestFalse(TEXT("Unmodified D-pad reaches normal tool controls"),PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Up,false));
	Heli->bControllerRadialHeld=true;
	TestFalse(TEXT("Adding a modifier during a tool hold cannot swallow its release"),PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Up,true));
	TestFalse(TEXT("Original tool release still reaches pawn"),PC->HandleControllerMapKeyReleased(EKeys::Gamepad_DPad_Up));
	Heli->bControllerRadialHeld=false;
	PC->HandleControllerLeftStickPressed(false); PC->PushPause(); PC->PopPause(); PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("Pausing cancels a pending turbo click"),PC->IsControllerTurboEnabled());
	Heli->ActiveHelicopterTypeIndex=SimCopterHelicopterRegistry::PlaneTypeIndex;
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("Airplanes do not acquire helicopter Turbo Mode"),PC->IsControllerTurboEnabled());
	TestFalse(TEXT("Airplane L3 still toggles help"),PC->IsControllerHelpExpanded());
	return true;
}
#endif
