#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Components/InputComponent.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Game/SimCopterPlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSteeringResponseTest,"SimCopter.Controller.SteeringResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterSteeringResponseTest::RunTest(const FString&)
{
	using namespace SimCopterFixed;
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* PC=World->SpawnActor<ASimCopterPlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
	PC->Possess(Heli);
	auto* Input=NewObject<UInputComponent>(Heli); Heli->SetupPlayerInputComponent(Input);
	auto Key=[&](FKey K,EInputEvent Event)
	{
		for (auto& Binding:Input->KeyBindings)
			if (Binding.Chord.Key==K && Binding.KeyEvent==Event) Binding.KeyDelegate.Execute(K);
	};
	// Exercise the pawn tick, including control updates, swept actor movement and camera,
	// after building real forward momentum. Pure model tests cannot detect a later override.
	for (int32 Type=0; Type<9; ++Type) for (bool Right : {false,true})
	{
		if (!TestTrue(TEXT("Load requested model"),Heli->SwitchHelicopterModel(Type))) return false;
		Heli->ResetTransientInputState(); Heli->SetActorLocationAndRotation(FVector(0,0,3000),FRotator::ZeroRotator);
		Heli->SeedFlightModelFromActor(); Heli->bIsLanded=false; Heli->bEngineRunning=true;
		Heli->FlightModel.State=ESimCopterFlightState::Flying;
		Heli->FlightModel.RotorSpeed=FSimCopterFlightModel::RotorTopSpeed;
		Heli->ControllerLeftY(1); Heli->ControllerLeftX(0);
		for (int32 Frame=0; Frame<300; ++Frame) Heli->Tick(1.f/60);
		const float SpeedBefore=ToFloat(Heli->FlightModel.ForwardSpeed);
		const float HeadingBefore=Heli->GetActorRotation().Yaw;
		const FKey Bumper=Right?EKeys::Gamepad_RightShoulder:EKeys::Gamepad_LeftShoulder;
		Key(Bumper,IE_Pressed);
		for (int32 Frame=0; Frame<60; ++Frame) Heli->Tick(1.f/60);
		const float Change=FMath::FindDeltaAngleDegrees(HeadingBefore,Heli->GetActorRotation().Yaw);
		AddInfo(FString::Printf(TEXT("Type %d %s actual pawn heading change %.2f, speed %.2f -> %.2f, yaw target %.2f, yaw smoothed %.2f"),
			Type,Right?TEXT("RB"):TEXT("LB"),Change,SpeedBefore,ToFloat(Heli->FlightModel.ForwardSpeed),ToFloat(Heli->FlightModel.YawRateTarget),ToFloat(Heli->FlightModel.YawRateSmoothed)));
		TestTrue(TEXT("The actual nose turns at cruise without releasing forward"),Right?Change>5:Change<-5);
		TestTrue(TEXT("Turn does not require losing forward momentum"),ToFloat(Heli->FlightModel.ForwardSpeed)>=SpeedBefore*.9f);
		Key(Bumper,IE_Released);
	}
	return true;
}
#endif
