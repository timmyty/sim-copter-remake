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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterControllerSprintTest,"SimCopter.Controller.SprintMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterControllerSprintTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* PC=World->SpawnActor<ASimCopterPlayerController>();
	PC->PlayerInput=NewObject<UPlayerInput>(PC);
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	PC->Possess(Heli); Heli->bEngineRunning=true;
	TestFalse(TEXT("Flight starts without sprint"),Heli->BuildFlightInputs().bTravelBoost);
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestTrue(TEXT("L3 reaches the real flight boost input"),Heli->BuildFlightInputs().bTravelBoost);
	TestFalse(TEXT("Ordinary L3 does not toggle help"),PC->IsControllerHelpExpanded());
	PC->HandleControllerLeftStickPressed(true);
	TestTrue(TEXT("Repeat events leave Sprint Mode on"),Heli->BuildFlightInputs().bTravelBoost);
	Heli->bControllerCameraAdjustHeld=true;
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestTrue(TEXT("R3+L3 toggles helicopter help"),PC->IsControllerHelpExpanded());
	TestTrue(TEXT("Help chord leaves Sprint Mode unchanged"),PC->IsControllerSprintEnabled());
	Heli->bControllerCameraAdjustHeld=false;
	Heli->ControllerMode=ESimCopterControllerMode::ToolWheel;
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestTrue(TEXT("L3 does not change sprint while selecting a tool"),PC->IsControllerSprintEnabled());
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
	TestFalse(TEXT("Sprint cannot operate a stopped helicopter"),Heli->BuildFlightInputs().bTravelBoost);
	Heli->bEngineRunning=true;
	Heli->ResetTransientInputState();
	TestFalse(TEXT("Save restore input reset clears sprint"),PC->IsControllerSprintEnabled());
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
	Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay=false;
	PC->Possess(Pilot);
	TestFalse(TEXT("Exiting the helicopter clears sprint"),PC->IsControllerSprintEnabled());
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	PC->Possess(Heli);
	TestFalse(TEXT("Boarding clears on-foot sprint"),PC->IsControllerSprintEnabled());
	Heli->MapPanel = SNew(SSimCopterMapPanel).Pawn(Heli);
	auto Map = Heli->MapPanel;
	PC->HandleControllerLeftStickPressed(false);
	TestFalse(TEXT("Holding L3 does not toggle sprint before a chord"),PC->IsControllerSprintEnabled());
	TestTrue(TEXT("L3 up is consumed before tool controls"),PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Up,false));
	TestFalse(TEXT("L3 up hides mission markers"),Map->Settings.bShowMissionBlips);
	PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Up,true);
	TestFalse(TEXT("Held D-pad cannot repeatedly toggle markers"),Map->Settings.bShowMissionBlips);
	PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("L3 map chord leaves sprint off"),PC->IsControllerSprintEnabled());
	TestTrue(TEXT("Chord release is consumed even after L3 release"),PC->HandleControllerMapKeyReleased(EKeys::Gamepad_DPad_Up));
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	PC->HandleControllerLeftStickPressed(false);
	PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Down,false);
	PC->HandleControllerMapKeyReleased(EKeys::Gamepad_DPad_Down);
	PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("L3 down hides service markers"),Map->Settings.bShowServiceBlips);
	TestTrue(TEXT("L3 map chord leaves enabled sprint on"),PC->IsControllerSprintEnabled());
	PC->ResetControllerSprint();
	Heli->bControllerCameraAdjustHeld=true;
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
	Heli->bControllerCameraAdjustHeld=false;
	TestFalse(TEXT("Unmodified D-pad reaches normal tool controls"),PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Up,false));
	Heli->bControllerCameraAdjustHeld=true;
	TestFalse(TEXT("Adding a modifier during a tool hold cannot swallow its release"),PC->HandleControllerMapKeyPressed(EKeys::Gamepad_DPad_Up,true));
	TestFalse(TEXT("Original tool release still reaches pawn"),PC->HandleControllerMapKeyReleased(EKeys::Gamepad_DPad_Up));
	Heli->bControllerCameraAdjustHeld=false;
	PC->HandleControllerLeftStickPressed(false); PC->PushPause(); PC->PopPause(); PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("Pausing cancels a pending sprint click"),PC->IsControllerSprintEnabled());
	Heli->ActiveHelicopterTypeIndex=SimCopterHelicopterRegistry::PlaneTypeIndex;
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestFalse(TEXT("Airplanes do not acquire helicopter Sprint Mode"),PC->IsControllerSprintEnabled());
	TestFalse(TEXT("Airplane L3 still toggles help"),PC->IsControllerHelpExpanded());
	return true;
}
#endif
