#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/SimCopterCheats.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "InputCoreTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPilotRunTest,"SimCopter.RescuePilot.ShiftRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPilotRunTest::RunTest(const FString&)
{
	auto* Instance=NewObject<UGameInstance>(GEngine); Instance->InitializeStandalone();
	auto* World=Instance->GetWorld(); World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT { Instance->Shutdown(); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* PC=World->SpawnActor<ASimCopterPlayerController>();
	PC->PlayerInput=NewObject<UPlayerInput>(PC);
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
	Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay=false;
	PC->Possess(Pilot);
	if (!TestTrue(TEXT("Original pilot figure loads"),Pilot->LoadOriginalBodyFigure())) return false;
	auto* Move=Pilot->GetCharacterMovement();
	Move->SetMovementMode(MOVE_Walking);
	auto Step=[&](float Speed)
	{
		Move->Velocity=FVector(Speed,0,0);
		Pilot->Tick(1.0f/60);
	};
	const auto KeyEvent=[&](FKey Key, bool bDown)
    {
        PC->PlayerInput->InputKey(FInputKeyParams(Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.0 : 0.0, false));
        PC->PlayerInput->ProcessInputStack(TArray<UInputComponent*>(), 1.0f/60, false);
    };
    const float Walk=Pilot->GetWalkSpeedCmPerSec();
	Step(Walk);
	TestEqual(TEXT("Normal speed remains walking"),Move->MaxWalkSpeed,Walk);
	TestEqual(TEXT("Normal movement uses the walk clip"),Pilot->FigureAnim.Mnemonic,FString(TEXT("1Wal")));
	for (const FKey Key : {EKeys::LeftShift,EKeys::RightShift})
	{
		KeyEvent(Key, true);
		Step(Walk*2);
		TestEqual(TEXT("Either Shift doubles normal on-foot speed"),Move->MaxWalkSpeed,Walk*2);
		TestEqual(TEXT("Running selects the shipped run animation"),Pilot->FigureAnim.Mnemonic,FString(TEXT("1Run")));
		TestTrue(TEXT("Run clip has animated frames"),Pilot->FigureAnim.FrameCount>1);
		Step(0);
		TestEqual(TEXT("Shift alone leaves the pilot idle"),Pilot->FigureAnim.Mnemonic,FString(TEXT("NoMo")));
		Move->SetMovementMode(MOVE_Falling); Step(Walk);
		TestEqual(TEXT("Shift cannot boost airborne movement"),Move->MaxWalkSpeed,Walk);
		Move->SetMovementMode(MOVE_Walking);
		PC->SetIgnoreMoveInput(true); Step(Walk);
		TestEqual(TEXT("Modal movement hold suppresses running"),Move->MaxWalkSpeed,Walk);
		PC->SetIgnoreMoveInput(false);
		KeyEvent(Key, false);
		Step(Walk);
		TestEqual(TEXT("Release restores walking"),Move->MaxWalkSpeed,Walk);
		TestEqual(TEXT("Release restores walk animation"),Pilot->FigureAnim.Mnemonic,FString(TEXT("1Wal")));
	}
	KeyEvent(EKeys::LeftShift, true);
	KeyEvent(EKeys::RightShift, true);
	Step(Walk*2);
	TestEqual(TEXT("Both Shift keys do not stack the multiplier"),Move->MaxWalkSpeed,Walk*2);
	PC->PlayerInput->FlushPressedKeys(); Step(Walk);
	TestEqual(TEXT("Focus-loss key flush cannot leave running stuck"),Move->MaxWalkSpeed,Walk);
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased(); Step(Walk*2);
	TestEqual(TEXT("L3 Sprint Mode doubles walking speed"),Move->MaxWalkSpeed,Walk*2);
	TestEqual(TEXT("L3 Sprint Mode uses the original run animation"),Pilot->FigureAnim.Mnemonic,FString(TEXT("1Run")));
	PC->HandleControllerLeftStickPressed(true); Step(Walk*2);
	TestEqual(TEXT("Held-click repeat cannot cancel sprint"),Move->MaxWalkSpeed,Walk*2);
	KeyEvent(EKeys::LeftShift,true); Step(Walk*2);
	TestEqual(TEXT("Shift and L3 do not stack running speed"),Move->MaxWalkSpeed,Walk*2);
	KeyEvent(EKeys::LeftShift,false);
	KeyEvent(EKeys::Gamepad_RightThumbstick,true);
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TestTrue(TEXT("R3+L3 opens on-foot help"),PC->IsControllerHelpExpanded());
	TestTrue(TEXT("Help chord does not change sprint"),PC->IsControllerSprintEnabled());
	KeyEvent(EKeys::Gamepad_RightThumbstick,false);
	Move->SetMovementMode(MOVE_Falling); Step(Walk);
	TestEqual(TEXT("Controller sprint does not boost falling"),Move->MaxWalkSpeed,Walk);
	Move->SetMovementMode(MOVE_Walking);
	PC->SetIgnoreMoveInput(true); Step(Walk);
	TestEqual(TEXT("Modal movement hold suppresses controller sprint"),Move->MaxWalkSpeed,Walk);
	TestFalse(TEXT("A modal cannot accept a sprint click"),PC->HandleControllerLeftStickPressed(false));
	PC->SetIgnoreMoveInput(false);
	Step(0);
	TestEqual(TEXT("Sprint Mode alone leaves the pilot idle"),Pilot->FigureAnim.Mnemonic,FString(TEXT("NoMo")));
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased(); Step(Walk);
	TestEqual(TEXT("Second L3 click restores walking"),Move->MaxWalkSpeed,Walk);
	PC->HandleControllerLeftStickPressed(false); PC->HandleControllerLeftStickReleased();
	TArray<uint8> SavedPilot;
	TestTrue(TEXT("Pilot state captured with sprint enabled"),Pilot->CaptureRuntimeSaveState(SavedPilot));
	TestTrue(TEXT("Pilot state restores successfully"),Pilot->RestoreRuntimeSaveState(SavedPilot));
	TestFalse(TEXT("Loading a save clears the transient sprint toggle"),PC->IsControllerSprintEnabled());
	if (auto* Cheats=SimCopterCheats::Get(Pilot))
	{
		Cheats->bSuperpower=true;
		KeyEvent(EKeys::LeftShift, true);
		Step(Walk*20);
		TestEqual(TEXT("Superpower retains 20x without stacking normal run"),Move->MaxWalkSpeed,Walk*20);
		TestEqual(TEXT("Superpower retains the dog figure"),Pilot->FigureAnim.FigureIndex,Pilot->FigureShared->Model.FindFigureIndex(TEXT("2DOGG")));
		Cheats->bSuperpower=false;
		PC->PlayerInput->FlushPressedKeys(); Step(Walk);
		TestEqual(TEXT("Ending superpower restores the walking pilot"),Pilot->FigureAnim.Mnemonic,FString(TEXT("1Wal")));
	}
	else AddError(TEXT("Cheat state unavailable for the superpower compatibility check"));
	return true;
}
#endif
