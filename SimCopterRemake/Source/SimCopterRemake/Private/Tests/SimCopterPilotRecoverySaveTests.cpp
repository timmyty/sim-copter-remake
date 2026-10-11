#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Game/SimCopterPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Components/BoxComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPilotRecoverySaveTest,"SimCopter.RemoteRecovery.SaveAndObstruction",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterPilotRecoverySaveTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SetAutopilotTransform(FVector(0,0,700),FRotator::ZeroRotator,.05f);
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
	Pilot->SetParkedHelicopter(Heli);
	Pilot->SetActorLocation(Heli->GetCargoAnchorWorldLocation()-FVector(0,0,180));
	auto* PC=World->SpawnActor<ASimCopterPlayerController>();
	PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC); PC->Possess(Pilot);
	Pilot->SetAutoRescueAttached(true);
	TArray<uint8> Saved;
	TestTrue(TEXT("Pilot midway up rescue harness saves"),Pilot->CaptureRuntimeSaveState(Saved));
	Pilot->SetAutoRescueAttached(false);
	TestTrue(TEXT("Harness save restores"),Pilot->RestoreRuntimeSaveState(Saved));
	TestTrue(TEXT("Loaded pilot stays attached and cannot fall"),Pilot->IsAutoRescueAttached() && Pilot->GetCharacterMovement()->MovementMode==MOVE_None);
	auto* Ops=Heli->GetAirOperations();
	Ops->Update(.05f);
	TestTrue(TEXT("Saved pilot reconnects to real helicopter"),Ops->IsRecoveringPilot());
	// An obstacle inserted in the lift corridor must hold both people and cable in place.
	auto* Obstacle=World->SpawnActor<AActor>(); auto* Shape=NewObject<UBoxComponent>(Obstacle);
	Obstacle->SetRootComponent(Shape); Shape->SetBoxExtent(FVector(40,40,10));
	Shape->SetCollisionResponseToAllChannels(ECR_Block); Shape->RegisterComponent();
	Obstacle->SetActorLocation(Heli->GetCargoAnchorWorldLocation()-FVector(0,0,80));
	const FVector Before=Pilot->GetActorLocation();
	for (int32 I=0; I<100; ++I) Ops->Update(.05f);
	TestTrue(TEXT("Blocked lift does not pull pilot through obstacle"),Pilot->GetActorLocation().Equals(Before,.01));
	TestTrue(TEXT("Blocked lift retains attached pilot"),Pilot->IsAutoRescueAttached() && PC->GetPawn()==Pilot);
	Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision); Obstacle->Destroy();
	for (int32 I=0; I<100 && PC->GetPawn()==Pilot; ++I) Ops->Update(.05f);
	TestTrue(TEXT("Clearing corridor resumes saved rescue into cockpit"),PC->GetPawn()==Heli && !Ops->IsRecoveringPilot());
	return true;
}
#endif
