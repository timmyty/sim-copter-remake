#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterControllerInput.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterCriminalCar.h"
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
	auto* ControllerInput=NewObject<UInputComponent>(Heli); Heli->SetupPlayerInputComponent(ControllerInput);
	const auto ControllerKey=[&](FKey Key,EInputEvent Event=IE_Pressed)
	{
		for (auto& Binding:ControllerInput->KeyBindings)
			if (Binding.Chord.Key==Key && Binding.KeyEvent==Event) Binding.KeyDelegate.Execute(Key);
	};
	const auto SelectRemoteOption=[&]()
	{
		ControllerKey(EKeys::Gamepad_FaceButton_Left);
		const FVector2D Direction=SimCopterControllerInput::GetRadialSlotDirection(Heli->GetControllerRemoteTaserSlot(),Heli->GetControllerToolWheelSlotCount());
		Heli->ControllerRightX(Direction.X); Heli->ControllerRightY(Direction.Y);
		const FRotator BeforeMenuAim=Ops->RemoteTaserAim;
		Ops->UpdateRemoteTaserControls(0,0,Direction.X,Direction.Y,.1f);
		TestTrue(TEXT("Tool selection owns the stick without also moving remote aim"),Ops->RemoteTaserAim.Equals(BeforeMenuAim));
		ControllerKey(EKeys::Gamepad_FaceButton_Bottom);
		ControllerKey(EKeys::Gamepad_FaceButton_Bottom,IE_Released);
		Heli->ControllerRightX(0); Heli->ControllerRightY(0);
	};
	SelectRemoteOption();
	TestTrue(TEXT("Empty helicopter enters remote aim without officer or purchase"),Ops->IsRemoteTaserActive());
	TestEqual(TEXT("Selecting Remote Taser does not also fire"),Ops->ShotCooldown,0.0f);
	ControllerKey(EKeys::Gamepad_FaceButton_Bottom);
	TestTrue(TEXT("A fires the active remote taser"),Ops->ShotCooldown>0);
	ControllerKey(EKeys::Gamepad_FaceButton_Bottom,IE_Released);
	Ops->Update(.05f);
	TestTrue(TEXT("Weapon gimbal and sight share the firing direction"),Ops->RemoteTaserWeapon->GetForwardVector().Equals(Ops->GetRemoteTaserDirection(),.001));
	TestTrue(TEXT("Readout announces a steady hover and magnification"),Ops->GetRemoteTaserReadout().Contains(TEXT("HOLD")) && Ops->GetRemoteTaserReadout().Contains(TEXT("1.0x")));
	Ops->AimRemoteTaser(1,0,3); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Taser yaw moves beyond the former 100-degree limit"),Ops->RemoteTaserAim.Yaw>130);
	Ops->AimRemoteTaser(1,0,1); Ops->UpdateRemoteTaser(0);
	const FVector BeforeYawWrap=Ops->GetRemoteTaserDirection();
	Ops->AimRemoteTaser(1,0,.1f); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Yaw wraps smoothly through the rearward boundary"),Ops->RemoteTaserAim.Yaw<0 && FVector::DotProduct(BeforeYawWrap,Ops->GetRemoteTaserDirection())>.99);
	Ops->AimRemoteTaser(0,1,10); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Taser aims nearly straight up"),FMath::IsNearlyEqual(Ops->RemoteTaserAim.Pitch,85.f,.1f));
	Ops->AimRemoteTaser(0,1,10); Ops->AimRemoteTaser(0,-1,.1f); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Reversing at the upper stop responds immediately"),FMath::IsNearlyEqual(Ops->RemoteTaserAim.Pitch,81.5f,.1f));
	Ops->AimRemoteTaser(0,-1,10); Ops->UpdateRemoteTaser(0);
	TestTrue(TEXT("Taser aims nearly straight down"),FMath::IsNearlyEqual(Ops->RemoteTaserAim.Pitch,-85.f,.1f));
	TestTrue(TEXT("Weapon still follows the expanded sight direction"),Ops->RemoteTaserWeapon->GetForwardVector().Equals(Ops->GetRemoteTaserDirection(),.001));
	TestFalse(TEXT("Remote sight captures mouse for direct aiming"),PC->bShowMouseCursor);
	// A clear nearby suspect should not make the platform continually hunt for a new vantage.
	const FVector StablePosition=Heli->GetActorLocation();
	const FRotator StableFacing=Heli->GetActorRotation();
	for (int32 Frame=0; Frame<240; ++Frame) Ops->UpdateRemoteTaser(1.f/60);
	TestTrue(TEXT("Clear in-range target holds a stable firing platform for four seconds"),Heli->GetActorLocation().Equals(StablePosition,.1));
	TestTrue(TEXT("Hover does not turn the nose under the sight"),Heli->GetActorRotation().Equals(StableFacing,.01));
	const FVector StableSuspectPosition=Suspect->GetActorLocation();
	for (int32 Frame=0; Frame<120; ++Frame)
	{
		Suspect->SetActorLocation(StableSuspectPosition+FVector(0,Frame*5,0));
		Ops->UpdateRemoteTaser(1.f/60);
	}
	TestTrue(TEXT("Small target movement does not reposition the hover"),Heli->GetActorLocation().Equals(StablePosition,.1));
	Suspect->SetActorLocation(StableSuspectPosition);
	auto* BriefCover=Box(FMath::Lerp(Ops->GetRemoteTaserMuzzle(),StableSuspectPosition,.5),FVector(60));
	TestFalse(TEXT("Transient-cover fixture blocks the sight"),Ops->HasClearLine(Ops->GetRemoteTaserMuzzle(),StableSuspectPosition,Suspect));
	for (int32 Frame=0; Frame<24; ++Frame) Ops->UpdateRemoteTaser(1.f/60);
	TestTrue(TEXT("Brief cover does not disturb the firing platform"),Heli->GetActorLocation().Equals(StablePosition,.1));
	BriefCover->GetOwner()->Destroy(); Ops->UpdateRemoteTaser(.1f);
	TestEqual(TEXT("Clear sight resets the obstruction grace period"),Ops->RemoteObstructedSeconds,0.f);
	Suspect->SetActorLocation(StablePosition+FVector(3800,0,-1700));
	Ops->UpdateRemoteTaser(.1f);
	TestTrue(TEXT("Hover hysteresis avoids repositioning just beyond acquisition range"),Heli->GetActorLocation().Equals(StablePosition,.1));
	Suspect->SetActorLocation(StablePosition+FVector(5200,0,-1700));
	for (int32 Frame=0; Frame<24; ++Frame) Ops->UpdateRemoteTaser(1.f/60);
	TestTrue(TEXT("Pursuit resumes before the target leaves weapon range"),!Ops->bRemoteHolding && !Heli->GetActorLocation().Equals(StablePosition,1));
	TestTrue(TEXT("Following retains the chosen vantage instead of half-second replans"),Ops->RemotePursuitPlanSeconds>1.5f);
	TestTrue(TEXT("Following does not swing the nose camera"),Heli->GetActorRotation().Equals(StableFacing,.01));
	Heli->SetAutopilotTransform(StablePosition,StableFacing,.05f);
	Suspect->SetActorLocation(StableSuspectPosition); Ops->UpdateRemoteTaser(.05f);
	// Zoom goes through the bound camera axis, preserving the normal flight camera.
	const float FlightZoom=Heli->CameraZoomAlpha;
	const auto ZoomAxis=[&](float Value)
	{
		for (auto& Binding:ControllerInput->AxisBindings)
			if (Binding.AxisName==TEXT("SimCopterCameraZoom")) Binding.AxisDelegate.Execute(Value);
	};
	const auto SettleScope=[&]() { for (int32 Frame=0; Frame<120; ++Frame) Ops->UpdateRemoteTaser(1.f/60); };
	ZoomAxis(100); SettleScope();
	TestTrue(TEXT("Wheel clamps scope at four-times magnification"),FMath::IsNearlyEqual(Ops->GetRemoteTaserZoom(),4.f,.001f));
	TestEqual(TEXT("Scope wheel leaves flight camera zoom unchanged"),Heli->CameraZoomAlpha,FlightZoom);
	const float ScopeFov=Ops->RemoteTaserCamera->GetCameraComponent()->FieldOfView;
	TestTrue(TEXT("Four-times scope uses optical projection magnification"),FMath::IsNearlyEqual(FMath::Tan(FMath::DegreesToRadians(22.5f))/FMath::Tan(FMath::DegreesToRadians(ScopeFov*.5f)),4.f,.001f));
	TestTrue(TEXT("Scope retains the muzzle centre ray"),Ops->RemoteTaserCamera->GetActorLocation().Equals(Ops->GetRemoteTaserMuzzle(),.001) && Ops->RemoteTaserCamera->GetActorForwardVector().Equals(Ops->GetRemoteTaserDirection(),.001));
	Ops->RemoteTaserAim=FRotator::ZeroRotator; Ops->UpdateRemoteTaserControls(4,0,0,0,1.f/60);
	const float ScopedMouseYaw=Ops->RemoteTaserAim.Yaw;
	Ops->RemoteTaserAim=FRotator::ZeroRotator; Ops->AimRemoteTaser(1,0,.1f);
	const float ScopedStickYaw=Ops->RemoteTaserAim.Yaw;
	ZoomAxis(-100); SettleScope();
	TestTrue(TEXT("Wheel clamps scope at the original field of view"),FMath::IsNearlyEqual(Ops->RemoteTaserCamera->GetCameraComponent()->FieldOfView,45.f,.001f));
	Ops->RemoteTaserAim=FRotator::ZeroRotator; Ops->UpdateRemoteTaserControls(4,0,0,0,1.f/60);
	TestTrue(TEXT("Zoom reduces mouse sensitivity by magnification"),FMath::IsNearlyEqual(Ops->RemoteTaserAim.Yaw,ScopedMouseYaw*4,.001f));
	Ops->RemoteTaserAim=FRotator::ZeroRotator; Ops->AimRemoteTaser(1,0,.1f);
	TestTrue(TEXT("Zoom reduces stick sensitivity by magnification"),FMath::IsNearlyEqual(Ops->RemoteTaserAim.Yaw,ScopedStickYaw*4,.001f));
	ZoomAxis(2); SettleScope();
	Heli->ControllerLeftTrigger(1); SettleScope();
	TestTrue(TEXT("Holding LT engages four-times scope"),FMath::IsNearlyEqual(Ops->GetRemoteTaserZoom(),4.f,.001f));
	Heli->ControllerLeftTrigger(0); SettleScope();
	TestTrue(TEXT("Releasing LT restores wheel-selected zoom"),FMath::IsNearlyEqual(Ops->GetRemoteTaserZoom(),2.f,.001f));
	ControllerKey(EKeys::Gamepad_FaceButton_Left);
	ZoomAxis(3); Heli->ControllerLeftTrigger(1); SettleScope();
	TestTrue(TEXT("Tool menu blocks both scope inputs"),FMath::IsNearlyEqual(Ops->GetRemoteTaserZoom(),2.f,.001f) && Ops->RemoteScopeZoomTarget==2.f);
	ControllerKey(EKeys::Gamepad_FaceButton_Right); Heli->ControllerLeftTrigger(0);
	float ZoomAt30=0;
	for (int32 Rate : {30,60,120})
	{
		Ops->RemoteScopeZoom=1; Ops->RemoteScopeZoomTarget=4;
		for (int32 Frame=0; Frame<Rate/5; ++Frame) Ops->UpdateRemoteTaser(1.f/Rate);
		if (Rate==30) ZoomAt30=Ops->GetRemoteTaserZoom();
		else TestTrue(TEXT("Scope transition is frame-rate independent"),FMath::IsNearlyEqual(Ops->GetRemoteTaserZoom(),ZoomAt30,.001f));
	}
	ZoomAxis(-100); SettleScope();
	Ops->RemoteTaserAim=FRotator::ZeroRotator;
	Ops->UpdateRemoteTaserControls(6,-2,0,0,1.f/30);
	const FRotator MouseAt30=Ops->RemoteTaserAim;
	Ops->RemoteTaserAim=FRotator::ZeroRotator;
	Ops->UpdateRemoteTaserControls(3,-1,0,0,1.f/60); Ops->UpdateRemoteTaserControls(3,-1,0,0,1.f/60);
	TestTrue(TEXT("Equal mouse travel aims equally at 30 and 60 fps"),Ops->RemoteTaserAim.Equals(MouseAt30,.001));
	TestTrue(TEXT("Mouse movement aims without right mouse held"),MouseAt30.Yaw>0 && MouseAt30.Pitch>0);
	ZoomAxis(100); SettleScope();
	Ops->RemoteTaserAim=(Suspect->GetActorLocation()-Ops->RemoteTaserMount->GetComponentLocation()).Rotation();
	Ops->UpdateRemoteTaser(0); Ops->ShotCooldown=0; Ops->FireRemoteTaser();
	TestTrue(TEXT("A fully zoomed centre-reticle shot stuns the aimed suspect"),Suspect->IsTaserStunned());
	Ops->SetRemoteTaserCursor(true);
	TestTrue(TEXT("Cockpit cursor can be released from the sight"),PC->bShowMouseCursor);
	SelectRemoteOption();
	TestFalse(TEXT("Return to flight stops orbit"),Ops->IsAIPiloted());
	TestEqual(TEXT("Leaving remote mode clears scope zoom"),Ops->GetRemoteTaserZoom(),1.f);
	TestTrue(TEXT("Return to flight restores the cockpit cursor"),PC->bShowMouseCursor);
	auto Panel=SNew(SSimCopterToolFlaps).Pawn(Heli);
	TestFalse(TEXT("Cockpit controls cannot steal keyboard focus"),Panel->SupportsKeyboardFocus());
	TestTrue(TEXT("Taser panel visible without equipment"),Panel->BuildRemoteTaserFlap()->GetVisibility()!=EVisibility::Collapsed);
	Suspect->Destroy();

	// Pursue a real moving criminal car, with ordinary traffic closer to the helicopter.
	const auto MakeCar=[&](FVector Position,bool bCriminal)
	{
		auto* Car=World->SpawnActor<ASimCopterGroundAgent>();
		// Match the fastest authored road speed (city tile / 64), before the fleeing multiplier.
		Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),SimCopterCriminalCar::RoadSpeedMaxUnitsPerSecond*6.25f);
		Car->SetActorLocation(Position); Car->bSnapToGround=false;
		if (bCriminal) Car->MakeCriminalCar(42,0);
		return Car;
	};
	auto* CivilianCar=MakeCar(FVector(800,0,100),false);
	CivilianCar->SetFleeing(true); // Generic avoidance/fleeing must not invent a crime.
	Heli->ToggleRemoteTaser();
	const FVector NoCrimePosition=Heli->GetActorLocation();
	const FRotator NoCrimeAim=Ops->RemoteTaserAim;
	Ops->UpdateRemoteTaser(1.f);
	TestFalse(TEXT("Fleeing ordinary traffic does not create a pursuit"),Ops->RemoteTaserTarget.IsValid());
	TestTrue(TEXT("No-crime remote mode holds the aircraft and sight"),Heli->GetActorLocation().Equals(NoCrimePosition) && Ops->RemoteTaserAim.Equals(NoCrimeAim));
	Heli->ToggleRemoteTaser();
	auto* BurglarCar=MakeCar(FVector(2800,0,100),true);
	Heli->SetActorLocationAndRotation(FVector(0,0,1800),FRotator::ZeroRotator); Heli->SeedFlightModelFromActor();
	FVector EntryEye; FRotator EntryAim; PC->GetPlayerViewPoint(EntryEye,EntryAim);
	const FVector EntryPosition=Heli->GetActorLocation();
	Heli->ToggleRemoteTaser();
	TestTrue(TEXT("Automatic orbit acquires criminal cars instead of ordinary traffic"),Ops->RemoteTaserTarget.Get()==BurglarCar);
	TestTrue(TEXT("Entering remote mode neither teleports nor snaps the sight to a criminal"),Heli->GetActorLocation().Equals(EntryPosition) && Ops->RemoteTaserAim.Equals(EntryAim));
	BurglarCar->SetMoveTarget(FVector(40000,0,100));
	bool bExceededTurbo=false;
	for (int32 Frame=0; Frame<200; ++Frame)
	{
		const FVector Before=Heli->GetActorLocation(); BurglarCar->Tick(.05f); Ops->UpdateRemoteTaser(.05f);
		const float Turbo=Heli->GetMaxForwardSpeedCmPerSec()*SimCopterFixed::ToFloat(Heli->FlightModel.BaseTravelScale)*SimCopterFixed::ToFloat(FSimCopterFlightModel::TravelBoostScale);
		bExceededTurbo|=FVector::Distance(Before,Heli->GetActorLocation())>Turbo*.05f+.01f;
	}
	TestFalse(TEXT("Every pursuit step is bounded by the actual airframe turbo speed"),bExceededTurbo);
	TestTrue(TEXT("Fixture getaway car moves faster than the old 350 cm/s orbit"),BurglarCar->GetCurrentVelocityCmPerSec().Size2D()>350);
	TestTrue(TEXT("Helicopter keeps a moving getaway car in taser range"),FVector::Distance(Heli->GetActorLocation(),BurglarCar->GetActorLocation())<5500);
	TestTrue(TEXT("Moving-car pursuit never drags the player's sight"),Ops->RemoteTaserAim.Equals(EntryAim));
	Ops->AimRemoteTaser(.25f,0,.2f);
	const FRotator SightOffset=Ops->RemoteTaserAim;
	for (int32 Frame=0; Frame<20; ++Frame) { BurglarCar->Tick(.05f); Ops->UpdateRemoteTaser(.05f); }
	TestTrue(TEXT("Aircraft pursuit preserves manual world aim"),Ops->RemoteTaserAim.Equals(SightOffset));
	BurglarCar->DisableVehicleWithTaser(); Ops->UpdateRemoteTaser(.3f);
	TestTrue(TEXT("Temporary taser stop keeps the car tracked"),Ops->RemoteTaserTarget.Get()==BurglarCar);
	auto* Burglar=World->SpawnActor<ASimCopterGroundAgent>(); Burglar->InitialPersonState=13; Burglar->BehaviorContext.ResetToState(13);
	Burglar->SetActorLocation(BurglarCar->GetActorLocation()+FVector(0,200,0)); Burglar->SetBehaviorStartingVehicle(BurglarCar);
	Ops->UpdateRemoteTaser(.3f);
	TestTrue(TEXT("Pursuit transfers from getaway car to its robber on foot"),Ops->RemoteTaserTarget.Get()==Burglar);
	TestTrue(TEXT("Car-to-person handoff cannot redirect the sight"),Ops->RemoteTaserAim.Equals(SightOffset));
	BurglarCar->SetActorHiddenInGame(true);
	const FVector RobberStart=Burglar->GetActorLocation();
	for (int32 Frame=0; Frame<100; ++Frame)
	{
		Burglar->SetActorLocation(RobberStart+FVector(0,Frame*15,0)); Burglar->CurrentVelocityCmPerSec=FVector(0,300,0);
		Ops->UpdateRemoteTaser(.05f);
	}
	TestTrue(TEXT("Helicopter keeps a running robber within the stable firing range"),FVector::Distance(Heli->GetActorLocation(),Burglar->GetActorLocation())<5500);
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
	Ops->RemoteTargetSearchSeconds=0; Ops->UpdateRemoteTaser(0);
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

	// A free gimbal can look back at a suspect while pursuit flies away to its vantage.
	// Returning to flight must face the live suspect, including after a successful stun.
	PC->PlayerCameraManager=World->SpawnActor<APlayerCameraManager>();
	PC->PlayerCameraManager->InitializeFor(PC);
	for (int32 Model=0; Model<9; ++Model)
	{
		Heli->SwitchHelicopterModel(Model);
		for (bool bVehicle : {false,true})
		{
			auto* Target=bVehicle ? MakeCar(FVector(1200,-600,100),true) : World->SpawnActor<ASimCopterGroundAgent>();
			if (!bVehicle) { Target->InitialPersonState=13; Target->BehaviorContext.ResetToState(13); Target->SetActorLocation(FVector(1200,-600,24)); }
			Heli->SetAutopilotTransform(FVector(-300,200,1800),FRotator(0,150,0),.05f);
			Heli->ToggleRemoteTaser();
			TestTrue(TEXT("Exit-heading fixture acquires the criminal"),Ops->RemoteTaserTarget.Get()==Target);
			Ops->RemoteTaserAim=FRotator(-30,80,0); // Free aim need not face the pursuit target.
			Target->AddActorWorldOffset(FVector(100,250,0)); // Use current position, not the last pursuit update.
			const FVector Expected=(Target->GetActorLocation()-Heli->GetActorLocation()).GetSafeNormal2D();
			const FVector ExitPosition=Heli->GetActorLocation(), ExitVelocity=Heli->VelocityCmPerSec;
			const int32 ExitHealth=Heli->FlightModel.HitPoints;
			Heli->ToggleRemoteTaser();
			const FString Case=FString::Printf(TEXT("Model %d %s"),Model,bVehicle?TEXT("car"):TEXT("person"));
			TestTrue(Case+TEXT(" returns facing the live criminal"),FVector::DotProduct(Heli->GetActorForwardVector(),Expected)>.9999f);
			TestTrue(Case+TEXT(" exit preserves position and velocity"),Heli->GetActorLocation().Equals(ExitPosition) && Heli->VelocityCmPerSec.Equals(ExitVelocity));
			TestTrue(Case+TEXT(" restores aircraft view and visibility"),PC->GetViewTarget()==Heli && !PC->HiddenActors.Contains(Heli));
			Heli->Tick(.05f);
			TestTrue(Case+TEXT(" flight tick preserves the target-facing heading"),FVector::DotProduct(Heli->GetActorForwardVector(),Expected)>.9999f);
			TestEqual(Case+TEXT(" exit preserves health"),Heli->FlightModel.HitPoints,ExitHealth);
			if (!bVehicle)
			{
				Heli->SetAutopilotTransform(ExitPosition,FRotator(0,150,0),.05f);
				Heli->ToggleRemoteTaser(); Target->StunForArrest(); Ops->UpdateRemoteTaser(.3f);
				TestFalse(TEXT("Stun ends pursuit before returning to flight"),Ops->RemoteTaserTarget.IsValid());
				Heli->ToggleRemoteTaser();
				TestTrue(Case+TEXT(" returns facing the just-stunned criminal"),FVector::DotProduct(Heli->GetActorForwardVector(),Expected)>.9999f);
			}
			Target->Destroy();
		}
	}
	Heli->SwitchHelicopterModel(4);
	Heli->SetAutopilotTransform(FVector(0,0,1800),FRotator(0,-125,0),.05f);
	Heli->ToggleRemoteTaser(); Ops->RemoteTaserAim=FRotator(-30,55,0);
	const FRotator EmptyExitRotation=Heli->GetActorRotation();
	Heli->ToggleRemoteTaser();
	TestTrue(TEXT("A new session without criminals preserves flight heading"),Heli->GetActorRotation().Equals(EmptyExitRotation));
	auto* ExitSuspect=World->SpawnActor<ASimCopterGroundAgent>(); ExitSuspect->InitialPersonState=13; ExitSuspect->BehaviorContext.ResetToState(13);
	ExitSuspect->SetActorLocation(FVector(0,0,24)); Heli->ToggleRemoteTaser(); Heli->ToggleRemoteTaser();
	TestTrue(TEXT("A suspect directly below cannot invent a horizontal heading"),Heli->GetActorRotation().Equals(EmptyExitRotation));
	ExitSuspect->SetActorLocation(FVector(1000,500,24)); Heli->ToggleRemoteTaser(); Ops->EndRemoteTaser();
	TestTrue(TEXT("Cleanup without a player return does not rotate the aircraft"),Heli->GetActorRotation().Equals(EmptyExitRotation));
	Heli->ToggleRemoteTaser(); ExitSuspect->Destroy(); Heli->ToggleRemoteTaser();
	TestTrue(TEXT("A destroyed target cannot rotate the returning aircraft"),Heli->GetActorRotation().Equals(EmptyExitRotation));

	// Reproduce Space held during startup/exit; test the actual possession transition and ticks.
	Heli->SetActorLocation(FVector(0,0,800)); Heli->SeedFlightModelFromActor();
	Heli->bIsLanded=false; Heli->bEngineRunning=true; Heli->MoveCollective(1);
	const int32 Health=Heli->FlightModel.HitPoints;
	ControllerKey(EKeys::Gamepad_FaceButton_Top);
	auto* Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (!TestNotNull(TEXT("Pilot can exit"),Pilot)) return false;
	TestFalse(TEXT("Space and collective latches cleared on exit"),Heli->bEngineStartHeld || Heli->CollectiveInput!=0 || Heli->bControllerEngineStartHeld);
	const double HoldZ=Heli->GetActorLocation().Z;
	for (int32 I=0; I<20; ++I) Heli->Tick(.05f);
	TestEqual(TEXT("Held Space cannot keep unattended helicopter climbing"),Heli->GetActorLocation().Z,HoldZ);
	TestEqual(TEXT("Exit preserves airframe health"),Heli->FlightModel.HitPoints,Health);
	// This fixture ticks only the aircraft; place the pilot at the end of the fall.
	Pilot->SetActorLocation(FVector(0,200,24)); Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
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
	// The pilot falls past a roof. A clear roof under the exit is not a reachable pickup spot.
	auto* ExitRoof=Box(FVector(3000,0,980),FVector(500,500,20));
	Pilot->SetActorLocation(FVector(3800,0,24)); Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Heli->SetAutopilotTransform(FVector(3000,0,1600),FRotator::ZeroRotator,.05f);
	Heli->bIsLanded=false; Ops->BeginPilotRecovery(Pilot); Ops->RecoveryHoldSeconds=0;
	Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Ops->Update(.05f);
	TestTrue(TEXT("A long fall cannot lock in an intermediate rooftop landing"),Ops->IsRecoveringPilot() && !Ops->bHasRecoveryLandingGoal);
	Pilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	// Loading an older save can restore the roof that the old search had already chosen.
	bool RoofCanLand=false;
	Ops->FindRecoverySurface(Ops->RecoveryLandingGoal,RoofCanLand);
	TestTrue(TEXT("Exit roof is physically clear but above the pilot"),RoofCanLand && Ops->RecoveryLandingGoal.Z>1000);
	Ops->bHasRecoveryLandingGoal=true;
	Ops->Update(.05f);
	TestTrue(TEXT("No-harness landing chooses the pilot's ground level instead of the exit roof"),
		Ops->bHasRecoveryLandingGoal && Ops->RecoveryLandingGoal.Z<300);
	for (int32 I=0; I<1000 && Ops->IsRecoveringPilot(); ++I) Heli->Tick(.05f);
	TestTrue(TEXT("Airborne exit returns the aircraft to ground near the pilot"),
		Heli->bIsLanded && Heli->GetActorLocation().Z<300 && FVector::Dist2D(Heli->GetActorLocation(),Pilot->GetActorLocation())<1000);
	ExitRoof->GetOwner()->Destroy();

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
	// Regress the player controls and repeated recovery with the real pawn tick paths.
	for (int32 Scenario=0; Scenario<5; ++Scenario)
	{
		const auto RescueInit=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
		auto* RescueWorld=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&RescueInit);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(RescueWorld);
		ON_SCOPE_EXIT { GEngine->DestroyWorldContext(RescueWorld); RescueWorld->DestroyWorld(false); };
		const auto RescueBox=[&](FVector Location,FVector Half)
		{
			auto* Actor=RescueWorld->SpawnActor<AActor>(); auto* Shape=NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Shape); Shape->SetBoxExtent(Half); Shape->SetCollisionResponseToAllChannels(ECR_Block);
			Shape->RegisterComponent(); Actor->SetActorLocation(Location); return Shape;
		};
		auto* RescueFloor=RescueBox(FVector(0,0,-20),FVector(10000,10000,20));
		auto* RescueStep=RescueBox(FVector(110,0,35),FVector(60,600,35));
		auto* RescueHeli=RescueWorld->SpawnActor<ASimCopterHelicopterPawn>(); RescueHeli->SwitchHelicopterModel(4);
		RescueHeli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::RescueHarness,true);
		auto* RescueOps=RescueHeli->GetAirOperations();
		auto* RescuePC=RescueWorld->SpawnActor<ASimCopterPlayerController>(); RescuePC->Player=NewObject<ULocalPlayer>(GEngine); RescueWorld->AddController(RescuePC);
		RescuePC->Possess(RescueHeli); RescueHeli->SetAutopilotTransform(FVector(0,0,600),FRotator::ZeroRotator,.05f);
		RescueHeli->ExitHelicopter();
		auto* RescuePilot=Cast<ASimCopterOnFootPawn>(RescuePC->GetPawn());
		if (!TestNotNull(TEXT("Airborne exit creates a pilot"),RescuePilot)) return false;
		RescuePilot->SetActorLocation(FVector(0,Scenario==1?2200:600,94));
		RescuePilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking); RescueOps->RecoveryHoldSeconds=0;
		ASimCopterGroundAgent* Patient=nullptr;
		if (Scenario==0 || Scenario==3 || Scenario==4)
		{
			Patient=RescueWorld->SpawnActor<ASimCopterGroundAgent>(); Patient->InitialPersonState=6;
			Patient->BehaviorContext.ResetToState(6); Patient->BehaviorContext.Attributes[EBhavAttr::MedevacHealth]=100;
			Patient->SetActorLocation(RescuePilot->GetActorLocation());
			TestTrue(TEXT("RescuePilot carries the injured patient"),RescuePilot->PickUpMissionPerson(Patient));
		}
		if (Scenario==3)
		{
			// A grounded pilot on a ledge can be recovered even with no support under the aircraft.
			RescueStep->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			RescueFloor->SetBoxExtent(FVector(300,120,20)); RescueFloor->GetOwner()->SetActorLocation(FVector(0,600,-20));
			FVector Goal; bool CanLand=false;
			TestFalse(TEXT("Ledge fixture has no landing surface below the aircraft"),RescueOps->FindRecoverySurface(Goal,CanLand));
		}
		if (Scenario==2)
		{
			RescueStep->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// Clear support below, but an overhang blocks the descent between the ground probes.
			auto* Overhang=RescueBox(FVector(0,0,300),FVector(300,100,20));
			Overhang->SetCollisionResponseToChannel(ECC_Camera,ECR_Ignore);
			FVector Goal; bool CanLand=false;
			TestTrue(TEXT("Landing support alone passes below the overhang"),RescueOps->FindRecoverySurface(Goal,CanLand) && CanLand);
			TestFalse(TEXT("Full descent detects the overhang"),RescueOps->IsRecoveryRouteClear(RescueHeli->GetActorLocation(),Goal));
		}
		bool bPressedDrop=false, bWalkedUp=false, bPassengerFirst=false, bHarnessStarted=false;
		for (int32 Frame=0; Frame<1400 && RescuePC->GetPawn()!=RescueHeli; ++Frame)
		{
			// Use the actual pawn ticks, including patient attachment/behavior and auto-boarding.
			RescueHeli->Tick(.05f);
			bHarnessStarted|=RescueOps->bRecoveryHarnessActive;
			if (Patient)
			{
				Patient->Tick(.05f);
				FVector End;
				if (Scenario==0 && !bPressedDrop && RescuePilot->IsCarryingMissionPerson() && RescueHeli->TryGetRopeEndWorldLocation(End) &&
					FVector::Distance(End,Patient->GetActorLocation())<=90)
				{
					const int32 Seats=RescueHeli->GetAvailablePassengerSeats();
					RescueHeli->AddMissionPassengers(Seats);
					TestFalse(TEXT("Full cabin refuses a harness handoff"),RescueOps->TryLoadRecoveryPassenger(RescuePilot));
					TestTrue(TEXT("Failed handoff preserves the carried patient"),RescuePilot->GetCarriedMissionPerson()==Patient && !Patient->GetBehaviorCarrier());
					RescueHeli->RemoveMissionPassengers(Seats);
					bPressedDrop=true;
					auto* Input=NewObject<UInputComponent>(RescuePilot); RescuePilot->SetupPlayerInputComponent(Input);
					for (auto& Binding:Input->KeyBindings)
						if (Binding.Chord.Key==EKeys::G && Binding.KeyEvent==IE_Pressed) Binding.KeyDelegate.Execute(EKeys::G);
					TestTrue(TEXT("Drop beside the lowered harness attaches the same patient"),Patient->GetBehaviorCarrier()==RescueHeli && Patient->IsRidingHarness());
					TestFalse(TEXT("Harness handoff releases the pilot's carry reference"),RescuePilot->IsCarryingMissionPerson());
				}
				if (Scenario==4 && !bWalkedUp && RescuePilot->IsCarryingMissionPerson() && RescueHeli->TryGetRopeEndWorldLocation(End) &&
					FVector::Distance(End,Patient->GetActorLocation())<65)
				{
					bWalkedUp=true;
					const FVector BeforeWalk=RescuePilot->GetActorLocation();
					RescuePilot->SetActorLocation(BeforeWalk+FVector(0,200,0)); RescuePilot->Tick(.05f);
					TestTrue(TEXT("Distant patient stays in the pilot's arms"),RescuePilot->GetCarriedMissionPerson()==Patient);
					RescuePilot->SetActorLocation(BeforeWalk+FVector(0,40,0));
					TestTrue(TEXT("Walk-up reaches the sling without exact overhead alignment"),FVector::Distance(End,Patient->GetActorLocation())<=90 && FVector::Dist2D(End,Patient->GetActorLocation())>24);
					const int32 Seats=RescueHeli->GetAvailablePassengerSeats(); RescueHeli->AddMissionPassengers(Seats);
					RescuePilot->Tick(.05f);
					TestTrue(TEXT("Automatic handoff waits for a free cabin seat"),RescuePilot->GetCarriedMissionPerson()==Patient && !Patient->GetBehaviorCarrier());
					RescueHeli->RemoveMissionPassengers(Seats);
					auto* HandoffBlocker=RescueBox((End+Patient->GetActorLocation())*.5,FVector(4));
					RescuePilot->Tick(.05f);
					TestTrue(TEXT("Automatic handoff cannot take the patient through an obstacle"),RescuePilot->GetCarriedMissionPerson()==Patient && !Patient->GetBehaviorCarrier());
					HandoffBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision); HandoffBlocker->GetOwner()->Destroy();
					// No G, controller X, direct handoff call or aircraft realignment: proximity alone loads it.
					RescuePilot->Tick(.05f);
					TestTrue(TEXT("Walking up automatically loads the same patient on the harness"),Patient->GetBehaviorCarrier()==RescueHeli && Patient->IsRidingHarness());
					TestFalse(TEXT("Automatic walk-up releases the pilot's carry reference"),RescuePilot->IsCarryingMissionPerson());
					TestTrue(TEXT("Walk-up loads the patient while the pilot stays on foot"),RescuePC->GetPawn()==RescuePilot && !RescuePilot->IsAutoRescueAttached());
				}
				bPassengerFirst|=Patient->GetBehaviorCarrier()==RescueHeli && !Patient->IsRidingHarness() && RescuePC->GetPawn()==RescuePilot;
			}
			if (RescuePC->GetPawn()==RescuePilot && (Scenario!=4 || bWalkedUp)) RescuePilot->Tick(.05f);
		}
		TestTrue(*FString::Printf(TEXT("Scenario %d deploys harness"),Scenario),bHarnessStarted);
		TestTrue(*FString::Printf(TEXT("Scenario %d retrieves pilot"),Scenario),RescuePC->GetPawn()==RescueHeli);
		if (Scenario==1 && RescuePC->GetPawn()==RescueHeli)
		{
			// A completed automatic lift used to reset the winch to Bucket but leave the
			// displayed rope end at Harness. Saving preserved that disagreement indefinitely.
			TestFalse(TEXT("Completed recovery clears the harness presentation"),RescueHeli->IsHarnessRopeEndSelected());
			RescueHeli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::WaterBucket,true);
			auto* WinchInput=NewObject<UInputComponent>(RescueHeli); RescueHeli->SetupPlayerInputComponent(WinchInput);
			const auto WinchKey=[&](FKey Key,EInputEvent Event=IE_Pressed)
			{
				for (auto& Binding:WinchInput->KeyBindings)
					if (Binding.Chord.Key==Key && Binding.KeyEvent==Event) Binding.KeyDelegate.Execute(Key);
			};
			const auto SelectTool=[&](ESimCopterHelicopterTool Tool)
			{
				WinchKey(EKeys::Gamepad_FaceButton_Left);
				const int32 Slot=RescueHeli->ControllerToolWheelTools.IndexOfByKey(Tool);
				TestTrue(TEXT("Requested installed attachment is in the controller wheel"),Slot!=INDEX_NONE);
				const FVector2D Direction=SimCopterControllerInput::GetRadialSlotDirection(Slot,RescueHeli->GetControllerToolWheelSlotCount());
				RescueHeli->ControllerRightX(Direction.X); RescueHeli->ControllerRightY(Direction.Y);
				RescueHeli->UpdateControllerRadialSelection();
				RescueHeli->ControllerRightX(0); RescueHeli->ControllerRightY(0);
				WinchKey(EKeys::Gamepad_FaceButton_Bottom); WinchKey(EKeys::Gamepad_FaceButton_Bottom,IE_Released);
				TestTrue(TEXT("Controller confirms the requested attachment"),RescueHeli->GetActiveTool()==Tool);
			};
			const auto DriveWinch=[&](FKey Direction)
			{
				WinchKey(Direction);
				for (int32 Step=0; Step<128; ++Step)
				{
					RescueHeli->UpdateControllerToolManipulation(); RescueHeli->StepRopeState();
				}
				WinchKey(Direction,IE_Released); RescueHeli->UpdateControllerToolManipulation();
				RescueHeli->UpdateRopeVisuals(0);
			};
			for (bool bLegacySave : {false,true})
			{
				// Also emulate already-written saves after the producer has been corrected.
				if (bLegacySave) RescueHeli->bHarnessRopeEndSelected=true;
				TArray<uint8> SavedWinch;
				TestTrue(TEXT("Post-recovery aircraft saves"),RescueHeli->CaptureRuntimeSaveState(SavedWinch));
				TestTrue(TEXT("Post-recovery aircraft reloads"),RescueHeli->RestoreRuntimeSaveState(SavedWinch));
				TestFalse(TEXT("Reload reconciles the saved harness presentation with the bucket winch"),RescueHeli->IsHarnessRopeEndSelected());
				SelectTool(ESimCopterHelicopterTool::WaterBucket); DriveWinch(EKeys::Gamepad_DPad_Down);
				TestFalse(TEXT("First bucket selection after reload deploys the bucket"),RescueHeli->WinchState.bBucketStowed);
				TestTrue(TEXT("First bucket selection leaves the harness stowed"),RescueHeli->WinchState.bHarnessStowed);
				TestFalse(TEXT("First bucket selection displays the bucket without a tool round trip"),RescueHeli->IsHarnessRopeEndSelected());
				TestTrue(TEXT("Actual bucket mesh is visible after reload"),RescueHeli->OriginalBucketMeshComponent->IsVisible());
				TestFalse(TEXT("Harness mesh stays hidden for the selected bucket"),RescueHeli->OriginalHarnessMeshComponent->IsVisible());
				SelectTool(ESimCopterHelicopterTool::RescueHarness); DriveWinch(EKeys::Gamepad_DPad_Down);
				TestTrue(TEXT("Switching to harness still exchanges the deployed attachment"),RescueHeli->IsHarnessRopeEndSelected() && !RescueHeli->WinchState.bHarnessStowed);
				TestTrue(TEXT("Deployed harness saves"),RescueHeli->CaptureRuntimeSaveState(SavedWinch));
				TestTrue(TEXT("Deployed harness reloads"),RescueHeli->RestoreRuntimeSaveState(SavedWinch));
				TestTrue(TEXT("Reload preserves a genuinely deployed harness"),RescueHeli->IsHarnessRopeEndSelected() && !RescueHeli->WinchState.bHarnessStowed);
				SelectTool(ESimCopterHelicopterTool::WaterBucket); DriveWinch(EKeys::Gamepad_DPad_Down);
				TestFalse(TEXT("Switching back to bucket displays the bucket"),RescueHeli->IsHarnessRopeEndSelected());
				DriveWinch(EKeys::Gamepad_DPad_Up);
				TestTrue(TEXT("Controller raises both attachments for the next save"),RescueHeli->WinchState.IsFullyStowed());
			}
		}
		if (Scenario==4) TestTrue(TEXT("Test exercised automatic walk-up loading"),bWalkedUp);
		if (Patient)
		{
			if (Scenario==0) TestTrue(TEXT("Test exercised the player's drop action near the harness"),bPressedDrop);
			TestTrue(TEXT("Patient reaches the cabin before the pilot"),bPassengerFirst);
			TestTrue(TEXT("Rescue preserves the living patient in a real seat"),Patient->GetBehaviorCarrier()==RescueHeli && !Patient->IsRidingHarness() && !Patient->IsMissionPatientDead());
		}
		if (Scenario==0 && RescuePC->GetPawn()==RescueHeli)
		{
			// No manual BeginPilotRecovery/reset between rescues: a real second exit must re-arm it.
			RescueHeli->ExitHelicopter(); RescuePilot=Cast<ASimCopterOnFootPawn>(RescuePC->GetPawn());
			if (!TestNotNull(TEXT("RescuePilot exits again after harness retrieval"),RescuePilot)) return false;
			RescuePilot->SetActorLocation(RescueHeli->GetActorLocation()+FVector(0,2200,0));
			RescuePilot->SetActorLocation(FVector(RescuePilot->GetActorLocation().X,RescuePilot->GetActorLocation().Y,94));
			RescuePilot->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			// Uneven landing footprint remains unsuitable after the aircraft followed the first rescue.
			RescueStep->GetOwner()->SetActorLocation(FVector(RescueHeli->GetActorLocation().X+110,RescueHeli->GetActorLocation().Y,35));
			TestTrue(TEXT("Second exit automatically restarts recovery"),RescueOps->IsRecoveringPilot());
			for (int32 Frame=0; Frame<1400 && RescuePC->GetPawn()==RescuePilot; ++Frame)
			{
				RescueHeli->Tick(.05f); Patient->Tick(.05f);
				if (RescuePC->GetPawn()==RescuePilot) RescuePilot->Tick(.05f);
			}
			TestTrue(TEXT("Second exit follows the distant pilot and lowers again"),RescuePC->GetPawn()==RescueHeli);
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
