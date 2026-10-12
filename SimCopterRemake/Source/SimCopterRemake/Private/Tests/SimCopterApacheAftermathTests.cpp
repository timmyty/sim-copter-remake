#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterEnergyTaser.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Game/SimCopterPlayerController.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "Formats/SimCopterPeopleReader.h"
#include "Formats/SimCity2000Reader.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterApacheAftermathTest,"SimCopter.Apache.AftermathMedicAndTaser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterApacheAftermathTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* FloorActor=World->SpawnActor<AActor>(); auto* Floor=NewObject<UBoxComponent>(FloorActor);
	FloorActor->SetRootComponent(Floor); Floor->SetBoxExtent(FVector(10000,10000,10));
	Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Floor->SetCollisionResponseToAllChannels(ECR_Block);
	Floor->RegisterComponent(); FloorActor->SetActorLocation(FVector(0,0,-10));
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize=400; Traffic->PeopleTileClasses.Init(7,FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0,FSimCity2000City::TileCount); Traffic->WaterTileFlags.Init(0,FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d,FSimCity2000City::TileCount);
	const FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame"));
	auto Model=MakeShared<FPeopleBehaviorModel>(); FString Error;
	if (!TestTrue(TEXT("Original behavior loads"),FSimCopterPeopleReader::LoadFromFile(FSimCopterPeopleReader::ResolvePeoplePath(Root),*Model,Error))) return false;
	const auto Person=[&](int32 State,FVector At)
	{
		auto* P=World->SpawnActor<ASimCopterGroundAgent>(); P->SetOwner(Traffic); P->InitialPersonState=State;
		P->SetInitialBehaviorClass(State==5 ? 12 : 0); P->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian,TEXT(""),Root,85);
		P->BehaviorModel=Model; P->bBehaviorActive=true; P->BehaviorContext.ResetToState(State);
		P->BehaviorContext.Attributes[EBhavAttr::Visible]=1; P->BehaviorContext.Attributes[EBhavAttr::MedevacHealth]=100;
		P->SetActorLocation(At); Traffic->PedestrianAgents.Add(P); return P;
	};
	auto* PC=World->SpawnActor<ASimCopterPlayerController>(); World->AddController(PC);
	PC->Player=NewObject<ULocalPlayer>(GEngine); PC->ChangeState(NAME_Playing);
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>(); PC->Possess(Heli);
	if (!TestTrue(TEXT("Apache loads"),Heli->SwitchHelicopterModel(2))) return false;
	TestTrue(TEXT("Fixture is the passengerless Apache"),Heli->IsApacheHelicopter() && Heli->GetAvailablePassengerSeats()==0);
	Heli->SetActorLocation(FVector(0,0,Heli->GetSimpleCollisionHalfHeight()+1)); Heli->bIsLanded=true; Heli->GroundClearanceCm=1;
	auto* Audio=USimCopterAudioSubsystem::Get(World); Audio->bSoundsAvailable=true;
	auto* Medic=Person(5,FVector(200,200,24)); Medic->SetPersistentHospitalRoofCrew(true);
	const int32 Before=Audio->OneShots.Num();
	for (int32 Attempt=0;Attempt<8;++Attempt)
	{
		// Each previous line has finished; the mixer's simultaneous-voice guard cannot hide a loop.
		for (auto& Sound:Audio->OneShots) Sound.EndTime=0;
		Medic->PlayPersonVoiceEvent(19,true,true,true);
	}
	TestEqual(TEXT("An unchanged empty search announces no victims only once"),Audio->OneShots.Num()-Before,1);
	auto* Patient=Person(6,FVector(230,200,24));
	int32 Distance;
	TestTrue(TEXT("New patient is found and starts a new search episode"),Medic->SelectObjectOfClass(Medic->BehaviorContext,EBhavObjectClass::MedevacVictim,Distance));
	Medic->BehaviorContext.SelectedObject=Patient; Medic->BehaviorContext.bHasSelection=true;
	TestTrue(TEXT("Medic picks up patient"),Medic->PutSelectedPersonOnMe(Medic->BehaviorContext));
	for (auto& Sound:Audio->OneShots) Sound.EndTime=0;
	const int32 CarrySounds=Audio->OneShots.Num(); Medic->PlayPersonVoiceEvent(19,true,true,true);
	TestEqual(TEXT("A medic carrying a patient cannot announce no victims"),Audio->OneShots.Num(),CarrySounds);
	Medic->bBehaviorActive=false; Medic->SetMissionInjuredPose();
	TestNull(TEXT("Injuring a medic releases their carried patient"),Patient->GetBehaviorCarrier());
	TestTrue(TEXT("Injured medic becomes a patient"),Medic->IsMedevacVictim());
	Medic->BehaviorContext.SelectedObject=Patient;
	TestFalse(TEXT("Injured medic cannot pick up another patient"),Medic->PutSelectedPersonOnMe(Medic->BehaviorContext));
	Patient->Destroy(); Medic->Destroy();
	// Use the actual Apache exit and actual shoulder camera, then look at a live criminal.
	Heli->ExitHelicopter(); auto* Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (!TestNotNull(TEXT("Apache exit possesses the on-foot pilot"),Pilot)) return false;
	Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay=false; Pilot->CameraBoom->bEnableCameraLag=false;
	Pilot->CameraPitchDeg=0; Pilot->StartTaserAim();
	for (int32 I=0;I<60;++I) { Pilot->UpdateCamera(1.f/60); Pilot->CameraBoom->TickComponent(1.f/60,LEVELTICK_All,nullptr); }
	auto* Suspect=Person(13,Pilot->CameraComponent->GetComponentLocation()+Pilot->CameraComponent->GetForwardVector()*800);
	Pilot->UpdateEnergyTaser(.7f);
	TestTrue(TEXT("Taser charges from the actual exit camera"),Pilot->TaserCharge.CanFire());
	AddInfo(FString::Printf(TEXT("Exit pilot %s camera %s suspect %s charge %.2f"),*Pilot->GetActorLocation().ToString(),*Pilot->CameraComponent->GetComponentLocation().ToString(),*Suspect->GetActorLocation().ToString(),Pilot->TaserCharge.Energy));
	Pilot->FireTaser(); int32 Orbs=0; for (TActorIterator<ASimCopterTaserOrb> It(World);It;++It) ++Orbs;
	TestEqual(TEXT("Taser fires after the Apache exit"),Orbs,1);
	Pilot->TaserCharge.Update(.7f,true);
	Suspect->SetActorLocation(FVector(2000,2000,24)); Pilot->UpdateEnergyTaser(.01f);
	TestEqual(TEXT("Looking away immediately removes stored energy"),Pilot->TaserCharge.Energy,0.f);
	TestEqual(TEXT("Looking away closes the emitters"),Pilot->TaserCharge.Deployment,0.f);
	for (auto Tool : {ESimCopterHelicopterTool::ApacheMachineGun,ESimCopterHelicopterTool::ApacheMissile})
	{
		PC->Possess(Heli); Pilot->Destroy(); Heli->SetSelectedTool(Tool);
		Heli->StartPrimaryToolUse(); Heli->EmitApacheMachineGunFrame();
		Heli->ExitHelicopter(); Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn());
		if (!TestNotNull(TEXT("Repeated armed Apache exit returns the pilot"),Pilot)) return false;
		TestFalse(TEXT("Exit clears held Apache fire"),Heli->bPrimaryToolUseHeld);
		Pilot->CameraPitchDeg=0; Pilot->CameraBoom->bEnableCameraLag=false; Pilot->StartTaserAim();
		for (int32 I=0;I<60;++I) { Pilot->UpdateCamera(1.f/60); Pilot->CameraBoom->TickComponent(1.f/60,LEVELTICK_All,nullptr); }
		Suspect->SetActorLocation(Pilot->CameraComponent->GetComponentLocation()+Pilot->CameraComponent->GetForwardVector()*800);
		Pilot->UpdateEnergyTaser(.7f); TestTrue(TEXT("Apache weapon activity does not prevent charging on a criminal"),Pilot->TaserCharge.CanFire());
		Pilot->FireTaser(); TestTrue(TEXT("Each new pilot can release a taser orb"),Pilot->TaserCooldown>0);
	}
	return true;
}
#endif
