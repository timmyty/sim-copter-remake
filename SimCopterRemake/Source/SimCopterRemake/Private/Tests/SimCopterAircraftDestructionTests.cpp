#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "City/SimCity2000CityActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterPlaneParts.h"
#include "Ground/SimCopterApachePool.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Formats/SimCopterPeopleReader.h"
#include "UI/SSimCopterPlaneControls.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "ProceduralMeshComponent.h"

namespace
{
UWorld* MakeAircraftTestWorld()
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	return World;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPlaneControlsTest,"SimCopter.AircraftDestruction.PlaneControlsAndPropeller",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterPlaneControlsTest::RunTest(const FString&)
{
	auto* World=MakeAircraftTestWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Plane=World->SpawnActor<ASimCopterHelicopterPawn>();
	TestTrue(TEXT("Original plane loads"),Plane->SwitchHelicopterModel(SimCopterHelicopterRegistry::PlaneTypeIndex));
	Plane->PlaceOnHelipad(FVector::ZeroVector,0); Plane->bEngineRunning=true;
	Plane->TogglePlaneLandingGear(); TestTrue(TEXT("Gear cannot retract on ground"),Plane->IsPlaneLandingGearDown());
	FSimCopterFlightEnvironment Env; FSimCopterFlightInputs Inputs;
	Inputs.bPitchForwardKey=true;
	for (int32 I=0;I<20;++I) Plane->StepFixedWing(.05f,Inputs,Env);
	TestEqual(TEXT("Climb key alone cannot provide thrust"),Plane->FlightModel.ForwardSpeed,0);
	Inputs.ClimbCommand=1;
	for (int32 I=0;I<100;++I) Plane->StepFixedWing(.05f,Inputs,Env);
	TestTrue(TEXT("Throttle plus W takes off"),Plane->FlightModel.State==ESimCopterFlightState::Flying);
	const int32 High=Plane->FlightModel.Altitude;
	const float HeldThrottle=Plane->GetPlaneThrottle();
	Inputs={}; Inputs.bPitchBackKey=true;
	for (int32 I=0;I<10;++I) Plane->StepFixedWing(.05f,Inputs,Env);
	TestTrue(TEXT("S descends"),Plane->FlightModel.Altitude<High);
	TestEqual(TEXT("Pitch does not change throttle"),Plane->GetPlaneThrottle(),HeldThrottle);
	Inputs={}; Inputs.ClimbCommand=-1;
	const int32 Fast=Plane->FlightModel.ForwardSpeed;
	for (int32 I=0;I<30;++I) Plane->StepFixedWing(.05f,Inputs,Env);
	TestTrue(TEXT("Throttle reduction slows plane"),Plane->FlightModel.ForwardSpeed<Fast);
	Plane->TogglePlaneLandingGear(); TestFalse(TEXT("Gear retracts in flight"),Plane->IsPlaneLandingGearDown());
	if (!TestNotNull(TEXT("Animated original parts built"),Plane->PlaneParts.Get())) return false;
	TestFalse(TEXT("Retracted gear hidden"),Plane->PlaneParts->IsGearVisible());
	Plane->PlaneParts->SetPresentationTime(.1f,true,false); const float Angle=Plane->PlaneParts->GetPropellerAngle();
	Plane->PlaneParts->SetPresentationTime(.133333f,true,false);
	TestNotEqual(TEXT("Propeller changes pose at 30 Hz"),Plane->PlaneParts->GetPropellerAngle(),Angle);
	const float Stopped=Plane->PlaneParts->GetPropellerAngle(); Plane->PlaneParts->SetPresentationTime(1,false,true);
	TestEqual(TEXT("Stopped propeller holds pose"),Plane->PlaneParts->GetPropellerAngle(),Stopped);
	TArray<uint8> Data; Plane->SetPlaneThrottle(.63f);
	TestTrue(TEXT("Plane saves"),Plane->CaptureRuntimeSaveState(Data));
	auto* Restored=World->SpawnActor<ASimCopterHelicopterPawn>();
	Restored->RestoreSavedCareerState(Plane->GetHelicopterTypeIndex(),0,0,1,0,0);
	TestTrue(TEXT("Plane restores"),Restored->RestoreRuntimeSaveState(Data));
	TestEqual(TEXT("Throttle persists"),Restored->GetPlaneThrottle(),.63f);
	TestFalse(TEXT("Gear target persists"),Restored->IsPlaneLandingGearDown());
	Data[4]=2; Data.SetNum(Data.Num()-5);
	TestTrue(TEXT("Version-two aircraft save migrates"),Restored->RestoreRuntimeSaveState(Data));
	TestTrue(TEXT("Old saves default to safe gear down"),Restored->IsPlaneLandingGearDown());
	// The tour drives presentation time explicitly while normal simulation is paused.
	World->SpawnActor<ASimCopterTrafficSystemActor>();
	auto* Ambient=World->SpawnActor<ASimCopterAmbientVehiclesActor>();
	auto* Mesh=Ambient->CreateVehicleMesh(SimCopterAmbientVehicles::PlaneObjectId,TEXT("TestTourPlane"),true);
	TestNotNull(TEXT("Tour aircraft mesh loads"),Mesh);
	if (Mesh)
	{
		Mesh->SetVisibility(true,true);
		for (USceneComponent* Child:Mesh->GetAttachChildren()) if(auto* Parts=Cast<USimCopterPlanePartsComponent>(Child))
		{
			Parts->SetPresentationTime(20,true,false); const float First=Parts->GetPropellerAngle();
			Parts->SetPresentationTime(20.1f,true,false);
			TestNotEqual(TEXT("Paused-tour propeller animates"),Parts->GetPropellerAngle(),First);
			Mesh->SetVisibility(false,true); Parts->SetPresentationTime(21,true,true);
			TestFalse(TEXT("Hidden ambient plane leaves no floating gear"),Parts->IsGearVisible());
		}
	}
	if (FParse::Param(FCommandLine::Get(),TEXT("SimPlaneControlsPreview")))
	{
		auto Widget=SNew(SSimCopterPlaneControls).Scale(4).GetPlane([Plane](){return Plane;});
		FWidgetRenderer Renderer(true);
		for (int32 State=0;State<2;++State)
		{
			Plane->bPlaneLandingGearDown=State==0;
			auto* Target=Renderer.DrawWidget(Widget,FVector2D(376,500));
			TArray<FColor> Pixels; FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,Flags);
			TestTrue(TEXT("Gear UI image saved"),FImageUtils::SaveImageByExtension(
				*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/aircraft-destruction")/FString::Printf(TEXT("gear-%d.png"),State)),FImageView(Pixels.GetData(),376,500)));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterWeaponDamageTest,"SimCopter.AircraftDestruction.ProjectilesAndPopulation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterWeaponDamageTest::RunTest(const FString&)
{
	auto* World=MakeAircraftTestWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->PeopleTileClasses.Init(7,128*128); Traffic->TileCenterWorldZ.Init(0,128*128);
	Traffic->WaterTileFlags.Init(0,128*128); Traffic->XbldTileIds.Init(0,128*128); Traffic->ActiveTileSize=400;
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
	auto* Aircraft=World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Pool=Aircraft->GetApachePool(); Pool->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot());
	TSharedPtr<FPeopleBehaviorModel> Model=MakeShared<FPeopleBehaviorModel>(); FString Error;
	TestTrue(TEXT("Original patient programs load"),FSimCopterPeopleReader::LoadFromFile(
		FSimCopterPeopleReader::ResolvePeoplePath(SimCopterOriginalGame::ResolveRoot()),*Model,Error));
	auto NewWalker=[&]()
	{
		auto* Person=World->SpawnActor<ASimCopterGroundAgent>(); Person->SetOwner(Traffic);
		Person->BehaviorModel=Model; Person->bBehaviorActive=true; Person->BehaviorContext.ResetToState(0);
		Person->BehaviorContext.Attributes[EBhavAttr::MedevacHealth]=100;
		return Person;
	};
	auto Shoot=[&](ASimCopterGroundAgent* Target,bool Missile)
	{
		const FVector At=Target->GetActorLocation();
		if(Missile) Pool->LaunchMissile(At-FVector(300,0,0),FVector::ForwardVector,0);
		else Pool->LaunchBullet(At-FVector(300,0,0),FVector::ForwardVector,0);
		for(int32 I=0;I<6;++I) Pool->TickComponent(.05f,LEVELTICK_All,nullptr);
	};
	auto NewCar=[&](float X)
	{
		auto* Car=World->SpawnActor<ASimCopterGroundAgent>();
		Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),100);
		Car->SetOwner(Traffic); Car->SetActorLocation(FVector(X,2000,Car->GetCapsuleHalfHeightCm())); return Car;
	};
	auto* Car=NewCar(2000); Shoot(Car,true);
	TestTrue(TEXT("Real missile explodes the struck car"),Car->HasVehicleExploded());
	TestTrue(TEXT("Destroyed car has no render or blocking collision"),Car->IsHidden()&&!Car->GetActorEnableCollision());
	TestFalse(TEXT("Destroyed car cannot explode twice"),Car->ApplyWeaponVehicleHit(true,Car->GetActorLocation()));
	auto* GunCar=NewCar(4000); Shoot(GunCar,false);
	TestTrue(TEXT("First gun hit burns and stops car"),GunCar->IsVehicleAccidentBurning()&&GunCar->IsVehicleImmobilized());
	Shoot(GunCar,false); TestTrue(TEXT("Repeated machine gun hit explodes car"),GunCar->HasVehicleExploded());
	auto* Patient=NewWalker();
	Patient->SetActorLocation(FVector(6000,2000,Patient->GetCapsuleHalfHeightCm())); Shoot(Patient,false);
	TestTrue(TEXT("Machine gun injures NPC"),Patient->IsMedevacVictim());
	TestTrue(TEXT("Injured NPC has a medical mission"),Patient->MissionEventId!=INDEX_NONE);
	auto* BlastPatient=NewWalker();
	BlastPatient->SetActorLocation(FVector(8000,2000,BlastPatient->GetCapsuleHalfHeightCm())); Shoot(BlastPatient,true);
	TestTrue(TEXT("Missile injures NPC"),BlastPatient->IsMedevacVictim());
	TArray<ASimCopterGroundAgent*> Walkers;
	for(int32 I=0;I<40;++I)
	{
		auto* Person=NewWalker();
		Person->SetActorLocation(FVector(1000+(I%10)*200,5000+(I/10)*200,Person->GetCapsuleHalfHeightCm())); Walkers.Add(Person);
	}
	const int32 Injured=Missions->ApplyNuclearPopulationDamage(1234);
	TestTrue(TEXT("Radioactivity injures many walkers"),Injured>=20);
	int32 Healthy=0; for(auto* Person:Walkers) Healthy+=!Person->IsMedevacVictim();
	TestTrue(TEXT("Radioactivity leaves healthy survivors"),Healthy>0&&Healthy<20);
	TestTrue(TEXT("Existing patients remain alive"),Patient->IsMedevacVictim()&&!Patient->IsMissionPatientDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCityWeaponTest,"SimCopter.AircraftDestruction.WholeCity",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterCityWeaponTest::RunTest(const FString&)
{
	auto* World=MakeAircraftTestWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false;
	City->bRenderProceduralMapExtension=false; City->bRenderStreetLightSpotLights=false;
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	City->CityFile.FilePath=SimCopterOriginalGame::ResolveRoot()/TEXT("cities/career/city1.sc2");
	City->FinishSpawning(FTransform::Identity); City->RebuildCity();
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>(); Traffic->SourceCityActor=City;
	Traffic->CityFile.FilePath=City->CityFile.FilePath; Traffic->RebuildSpawnData();
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(Missions,1); Missions->MissionSystem.BeginSession();
	const int32 OriginalBuildingCount=City->Buildings.Num();
	int32 Destroyed=0, Essential=0;
	// Exercise the actual projectile trace before exhausting every instance mapping.
	auto* Aircraft=World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Pool=Aircraft->GetApachePool(); Pool->SetOriginalGameRoot(SimCopterOriginalGame::ResolveRoot());
	for (const auto& Building:City->Buildings)
	{
		if (Building.bDemolished || Building.Parts.IsEmpty()) continue;
		FBox Bounds;
		if (!City->TryGetBuildingBoundsAtTile(Building.OriginTile.X,Building.OriginTile.Y,Bounds)) continue;
		const FVector Start(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Max.Z+300);
		FHitResult Roof;
		if (!World->LineTraceSingleByChannel(Roof,Start,Start-FVector(0,0,1000),ECC_Visibility)) continue;
		FIntPoint Origin;
		if (!City->TryGetBuildingHitOrigin(Roof,Origin)) continue;
		Pool->LaunchMissile(Start,-FVector::UpVector,0);
		for (int32 I=0;I<12;++I) Pool->TickComponent(.05f,LEVELTICK_All,nullptr);
		TestFalse(TEXT("Fired missile collapses a real city building"),City->HasStandingBuildingAtTile(Origin.X,Origin.Y));
		Destroyed+=!City->HasStandingBuildingAtTile(Origin.X,Origin.Y);
		break;
	}
	TestEqual(TEXT("Real city collision is reachable by missiles"),Destroyed,1);
	for(const auto& Building:City->Buildings)
	{
		if(Building.bDemolished||Building.Parts.IsEmpty()) continue;
		const FIntPoint Origin=Building.OriginTile;
		const auto Part=Building.Parts[0];
		FHitResult Hit(City,City->BuildingInstanceComponents[Part.ComponentIndex],FVector::ZeroVector,FVector::UpVector);
		Hit.Item=Part.InstanceIndex; Hit.bBlockingHit=true;
		FBox Bounds; if(City->TryGetBuildingBoundsAtTile(Origin.X,Origin.Y,Bounds)) Hit.ImpactPoint=Bounds.GetCenter();
		Essential+=(Building.XbldId==0xd1||Building.XbldId==0xd2||Building.XbldId==0xd3);
		if(!Missions->DestroyBuildingFromMissile(Hit)) { AddError(FString::Printf(TEXT("Missile failed at %s class %d"),*Origin.ToString(),Building.XbldId)); break; }
		++Destroyed;
		TestFalse(TEXT("Collapsed footprint has no standing mesh"),City->HasStandingBuildingAtTile(Origin.X,Origin.Y));
	}
	TestTrue(TEXT("Real city contains hundreds of destructible structures"),OriginalBuildingCount>200);
	TestEqual(TEXT("Every original building can be destroyed"),Destroyed,OriginalBuildingCount);
	TestTrue(TEXT("Essential buildings also destructible by missiles"),Essential>0);
	int32 Standing=0; for(const auto& Building:City->Buildings) Standing+=!Building.bDemolished;
	TestEqual(TEXT("No city building is indestructible"),Standing,0);
	TArray<FIntPoint> Saved; City->GetDemolishedBuildingOrigins(Saved);
	TestEqual(TEXT("Every collapsed footprint is persisted"),Saved.Num(),Destroyed);
	AddInfo(FString::Printf(TEXT("Destroyed %d buildings, including %d essential services."),Destroyed,Essential));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPilotConversationTest,"SimCopter.AircraftDestruction.PilotConversation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSimCopterPilotConversationTest::RunTest(const FString&)
{
	auto* World=MakeAircraftTestWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Person=World->SpawnActor<ASimCopterGroundAgent>();
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
	auto* Controller=World->SpawnActor<APlayerController>();
	Controller->Player=NewObject<ULocalPlayer>(GEngine); Controller->Possess(Pilot);
	TestTrue(TEXT("Collision fixture has a local pilot"),Pilot->IsLocallyControlled());
	TSharedPtr<FPeopleBehaviorModel> Model=MakeShared<FPeopleBehaviorModel>(); FString Error;
	TestTrue(TEXT("Original conversation programs load"),FSimCopterPeopleReader::LoadFromFile(
		FSimCopterPeopleReader::ResolvePeoplePath(SimCopterOriginalGame::ResolveRoot()),*Model,Error));
	Person->BehaviorModel=Model; Person->bBehaviorActive=true; Person->BehaviorContext.ResetToState(0);
	Pilot->GetCharacterMovement()->Velocity=FVector(200,0,0);
	FHitResult Hit(Person,Person->CollisionComponent,Person->GetActorLocation(),-FVector::ForwardVector);
	Pilot->NotifyHit(Pilot->GetCapsuleComponent(),Person,Person->CollisionComponent,true,
		Hit.ImpactPoint,Hit.ImpactNormal,FVector::ZeroVector,Hit);
	TestEqual(TEXT("Player body contact dispatches original neutral-person reaction"),Person->BehaviorContext.LastReactionProgramId,914);
	TestTrue(TEXT("Contact feedback is rate limited"),Pilot->NextContactSoundTime>World->GetTimeSeconds());
	Person->RunMeetSelector(Person->BehaviorContext,Pilot);
	TestTrue(TEXT("Player contact selects conversation, not object Whoa"),
		Person->BehaviorContext.PendingAnimMnemonic==TEXT("2Gab")||Person->BehaviorContext.PendingAnimMnemonic==TEXT("HipH"));
	Pilot->NextContactSoundTime=0;
	Pilot->NotifyHit(Pilot->GetCapsuleComponent(),Person,Person->CollisionComponent,true,
		Hit.ImpactPoint,FVector::UpVector,FVector::ZeroVector,Hit);
	TestEqual(TEXT("Walking on the ground does not trigger contact sounds"),Pilot->NextContactSoundTime,0.0);
	Controller->UnPossess();
	return true;
}
#endif
