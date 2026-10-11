#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Audio/SimCopterAudioSubsystem.h"
#include "UI/SSimCopterToolFlaps.h"
#include "ProceduralMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UI/SimCopterHangarArt.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Slate/WidgetRenderer.h"
#include "ImageUtils.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "RenderingThread.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRemoteRecoveryTest,"SimCopter.RemoteRecovery.TaserExitAndRescue",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterRemoteRecoveryTest::RunTest(const FString&)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	bool DeferredCleanup=false;
	ON_SCOPE_EXIT { if (!DeferredCleanup) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); } };
	const auto Box=[&](FVector Location,FVector Half)
	{
		auto* Actor=World->SpawnActor<AActor>(); auto* Shape=NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Shape); Shape->SetBoxExtent(Half); Shape->SetCollisionResponseToAllChannels(ECR_Block);
		Shape->RegisterComponent(); Actor->SetActorLocation(Location); return Shape;
	};
	auto* Floor=Box(FVector(0,0,-20),FVector(10000,10000,20));
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Ops=Heli->GetAirOperations();
	auto* PC=World->SpawnActor<ASimCopterPlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC);
	PC->Possess(Heli);
	for (int32 Model=0; Model<9; ++Model)
	{
		TestTrue(TEXT("Fleet model loads"),Heli->SwitchHelicopterModel(Model)); Ops->UpdateRemoteTaserMount();
		FBox Bounds; Heli->TryGetAirframeLocalBoundsCm(Bounds);
		TestTrue(TEXT("Every helicopter has a visible nose taser"),Ops->RemoteTaserWeapon && Ops->RemoteTaserWeapon->IsVisible() &&
			Ops->RemoteTaserMount->GetRelativeLocation().X>Bounds.Max.X);
	}
	Heli->SwitchHelicopterModel(4);
	Heli->SetActorLocation(FVector(0,0,1800)); Heli->SeedFlightModelFromActor(); Heli->bEngineRunning=true; Heli->bIsLanded=false;
	Heli->GroundClearanceCm=1600;
	auto* Suspect=World->SpawnActor<ASimCopterGroundAgent>(); Suspect->InitialPersonState=10; Suspect->BehaviorContext.ResetToState(10);
	Suspect->SetActorLocation(FVector(1000,0,24));
	Heli->ToggleRemoteTaser();
	TestTrue(TEXT("Empty helicopter enters remote aim without officer or purchase"),Ops->IsRemoteTaserActive());
	Ops->Update(.05f);
	TestTrue(TEXT("Weapon gimbal and sight share the firing direction"),Ops->RemoteTaserWeapon->GetForwardVector().Equals(Ops->GetRemoteTaserDirection(),.001));
	TestTrue(TEXT("Readout announces automatic orbit"),Ops->GetRemoteTaserReadout().Contains(TEXT("AUTO ORBIT")));
	Ops->AimRemoteTaser(1,0,3); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Taser yaw moves beyond the former 100-degree limit"),Ops->RemoteAimOffset.X>130);
	Ops->AimRemoteTaser(1,0,1); Ops->UpdateRemoteTaser(0);
	const FVector BeforeYawWrap=Ops->GetRemoteTaserDirection();
	Ops->AimRemoteTaser(1,0,.1f); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Yaw wraps smoothly through the rearward boundary"),Ops->RemoteAimOffset.X<0 && FVector::DotProduct(BeforeYawWrap,Ops->GetRemoteTaserDirection())>.99);
	Ops->AimRemoteTaser(0,1,10); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Taser aims nearly straight up"),FMath::IsNearlyEqual(Ops->RemoteTaserAim.Pitch,85.f,.1f));
	Ops->AimRemoteTaser(0,1,10); Ops->AimRemoteTaser(0,-1,.1f); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Reversing at the upper stop responds immediately"),FMath::IsNearlyEqual(Ops->RemoteTaserAim.Pitch,81.5f,.1f));
	Ops->AimRemoteTaser(0,-1,10); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Taser aims nearly straight down"),FMath::IsNearlyEqual(Ops->RemoteTaserAim.Pitch,-85.f,.1f));
	TestTrue(TEXT("Weapon still follows the expanded sight direction"),Ops->RemoteTaserWeapon->GetForwardVector().Equals(Ops->GetRemoteTaserDirection(),.001));
	TestFalse(TEXT("Remote sight captures mouse for direct aiming"),PC->bShowMouseCursor);
	Ops->RemoteAimOffset=FVector2D::ZeroVector;
	Ops->UpdateRemoteTaserControls(6,-2,0,0,1.f/30);
	const FVector2D MouseAt30=Ops->RemoteAimOffset;
	Ops->RemoteAimOffset=FVector2D::ZeroVector;
	Ops->UpdateRemoteTaserControls(3,-1,0,0,1.f/60); Ops->UpdateRemoteTaserControls(3,-1,0,0,1.f/60);
	TestTrue(TEXT("Equal mouse travel aims equally at 30 and 60 fps"),Ops->RemoteAimOffset.Equals(MouseAt30,.001));
	TestTrue(TEXT("Mouse movement aims without right mouse held"),MouseAt30.X>0 && MouseAt30.Y>0);
	Ops->SetRemoteTaserCursor(true);
	TestTrue(TEXT("Cockpit cursor can be released from the sight"),PC->bShowMouseCursor);
	Heli->ToggleRemoteTaser();
	TestFalse(TEXT("Return to flight stops orbit"),Ops->IsAIPiloted());
	TestTrue(TEXT("Return to flight restores the cockpit cursor"),PC->bShowMouseCursor);
	auto Panel=SNew(SSimCopterToolFlaps).Pawn(Heli);
	TestFalse(TEXT("Cockpit controls cannot steal keyboard focus"),Panel->SupportsKeyboardFocus());
	TestTrue(TEXT("Taser panel visible without equipment"),Panel->BuildRemoteTaserFlap()->GetVisibility()!=EVisibility::Collapsed);
	Suspect->Destroy();

	// Pursue a real moving criminal car, with ordinary traffic closer to the helicopter.
	const auto MakeCar=[&](FVector Position,bool bCriminal)
	{
		auto* Car=World->SpawnActor<ASimCopterGroundAgent>();
		Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),1600);
		Car->SetActorLocation(Position); Car->bSnapToGround=false;
		if (bCriminal) Car->MakeCriminalCar(42,0);
		return Car;
	};
	auto* CivilianCar=MakeCar(FVector(800,0,100),false);
	auto* BurglarCar=MakeCar(FVector(2800,0,100),true);
	Heli->SetActorLocationAndRotation(FVector(0,0,1800),FRotator::ZeroRotator); Heli->SeedFlightModelFromActor();
	Heli->ToggleRemoteTaser();
	TestTrue(TEXT("Automatic orbit acquires criminal cars instead of ordinary traffic"),Ops->RemoteTaserTarget.Get()==BurglarCar);
	BurglarCar->SetMoveTarget(FVector(40000,0,100));
	for (int32 Frame=0; Frame<200; ++Frame) { BurglarCar->Tick(.05f); Ops->UpdateRemoteTaser(.05f); }
	TestTrue(TEXT("Fixture getaway car moves faster than the old 350 cm/s orbit"),BurglarCar->GetCurrentVelocityCmPerSec().Size2D()>350);
	TestTrue(TEXT("Helicopter keeps a moving getaway car in taser range"),FVector::Distance(Heli->GetActorLocation(),BurglarCar->GetActorLocation())<2500);
	TestTrue(TEXT("Pursuit sight follows the moving car"),FVector::DotProduct(Ops->GetRemoteTaserDirection(),(Ops->RemoteTaserFocus-Ops->GetRemoteTaserMuzzle()).GetSafeNormal())>.99);
	Ops->AimRemoteTaser(.25f,0,.2f);
	const double SightOffset=Ops->RemoteAimOffset.X;
	for (int32 Frame=0; Frame<20; ++Frame) { BurglarCar->Tick(.05f); Ops->UpdateRemoteTaser(.05f); }
	TestEqual(TEXT("Aircraft pursuit does not overwrite manual aim offsets"),Ops->RemoteAimOffset.X,SightOffset);
	BurglarCar->DisableVehicleWithTaser(); Ops->UpdateRemoteTaser(.3f);
	TestTrue(TEXT("Temporary taser stop keeps the car tracked"),Ops->RemoteTaserTarget.Get()==BurglarCar);
	auto* Burglar=World->SpawnActor<ASimCopterGroundAgent>(); Burglar->InitialPersonState=13; Burglar->BehaviorContext.ResetToState(13);
	Burglar->SetActorLocation(BurglarCar->GetActorLocation()+FVector(0,200,0)); Burglar->SetBehaviorStartingVehicle(BurglarCar);
	Ops->UpdateRemoteTaser(.3f);
	TestTrue(TEXT("Pursuit transfers from getaway car to its robber on foot"),Ops->RemoteTaserTarget.Get()==Burglar);
	BurglarCar->SetActorHiddenInGame(true);
	const FVector RobberStart=Burglar->GetActorLocation();
	for (int32 Frame=0; Frame<100; ++Frame)
	{
		Burglar->SetActorLocation(RobberStart+FVector(0,Frame*15,0)); Burglar->CurrentVelocityCmPerSec=FVector(0,300,0);
		Ops->UpdateRemoteTaser(.05f);
	}
	TestTrue(TEXT("Helicopter follows a robber running on foot"),FVector::Distance(Heli->GetActorLocation(),Burglar->GetActorLocation())<2500);
	Burglar->StunForArrest(); Ops->UpdateRemoteTaser(.3f);
	TestFalse(TEXT("Captured robber is no longer pursued"),Ops->RemoteTaserTarget.IsValid());
	const FVector Hold=Heli->GetActorLocation(); Ops->UpdateRemoteTaser(.5f);
	TestTrue(TEXT("With no active criminal the helicopter holds position"),Heli->GetActorLocation().Equals(Hold));
	Burglar->Destroy(); CivilianCar->Destroy(); BurglarCar->Destroy();

	// A wall blocks the direct firing line: find a vantage without flying through it.
	Heli->SetActorLocationAndRotation(FVector(0,0,1000),FRotator::ZeroRotator); Heli->SeedFlightModelFromActor();
	auto* Wall=Box(FVector(1500,0,700),FVector(80,800,700));
	auto* HiddenRobber=World->SpawnActor<ASimCopterGroundAgent>(); HiddenRobber->InitialPersonState=13;
	HiddenRobber->BehaviorContext.ResetToState(13); HiddenRobber->SetActorLocation(FVector(2800,0,24));
	Ops->RemoteTargetSearchSeconds=0; Ops->UpdateRemoteTaser(0); Ops->RemoteAimOffset=FVector2D::ZeroVector;
	TestTrue(TEXT("Orbit automatically finds a newly appearing robber"),Ops->RemoteTaserTarget.Get()==HiddenRobber);
	TestFalse(TEXT("Fixture building blocks the initial sight"),Ops->HasClearLine(Ops->GetRemoteTaserMuzzle(),HiddenRobber->GetActorLocation(),HiddenRobber));
	bool bCrossedWall=false;
	for (int32 Frame=0; Frame<240; ++Frame)
	{
		Ops->UpdateRemoteTaser(.05f);
		bCrossedWall |= Heli->GetParkingWorldBounds().Intersect(Wall->Bounds.GetBox());
	}
	TestFalse(TEXT("Pursuit keeps the full airframe out of the building"),bCrossedWall);
	TestTrue(TEXT("Pursuit repositions to restore a clear firing line"),Ops->HasClearLine(Ops->GetRemoteTaserMuzzle(),HiddenRobber->GetActorLocation(),HiddenRobber));
	Heli->ToggleRemoteTaser(); HiddenRobber->Destroy(); Wall->GetOwner()->Destroy();

	// Reproduce Space held during startup/exit; test the actual possession transition and ticks.
	Heli->SetActorLocation(FVector(0,0,800)); Heli->SeedFlightModelFromActor();
	Heli->bIsLanded=false; Heli->bEngineRunning=true; Heli->MoveCollective(1);
	const int32 Health=Heli->FlightModel.HitPoints;
	Heli->ExitHelicopter();
	auto* Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (!TestNotNull(TEXT("Pilot can exit"),Pilot)) return false;
	TestFalse(TEXT("Space and collective latches cleared on exit"),Heli->bEngineStartHeld || Heli->CollectiveInput!=0 || Heli->bControllerEngineStartHeld);
	const double HoldZ=Heli->GetActorLocation().Z;
	for (int32 I=0; I<20; ++I) Heli->Tick(.05f);
	TestEqual(TEXT("Held Space cannot keep unattended helicopter climbing"),Heli->GetActorLocation().Z,HoldZ);
	TestEqual(TEXT("Exit preserves airframe health"),Heli->FlightModel.HitPoints,Health);
	for (int32 I=0; I<250 && Ops->IsRecoveringPilot(); ++I) Ops->Update(.05f);
	TestTrue(TEXT("Flat clear ground produces a safe landing"),Heli->bIsLanded && !Ops->IsRecoveringPilot());
	TestFalse(TEXT("Landed helicopter powers down"),Heli->bEngineRunning);
	PC->Possess(Heli); Pilot->Destroy();
	Heli->MoveCollective(1); Heli->Tick(.05f);
	const double StartupZ=Heli->GetActorLocation().Z;
	Heli->ExitHelicopter(); Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (!TestNotNull(TEXT("Pilot can exit during engine startup"),Pilot)) return false;
	for (int32 I=0; I<30; ++I) Heli->Tick(.05f);
	TestTrue(TEXT("Grounded engine startup exit cannot turn into unattended climb"),Heli->GetActorLocation().Z<=StartupZ+1);
	TestFalse(TEXT("Startup exit clears held Space"),Heli->bEngineStartHeld || Heli->CollectiveInput!=0);

	// With no harness, an unsuitable footprint must cause a search and a nearby landing.
	auto* Step=Box(FVector(110,0,35),FVector(60,600,35));
	Pilot->SetActorLocation(FVector(0,180,24)); Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Heli->SetAutopilotTransform(FVector(0,0,600),FRotator::ZeroRotator,.05f);
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RescueHarness,false);
	Ops->BeginPilotRecovery(Pilot); Ops->RecoveryHoldSeconds=0;
	FVector NearbyGoal; bool CanLandBelow=true;
	TestTrue(TEXT("Uneven surface is detected beneath the exit"),Ops->FindRecoverySurface(NearbyGoal,CanLandBelow) && !CanLandBelow);
	Ops->Update(.05f);
	TestTrue(TEXT("No-harness recovery selects a nearby clear landing"),Ops->bHasRecoveryLandingGoal);
	const FVector InitialLanding=Ops->RecoveryLandingGoal;
	TestTrue(TEXT("Unsafe footprint causes horizontal relocation"),FVector::Dist2D(InitialLanding,FVector::ZeroVector)>100);
	// Occupy that destination after selection; the approach must choose another spot.
	auto* LandingBlocker=World->SpawnActor<ASimCopterGroundAgent>(); LandingBlocker->SetActorLocation(InitialLanding);
	Ops->Update(.05f);
	TestTrue(TEXT("New landing obstruction causes replanning"),Ops->bHasRecoveryLandingGoal && FVector::Dist2D(InitialLanding,Ops->RecoveryLandingGoal)>100);
	LandingBlocker->Destroy();
	for (int32 I=0; I<600 && Ops->IsRecoveringPilot(); ++I) Ops->Update(.05f);
	TestTrue(TEXT("Helicopter without harness lands nearby automatically"),Heli->bIsLanded && !Ops->IsRecoveringPilot());
	TestTrue(TEXT("Nearby landing shuts down with pilot still on foot"),!Heli->bEngineRunning && PC->GetPawn()==Pilot);
	TestTrue(TEXT("Landed aircraft is away from the uneven exit footprint"),FVector::Dist2D(Heli->GetActorLocation(),FVector::ZeroVector)>100);
	Heli->SetAutopilotTransform(FVector(0,0,600),FRotator::ZeroRotator,.05f);
	Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision); Step->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ops->BeginPilotRecovery(Pilot); Ops->RecoveryHoldSeconds=0;
	for (int32 I=0; I<25; ++I) Ops->Update(.1f);
	TestTrue(TEXT("No supported landing site holds safely instead of descending blindly"),Ops->IsRecoveringPilot() && !Ops->bHasRecoveryLandingGoal && Heli->GetActorLocation().Equals(FVector(0,0,600)));
	Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Step->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	for (int32 I=0; I<600 && Ops->IsRecoveringPilot(); ++I) Ops->Update(.05f);
	TestTrue(TEXT("Search retries and lands when a safe site becomes available"),Heli->bIsLanded && !Ops->IsRecoveringPilot());

	// A purchased harness retains its original rescue behavior on uneven ground,
	// lifting the actual held NPC first and then the pilot, including a stunned robber.
	for (int32 PersonState : {6,10,-1})
	{
		if (PC->GetPawn()==Heli)
		{
			Heli->SetActorLocation(FVector(0,0,600)); Heli->bIsLanded=false; Heli->ExitHelicopter();
			Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn());
		}
		Pilot->SetActorLocation(FVector(0,180,24)); Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Heli->SetActorLocation(FVector(0,0,600)); Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RescueHarness,false);
		Ops->BeginPilotRecovery(Pilot); Ops->RecoveryHoldSeconds=0;
		FVector Goal; bool CanLand=true;
		TestTrue(TEXT("Uneven ground resolves a hover goal"),Ops->FindRecoverySurface(Goal,CanLand));
		TestFalse(TEXT("Step across the footprint refuses a landing"),CanLand);
		ASimCopterGroundAgent* Passenger=nullptr;
		if (PersonState>=0)
		{
			Passenger=World->SpawnActor<ASimCopterGroundAgent>(); Passenger->InitialPersonState=PersonState;
			Passenger->BehaviorContext.ResetToState(PersonState); Passenger->BehaviorContext.Attributes[EBhavAttr::MedevacHealth]=100;
			Passenger->SetActorLocation(Pilot->GetActorLocation());
			if (PersonState==10) Passenger->StunForArrest();
			TestTrue(TEXT("Pilot carries the patient/suspect"),Pilot->PickUpMissionPerson(Passenger));
		}
		Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RescueHarness,true);
		bool PassengerFirst=Passenger==nullptr;
		for (int32 I=0; I<1800 && PC->GetPawn()!=Heli; ++I)
		{
			Ops->Update(.05f);
			if (Passenger && Passenger->GetBehaviorCarrier()==Heli && !Passenger->IsRidingHarness())
				PassengerFirst|=PC->GetPawn()==Pilot;
		}
		if (PC->GetPawn()!=Heli) AddInfo(FString::Printf(TEXT("Recovery state %d: %s, aircraft %s, pilot %s, carried %s, cable %.1f"),PersonState,*Ops->GetStatus(),*Heli->GetActorLocation().ToString(),*Pilot->GetActorLocation().ToString(),Passenger?*Passenger->GetActorLocation().ToString():TEXT("none"),Ops->RecoveryCableLength));
		TestTrue(TEXT("Passenger reaches cabin before pilot"),PassengerFirst);
		TestTrue(TEXT("Automatic harness returns pilot to cockpit"),PC->GetPawn()==Heli);
		TestFalse(TEXT("Recovery releases flight control after boarding"),Ops->IsRecoveringPilot());
		if (Passenger)
		{
			TestTrue(TEXT("Same passenger is alive in the cabin"),Passenger->GetBehaviorCarrier()==Heli && !Passenger->IsMissionPatientDead());
			if (PersonState==10) TestTrue(TEXT("Rescued robber remains handcuffed"),Passenger->IsHandcuffed());
			Passenger->Destroy();
		}
	}
	const auto Crack=USimCopterAudioSubsystem::BuildBoneCrack();
	TestTrue(TEXT("Severe fall has finite generated audio"),Crack.IsValid() && Crack.Duration>.2f && Crack.Duration<.5f);
	int32 Peak=0; for (int32 I=0; I<Crack.Pcm16.Num(); I+=2) { int16 S; FMemory::Memcpy(&S,Crack.Pcm16.GetData()+I,2); Peak=FMath::Max(Peak,FMath::Abs(int32(S))); }
	TestTrue(TEXT("Bone impact is audible PCM with headroom"),Peak>5000 && Peak<32767);
	if (FParse::Param(FCommandLine::Get(),TEXT("RemoteRecoveryPreview")))
	{
		DeferredCleanup=true;
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this,World,Heli,Ops]()
		{
			ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
			const FString Output=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/remote-taser-recovery");
			auto* Art=NewObject<USimCopterHangarArt>(Heli);
			Art->SetOriginalGameRoot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame")));
			for (int32 Active=0;Active<2;++Active)
			{
				Ops->bRemoteTaserActive=Active!=0;
				auto Controls=SNew(SSimCopterToolFlaps).Pawn(Heli).Art(Art);
				FWidgetRenderer Renderer(true);
				auto* Target=Renderer.DrawWidget(Controls,FVector2D(440,950));
				TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
				Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
				TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Target->SizeX,Target->SizeY,Pixels,Png);
				FFileHelper::SaveArrayToFile(Png,*(Output/FString::Printf(TEXT("taser-controls-%d.png"),Active)));
			}
			Ops->bRemoteTaserActive=false;
			// This unstarted fixture does not dispatch passenger EndPlay on Destroy.
			// Clear its dead test occupants before switching to the one-seat Apache.
			Heli->MissionPassengerSlots.Reset(); Heli->FlightModel.Passengers=0;
			auto* Rig=World->SpawnActor<AActor>();
			auto* Capture=NewObject<USceneCaptureComponent2D>(Rig);
			Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
			Capture->CaptureSource=SCS_BaseColor; Capture->bAlwaysPersistRenderingState=true;
			Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false); Capture->ShowFlags.SetBloom(false);
			Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
			auto* Target=NewObject<UTextureRenderTarget2D>(Rig);
			Target->RenderTargetFormat=RTF_RGBA8; Target->InitAutoFormat(700,500); Target->UpdateResourceImmediate(true);
			Capture->TextureTarget=Target; Capture->RegisterComponent();
			Heli->SetActorLocation(FVector(0,0,500)); Heli->SetActorRotation(FRotator::ZeroRotator);
			for (int32 Model=0;Model<9;++Model)
			{
				if (!TestTrue(TEXT("Preview fleet model loads"),Heli->SwitchHelicopterModel(Model))) return;
				Heli->UpdateVisuals(0); Ops->UpdateRemoteTaserMount();
				Capture->ClearShowOnlyComponents(); Capture->ShowOnlyActorComponents(Heli);
				const FVector Mount=Ops->RemoteTaserMount->GetComponentLocation();
				const FVector Eye=Mount+FVector(165,200,90);
				Capture->SetWorldLocationAndRotation(Eye,(Mount-FVector(25,0,-5)-Eye).Rotation()); Capture->FOVAngle=48;
				World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
				if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
				FAssetCompilingManager::Get().FinishAllCompilation(); FlushRenderingCommands();
				Capture->CaptureScene(); FlushRenderingCommands();
				TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
				for (auto& P:Pixels) P.A=255;
				TestTrue(TEXT("Weapon geometry preview saved"),FImageUtils::SaveImageByExtension(*(Output/FString::Printf(TEXT("nose-taser-%d.png"),Model)),FImageView(Pixels.GetData(),700,500)));
			}
		},1.f));
	}
	return true;
}
#endif
