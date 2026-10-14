#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterFatalRespawnTest, "SimCopter.PilotRespawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FSimCopterFatalRespawnTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const TCHAR* Case : {TEXT("Airborne"), TEXT("AutopilotOff"), TEXT("Grounded"),
		TEXT("Destroyed"), TEXT("SavedFatalFall"), TEXT("ExpiredWithoutController"), TEXT("Nonfatal")})
	{
		Names.Add(Case); Commands.Add(Case);
	}
}

bool FSimCopterFatalRespawnTest::RunTest(const FString& Case)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Ground = World->SpawnActor<AActor>();
	auto* Floor = NewObject<UBoxComponent>(Ground);
	Ground->SetRootComponent(Floor); Floor->SetBoxExtent(FVector(10000,10000,20));
	Floor->SetCollisionResponseToAllChannels(ECR_Block); Floor->RegisterComponent();
	Ground->SetActorLocation(FVector(0,0,-20));
	auto* PC = World->SpawnActor<ASimCopterPlayerController>();
	PC->Player = NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
	PC->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>();
	PC->PlayerCameraManager->InitializeFor(PC);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	if (!TestTrue(TEXT("Aircraft model loads"), Heli->SwitchHelicopterModel(4))) return false;
	auto* Ops = Heli->GetAirOperations();
	Ops->SetAutopilotEnabled(Case != TEXT("AutopilotOff"));
	PC->Possess(Heli);
	Heli->SetActorLocation(FVector(800,500,3200)); Heli->SeedFlightModelFromActor();
	Heli->bEngineRunning = true; Heli->bIsLanded = false; Heli->GroundClearanceCm = 3000;
	Heli->FlightModel.State = ESimCopterFlightState::Flying;
	Heli->ExitHelicopter();
	auto* Pilot = Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (!TestNotNull(TEXT("Airborne exit possesses the pilot"), Pilot)) return false;
	TestEqual(TEXT("Exit remembers this aircraft"), Pilot->GetParkedHelicopter(), Heli);
	PC->PlayerCameraManager->UpdateCamera(1.f);
	if (Case == TEXT("Grounded"))
	{
		Ops->EndPilotRecovery(); Heli->PlaceOnHelipad(FVector(800,500,0), 35);
		Heli->bEngineRunning = false;
	}
	FVector ExpectedPad;
	if (Case == TEXT("Destroyed"))
	{
		auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
		if (!TestTrue(TEXT("Airport data loads"), Traffic->RebuildSpawnData())) return false;
		FVector OccupiedPad;
		if (!TestTrue(TEXT("First airport pad exists"), Traffic->TryGetAirportPadWorldLocation(0, OccupiedPad))) return false;
		auto* Parked = World->SpawnActor<ASimCopterHelicopterPawn>(); Parked->PlaceOnHelipad(OccupiedPad,0);
		TestTrue(TEXT("Second airport pad exists"), Traffic->TryGetAirportPadWorldLocation(1, ExpectedPad));
		Heli->FlightModel.State = ESimCopterFlightState::Dying; Heli->FlightModel.HitPoints = 0;
	}
	else
	{
		Heli->FlightModel.HitPoints -= 2; Heli->CurrentDamage = 2;
	}
	Heli->FlightModel.Fuel /= 2; Heli->CurrentFuelGallons = SimCopterFixed::ToFloat(Heli->FlightModel.Fuel);
	const FVector AircraftLocation = Heli->GetActorLocation();
	const int32 Fuel = Heli->FlightModel.Fuel, HitPoints = Heli->FlightModel.HitPoints;
	const bool EngineRunning = Heli->bEngineRunning;
	Pilot->SetActorLocation(FVector(2000,500,Pilot->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	Pilot->FallPeakZ = Pilot->GetActorLocation().Z + (Case == TEXT("Nonfatal") ? 1000 : 2400);
	Pilot->GetCharacterMovement()->Velocity = FVector(0,0,-2000);
	Pilot->Landed(FHitResult()); Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (Case == TEXT("Nonfatal"))
	{
		Pilot->UpdateAirOperations(6);
		TestEqual(TEXT("Nonfatal injury stays on foot"), PC->GetPawn().Get(), static_cast<APawn*>(Pilot));
		TestEqual(TEXT("Nonfatal injury recovers walking speed"), Pilot->GetCharacterMovement()->MaxWalkSpeed, Pilot->WalkSpeedCmPerSec);
		return true;
	}
	TestEqual(TEXT("Unprotected fatal fall kills pilot"), Pilot->GetPilotHealth(), 0.f);
	TestEqual(TEXT("Fatal recovery starts with five seconds"), Pilot->InjurySeconds, 5.f);
	// Contact/Interact boarding cannot bypass the fatal countdown.
	Pilot->TryEnterHelicopter(100000);
	TestEqual(TEXT("Dead pilot cannot board early"), PC->GetPawn().Get(), static_cast<APawn*>(Pilot));
	// Restore possession for further independent checks against the old implementation.
	if (PC->GetPawn() != Pilot) PC->Possess(Pilot);
	for (int32 Frame = 0; Frame < 40; ++Frame) Ops->Update(.1f);
	if (Case != TEXT("Destroyed"))
		TestTrue(TEXT("Fatal recovery holds the surviving aircraft in place"), Heli->GetActorLocation().Equals(AircraftLocation,.1));
	Pilot->UpdateAirOperations(4);
	TestEqual(TEXT("Pilot remains on foot until delay expires"), PC->GetPawn().Get(), static_cast<APawn*>(Pilot));
	if (Case == TEXT("SavedFatalFall"))
	{
		TArray<uint8> Saved; TestTrue(TEXT("Fatal countdown saves"), Pilot->CaptureRuntimeSaveState(Saved));
		auto* Restored = World->SpawnActor<ASimCopterOnFootPawn>(); Restored->SetParkedHelicopter(Heli);
		TestTrue(TEXT("Fatal countdown restores"), Restored->RestoreRuntimeSaveState(Saved));
		Ops->EndPilotRecovery(); PC->Possess(Restored); Pilot->Destroy(); Pilot = Restored;
	}
	if (Case == TEXT("ExpiredWithoutController"))
	{
		PC->UnPossess(); Pilot->UpdateAirOperations(2);
		TestEqual(TEXT("Absent controller does not consume respawn"), Pilot->GetPilotHealth(), 0.f);
		PC->Possess(Pilot);
	}
	Pilot->Tick(1.1f);
	TestEqual(TEXT("Fatal recovery directly possesses aircraft"), PC->GetPawn().Get(), static_cast<APawn*>(Heli));
	TestEqual(TEXT("View returns directly to cockpit"), PC->GetViewTarget(), static_cast<AActor*>(Heli));
	TestFalse(TEXT("Pickup autopilot releases flight control"), Ops->IsRecoveringPilot());
	TestTrue(TEXT("Old pilot is hidden"), Pilot->IsHidden());
	TestFalse(TEXT("Old pilot no longer collides"), Pilot->GetActorEnableCollision());
	TestFalse(TEXT("Old pilot no longer ticks"), Pilot->IsActorTickEnabled());
	TestTrue(TEXT("Old pilot is scheduled for cleanup"), Pilot->GetLifeSpan() > 0);
	if (Case == TEXT("Destroyed"))
	{
		TestTrue(TEXT("Destroyed aircraft returns to the next free hangar pad"),
			Heli->GetActorLocation().Equals(ExpectedPad + FVector(0,0,Heli->GetHelipadRestingOriginOffsetCm()), .1));
		TestTrue(TEXT("Destroyed aircraft is repaired and parked"), Heli->FlightModel.HitPoints > 0 && Heli->bIsLanded);
	}
	else
	{
		TestTrue(TEXT("Surviving aircraft position is preserved"), Heli->GetActorLocation().Equals(AircraftLocation,.1));
		TestEqual(TEXT("Surviving aircraft retains fuel"), Heli->FlightModel.Fuel, Fuel);
		TestEqual(TEXT("Surviving aircraft retains damage"), Heli->FlightModel.HitPoints, HitPoints);
		TestEqual(TEXT("Surviving aircraft retains engine state"), Heli->bEngineRunning, EngineRunning);
		TestEqual(TEXT("Autopilot preference survives respawn"), Ops->IsAutopilotEnabled(), Case != TEXT("AutopilotOff"));
	}
	return true;
}
#endif
