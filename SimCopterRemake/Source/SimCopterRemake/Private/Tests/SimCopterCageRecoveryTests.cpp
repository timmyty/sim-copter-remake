#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Game/SimCopterPlayerController.h"
#include "Flight/SimCopterAirOperations.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "City/SimCity2000CityActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UI/SimCopterMapRaster.h"
#include "UI/SSimCopterDashboard.h"
#include "UI/SimCopterHangarArt.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ImageUtils.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "RenderingThread.h"
#include "Formats/SimCopterOriginalGamePaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCageRecoveryTest,"SimCopter.CaptureCage.VehicleBoatRecoveryAndWaypoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCageRecoveryTest::RunTest(const FString&)
{
	using namespace SimCopterMissions;
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	bool bDeferredCleanup=false;
	ON_SCOPE_EXIT { if (!bDeferredCleanup) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); } };
	// Install the preview sun before the first waypoint exposure query. A lightless
	// world deliberately caches its night fallback before retrying the light search.
	auto* Rig=World->SpawnActor<AActor>();
	if (FParse::Param(FCommandLine::Get(),TEXT("SimCageRecoveryPreview")))
	{
		auto* Sun=NewObject<UDirectionalLightComponent>(Rig); Sun->SetIntensity(60000);
		Sun->SetWorldRotation(FRotator(-45,135,0)); Sun->RegisterComponent();
		auto* Fill=NewObject<UDirectionalLightComponent>(Rig); Fill->SetIntensity(30000);
		Fill->SetWorldRotation(FRotator(-35,-135,0)); Fill->SetCastShadows(false); Fill->RegisterComponent();
		for (float Yaw : {0.0f,90.0f,180.0f,270.0f})
		{
			auto* Softbox=NewObject<UDirectionalLightComponent>(Rig); Softbox->SetIntensity(15000);
			Softbox->SetWorldRotation(FRotator(-25,Yaw,0)); Softbox->SetCastShadows(false); Softbox->RegisterComponent();
		}
	}
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false; City->bLoadOnBeginPlay=false; City->FinishSpawning(FTransform::Identity);
	City->WaterGameplayCornerZ.Init(0,129*129); City->WaterGameplayTerrainClasses.Init(10,128*128);
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>(); Traffic->SourceCityActor=City;
	Traffic->PeopleTileClasses.Init(7,128*128); Traffic->TileCenterWorldZ.Init(0,128*128);
	Traffic->WaterTileFlags.Init(0,128*128); Traffic->XbldTileIds.Init(0,128*128); Traffic->ActiveTileSize=400;
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* PC=World->SpawnActor<ASimCopterPlayerController>();
	PC->Player=NewObject<ULocalPlayer>(GEngine); World->AddController(PC); PC->Possess(Heli);
	Heli->SwitchHelicopterModel(4);
	Heli->EquipmentState.CareerEquipmentMask=0x23;
	TestTrue(TEXT("Legacy purchase grants replacement cage"),Heli->IsToolAvailable(ESimCopterHelicopterTool::CaptureCage));
	TestFalse(TEXT("Clamp cannot appear in cycling or controller wheel"),Heli->IsToolSelectable(ESimCopterHelicopterTool::TowClamp));
	Heli->SetSelectedTool(ESimCopterHelicopterTool::TowClamp);
	TestEqual(TEXT("Legacy selection becomes the cage"),Heli->GetActiveTool(),ESimCopterHelicopterTool::CaptureCage);
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,false);
	TestEqual(TEXT("Selling migrated cage preserves the other equipment"),Heli->EquipmentState.CareerEquipmentMask,3);
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::CaptureCage,true);
	auto* Ops=Heli->GetAirOperations();
	auto* System=World->GetSubsystem<USimCopterAirOperationsSubsystem>();
	auto* Yard=World->SpawnActor<ASimCopterRecoverySite>(); Yard->Configure(ESimCopterRecoverySite::Junkyard,FVector(5000,5000,0));
	auto* Harbor=World->SpawnActor<ASimCopterRecoverySite>(); Harbor->Configure(ESimCopterRecoverySite::Harbor,FVector(8000,8000,0));
	System->Sites={Yard,Harbor}; System->bSitesReady=true; System->RefreshRecoveryWaypoints();
	auto HasJunkyardMarker=[&]()
	{
		TArray<FSimCopterMissionWorldMarkerEntry> Markers; Missions->BuildMissionWorldMarkers(Markers);
		return Markers.ContainsByPredicate([&](const auto& Marker)
		{ return Marker.Label==TEXT("JUNKYARD") && Marker.WorldLocation.Equals(Yard->GetActorLocation()+FVector(0,0,120)); });
	};
	TestFalse(TEXT("Empty cage has no world junkyard tag"),HasJunkyardMarker());
	TestFalse(TEXT("No recovery targets means inactive waypoints"),Yard->IsWaypointActive() || Harbor->IsWaypointActive());
	auto NewCar=[&](FVector Position)
	{
		auto* Car=World->SpawnActor<ASimCopterGroundAgent>(); Car->SetOwner(Traffic);
		Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),600);
		Car->SetActorLocation(Position); Car->bSnapToGround=false;
		return Car;
	};
	auto PositionCage=[&](FVector Position)
	{
		Heli->SetActorLocation(FVector(0,0,1500));
		Heli->AddActorWorldOffset(Position+FVector(0,0,1000)-Heli->GetCaptureCageMountTransform().GetLocation());
		Ops->CargoLocation=Position; Ops->CargoVelocity=FVector::ZeroVector; Ops->CableLength=1000; Ops->SetCableInput(0);
	};
	auto OpenEmptyCage=[&](FVector Position)
	{
		Ops->ReleaseCargo(); Ops->bDeployed=false; Ops->UseCargoTool();
		Ops->CageFormation=1; Ops->CageSeal=0; Ops->CageCaptureDelay=0; PositionCage(Position);
	};
	auto* Car=NewCar(FVector(1000,1000,60)); Car->SetVehicleStalled(false);
	System->RefreshRecoveryWaypoints();
	TestTrue(TEXT("Stalled-car job activates only junkyard"),Yard->IsWaypointActive() && !Harbor->IsWaypointActive());
	TestFalse(TEXT("Stalled vehicle outside the cage does not show destination tag"),HasJunkyardMarker());
	OpenEmptyCage(Car->GetActorLocation()); Ops->Update(0);
	TestTrue(TEXT("Open cage automatically captures stalled car"),Ops->GetTowVehicle()==Car && Car->IsVehicleTowed());
	TestTrue(TEXT("Caged stalled vehicle shows junkyard world tag"),HasJunkyardMarker());
	Ops->ReleaseCargo();
	TestFalse(TEXT("Releasing stalled vehicle removes junkyard tag"),HasJunkyardMarker());
	OpenEmptyCage(Car->GetActorLocation()); Ops->Update(0);
	TestFalse(TEXT("Pickup closes bottom automatically"),Ops->bCageOpen);
	Ops->SetCableInput(1); Ops->Update(.1f);
	TestEqual(TEXT("Winch waits while bottom seals"),Ops->CableLength,1000.0f);
	TestTrue(TEXT("Floor sealing is animated"),Ops->CageSeal>0 && Ops->CageSeal<1);
	Ops->SetCableInput(0); Ops->Update(.3f);
	TestEqual(TEXT("Floor finishes closing"),Ops->CageSeal,1.0f);
	PositionCage(Yard->GetActorLocation()+FVector(0,0,70)); Ops->Update(0);
	TestTrue(TEXT("Flying through marker does not auto-deliver"),Car->IsVehicleTowed() && !Car->HasMissionResolutionReported());
	Ops->CageSeal=.5f; TestFalse(TEXT("Unsealed cargo cannot complete"),Ops->TryDeliverCargo()); Ops->CageSeal=1;
	Ops->CargoVelocity=FVector(500,0,0); TestFalse(TEXT("Fast delivery is rejected"),Ops->TryDeliverCargo()); Ops->CargoVelocity=FVector::ZeroVector;
	PositionCage(Harbor->GetActorLocation()+FVector(0,0,70)); TestFalse(TEXT("Harbor refuses car"),Ops->TryDeliverCargo());
	PositionCage(Yard->GetActorLocation()+FVector(181,0,70)); TestFalse(TEXT("Outside ring refuses delivery"),Ops->TryDeliverCargo());
	PositionCage(Yard->GetActorLocation()+FVector(0,0,500)); TestFalse(TEXT("High hover refuses delivery"),Ops->TryDeliverCargo());
	PositionCage(Yard->GetActorLocation()+FVector(0,0,70)); Ops->OpenCargo();
	TestTrue(TEXT("OPEN over junkyard completes car job"),Car->HasMissionResolutionReported() && !Ops->HasCargo() && Ops->bCageOpen);
	TestFalse(TEXT("Delivery removes junkyard world tag"),HasJunkyardMarker());
	TestFalse(TEXT("Delivery cannot pay twice"),Ops->TryDeliverCargo());
	Missions->MissionSystem.Tick(0); System->RefreshRecoveryWaypoints();
	TestFalse(TEXT("Completed car job removes waypoint"),Yard->IsWaypointActive());
	Car->Destroy();
	auto* Healthy=NewCar(FVector(1400,1400,70)); OpenEmptyCage(Healthy->GetActorLocation());
	TestFalse(TEXT("Cage ignores healthy non-fleeing traffic"),Ops->TryGrabCargo());
	Healthy->SetVehicleStalled(false);
	auto* WallActor=World->SpawnActor<AActor>(); auto* Wall=NewObject<UBoxComponent>(WallActor);
	WallActor->SetRootComponent(Wall); Wall->SetBoxExtent(FVector(8,150,150)); Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Wall->SetCollisionResponseToAllChannels(ECR_Block); Wall->RegisterComponent();
	WallActor->SetActorLocation(Healthy->GetActorLocation()+FVector(45,0,0)); PositionCage(Healthy->GetActorLocation()+FVector(90,0,0));
	TestFalse(TEXT("Capture cannot reach a car through a wall"),Ops->TryGrabCargo());
	WallActor->Destroy();
	if (Healthy->GetTowMissionId()!=INDEX_NONE) Missions->PostMissionEvent(EVT_SetCategory,Healthy->GetTowMissionId(),CAT_ExpireSilently,true);
	Healthy->Destroy(); Missions->MissionSystem.Tick(0);

	auto* Runner=NewCar(FVector(1800,1800,80)); Runner->MakeSpeeder(); Runner->SetMoveTarget(FVector(3000,1800,80)); Runner->Tick(.1f);
	TestTrue(TEXT("Fixture runner is moving"),!Runner->GetCurrentVelocityCmPerSec().IsNearlyZero());
	const FVector Route=Runner->GetMoveTargetLocation();
	OpenEmptyCage(Runner->GetActorLocation()); Ops->Update(0);
	TestTrue(TEXT("Moving runner is captured without taser"),Ops->GetTowVehicle()==Runner);
	TestFalse(TEXT("Moving getaway car does not show stalled-vehicle destination tag"),HasJunkyardMarker());
	Runner->Tick(20); TestTrue(TEXT("Cage prevents escape"),Runner->IsVehicleTowed() && Runner->GetCurrentVelocityCmPerSec().IsNearlyZero());
	Ops->OpenCargo(); TestFalse(TEXT("Away from ring OPEN releases without completion"),Runner->IsVehicleTowed() || Runner->HasMissionResolutionReported());
	TestTrue(TEXT("Released runner keeps original route"),Runner->GetMoveTargetLocation().Equals(Route));
	TestFalse(TEXT("Release cooldown prevents instant recapture"),Ops->TryGrabCargo());
	Runner->ClearSpeeder(); Runner->DisableVehicleWithTaser();
	TestEqual(TEXT("Vehicle stun doubles to sixteen seconds"),Runner->VehicleTaserSeconds,16.0f);
	OpenEmptyCage(Runner->GetActorLocation()); Ops->Update(0);
	TestTrue(TEXT("Taser-disabled car is capture eligible without damage"),Ops->GetTowVehicle()==Runner && !Runner->IsTowableVehicle());
	Runner->Tick(20); TestTrue(TEXT("Expired stun cannot release caged car"),Runner->IsVehicleTowed());
	TArray<uint8> Saved; Ops->bCage=false; // Emulate a legacy loaded clamp with its cargo.
	{ FMemoryWriter Writer(Saved); Ops->SerializeState(Writer); }
	{ FMemoryReader Reader(Saved); Ops->SerializeState(Reader); TestFalse(TEXT("Legacy cargo payload remains readable"),Reader.IsError()); }
	Ops->ResolveSavedCargo();
	TestTrue(TEXT("Loaded vehicle remains in sealed cage after stun expires"),Ops->IsCage() && Ops->GetTowVehicle()==Runner && !Ops->bCageOpen);
	PositionCage(Yard->GetActorLocation()+FVector(0,0,70)); Ops->OpenCargo();
	TestTrue(TEXT("Recovered formerly tasered car completes"),Runner->HasMissionResolutionReported()); Runner->Destroy();

	auto* Criminal=NewCar(FVector(2500,2500,60)); const int32 Crime=Missions->CreateMissionAt(60,60,TYPE_Burglar);
	Criminal->MakeCriminalCar(Crime,65536); Traffic->CriminalCars.Add(Criminal);
	OpenEmptyCage(Criminal->GetActorLocation()); Ops->Update(0); Ops->CageSeal=1;
	const uint8 CriminalState=Criminal->GetCriminalState(); Traffic->UpdateCriminalCars(100);
	TestEqual(TEXT("Caged criminal cannot advance its escape state machine"),Criminal->GetCriminalState(),CriminalState);
	Criminal->SetActorRotation(FRotator(0,35,0));
	PositionCage(Yard->GetActorLocation()+FVector(240,0,300));
	TestFalse(TEXT("A small edge overlap is not a junkyard capture"),Ops->TryDeliverCargo());
	PositionCage(Yard->GetActorLocation()+FVector(140,0,300));
	Ops->CargoVelocity=FVector(500,0,0);
	TestFalse(TEXT("Fast flyby above ring is not a capture"),Ops->TryDeliverCargo());
	Ops->CargoVelocity=FVector::ZeroVector;
	Ops->OpenCargo(); Missions->MissionSystem.Tick(0);
	TestTrue(TEXT("Dropping getaway car from above mostly inside the ring resolves its crime"),Criminal->HasMissionResolutionReported());
	TestFalse(TEXT("Junkyard capture ends the burglar mission successfully"),Missions->IsMissionEventActive(Crime));
	const int32 CaptureScore=Missions->MissionSystem.GetScore();
	Ops->OpenCargo(); Criminal->CompleteVehicleTow(true); Missions->MissionSystem.Tick(0);
	TestEqual(TEXT("Repeated OPEN cannot award capture twice"),Missions->MissionSystem.GetScore(),CaptureScore);
	Criminal->Destroy();

	auto* Ambient=World->SpawnActor<ASimCopterAmbientVehiclesActor>(); auto& Boat=Ambient->Boats[0];
	Boat.bVisible=true; Boat.ObjectId=SimCopterAmbientVehicles::CapsizedBoatObjectId; Boat.World=FVector(4000,4000,60);
	Boat.Mesh=Ambient->CreateVehicleMesh(Boat.ObjectId,TEXT("HarborRecoveryHull"));
	if (!TestNotNull(TEXT("Real capsized hull mesh loads"),Boat.Mesh.Get())) return false;
	Boat.Direction=FVector(1,1,0).GetSafeNormal();
	Boat.TowEventId=Missions->CreateMissionAt(70,70,TYPE_BoatTow);
	const int32 BoatRecoveryEvent=Boat.TowEventId;
	System->RefreshRecoveryWaypoints(); TestTrue(TEXT("Hull recovery activates harbor"),Harbor->IsWaypointActive());
	auto* Survivor=World->SpawnActor<ASimCopterGroundAgent>(); Survivor->MissionEventId=Boat.EventId;
	Survivor->SetActorLocation(Boat.World+FVector(100,0,100)); Ambient->BoatRiders.Add({Survivor});
	OpenEmptyCage(Boat.World); TestFalse(TEXT("Boat must have all riders rescued first"),Ops->TryGrabCargo());
	Survivor->Destroy();
	OpenEmptyCage(Boat.World); Ops->Update(0); Ops->Update(.4f);
	TestTrue(TEXT("Boat captured and bottom sealed automatically"),Ops->HasBoat() && Boat.bTowAttached && !Ops->bCageOpen && Ops->CageSeal==1);
	TestFalse(TEXT("Boat cargo does not show junkyard world tag"),HasJunkyardMarker());
	Saved.Reset(); { FMemoryWriter Writer(Saved); Ops->SerializeState(Writer); }
	{ FMemoryReader Reader(Saved); Ops->SerializeState(Reader); } Ops->ResolveSavedCargo();
	TestTrue(TEXT("Boat cage survives save restore"),Ops->HasBoat() && !Ops->bCageOpen);
	PositionCage(Yard->GetActorLocation()+FVector(0,0,70)); TestFalse(TEXT("Junkyard refuses boat"),Ops->TryDeliverCargo());
	FBox Hull; Ambient->GetBoatTowBounds(0,Hull);
	const FVector HullCenterOffset=Hull.GetCenter()-Boat.World;
	const double HullBottomOffset=Hull.Min.Z-Boat.World.Z;
	AddInfo(FString::Printf(TEXT("Capsized hull world extent %s, center offset %s, bottom offset %.1f"),
		*Hull.GetExtent().ToString(),*HullCenterOffset.ToString(),HullBottomOffset));
	auto PositionHull=[&](FVector FootprintCenter)
	{
		// Place using the visible hull, while leaving the mesh at the preceding physics tick.
		PositionCage(FootprintCenter-FVector(HullCenterOffset.X,HullCenterOffset.Y,HullBottomOffset));
	};
	PositionHull(Harbor->GetActorLocation()+FVector(320,0,250));
	TestFalse(TEXT("A hull outside the lit ring is rejected"),Ops->TryDeliverCargo());
	PositionHull(Harbor->GetActorLocation()+FVector(0,0,450));
	TestFalse(TEXT("A hull too high above the harbor is rejected"),Ops->TryDeliverCargo());
	PositionHull(Harbor->GetActorLocation()+FVector(0,0,-150));
	TestFalse(TEXT("A hull below the harbor deck is rejected"),Ops->TryDeliverCargo());
	PositionHull(Harbor->GetActorLocation()+FVector(100,0,250));
	Ops->CargoVelocity=FVector(250,0,0);
	TestFalse(TEXT("A fast hull flyby is rejected"),Ops->TryDeliverCargo());
	Ops->CargoVelocity=FVector::ZeroVector;
	const int32 ScoreBeforeBoat=Missions->MissionSystem.GetScore();
	Ops->OpenCargo();
	TestTrue(TEXT("OPEN delivers boat to harbor cradle"),!Ops->HasCargo() && !Boat.bTowAttached && Boat.RepairDisplaySeconds>0);
	Missions->MissionSystem.Tick(0); System->RefreshRecoveryWaypoints();
	TestFalse(TEXT("A short drop of the visible hull completes its recovery mission"),Missions->IsMissionEventActive(BoatRecoveryEvent));
	TestEqual(TEXT("Hull recovery awards its completion once"),Missions->MissionSystem.GetScore(),ScoreBeforeBoat+100);
	const int32 ScoreAfterBoat=Missions->MissionSystem.GetScore();
	Ops->OpenCargo(); Ambient->FinishBoatTow(0); Missions->MissionSystem.Tick(0);
	TestEqual(TEXT("Repeated release cannot award hull recovery twice"),Missions->MissionSystem.GetScore(),ScoreAfterBoat);
	TestFalse(TEXT("Finished hull removes harbor waypoint"),Harbor->IsWaypointActive());

	const FVector PoliceRoof(6000,6000,1200);
	System->ServiceRoofs.Add(0xD2,{PoliceRoof});
	// A real three-tile police footprint, including roof area outside the old 3 m circle.
	FSimCopterGroundRouteNode RoofNode; RoofNode.Location=PoliceRoof; RoofNode.PeopleFootprintSize=3; RoofNode.BuildingId=0xD2;
	const int32 RoofNodeIndex=Traffic->PedestrianNodes.Add(RoofNode);
	for (int32 DX=-1;DX<=1;++DX) for (int32 DY=-1;DY<=1;++DY)
	{
		int32 X,Y; Traffic->TryGetPeopleTileCoordinateAtWorldLocation(PoliceRoof+FVector(DX*400,DY*400,0),X,Y);
		Traffic->XbldTileIds[Y*128+X]=0xD2;
		Traffic->PedestrianNodeIndexByTile.Add(FIntPoint(X,Y),RoofNodeIndex);
		Traffic->BuildingRoofPostByTile.Add(FIntPoint(X,Y),PoliceRoof);
	}
	for (bool bBurglar : {false,true})
	{
		auto* RoofCar=NewCar(FVector(5000,5000,60));
		int32 Event=INDEX_NONE;
		if(bBurglar)
		{
			Event=Missions->CreateMissionAt(62,62,TYPE_Burglar);
			RoofCar->MakeCriminalCar(Event,65536); Traffic->CriminalCars.Add(RoofCar);
			RoofCar->DisableVehicleWithTaser();
		}
		else { RoofCar->SetVehicleStalled(false); Event=RoofCar->GetTowMissionId(); }
		OpenEmptyCage(RoofCar->GetActorLocation()); Ops->Update(0); Ops->CageSeal=1;
		TestTrue(TEXT("Roof-delivery vehicle is held by cage"),Ops->GetTowVehicle()==RoofCar);
		PositionCage(PoliceRoof+FVector(0,0,650));
		TestFalse(TEXT("Police roof rejects cargo too high"),Ops->TryDeliverCargo());
		PositionCage(PoliceRoof+FVector(0,0,-300));
		TestFalse(TEXT("Police roof rejects cargo below the deck"),Ops->TryDeliverCargo());
		PositionCage(PoliceRoof+FVector(800,0,300));
		TestFalse(TEXT("Police roof rejects cargo beyond the footprint"),Ops->TryDeliverCargo());
		PositionCage(PoliceRoof+FVector(0,0,70)); Ops->CargoVelocity=FVector(250,0,0);
		TestFalse(TEXT("Police roof rejects fast-moving cargo"),Ops->TryDeliverCargo());
		PositionCage(PoliceRoof+FVector(420,0,300));
		const int32 BeforeScore=Missions->MissionSystem.GetScore();
		Ops->CargoVelocity=FVector::ZeroVector; Ops->OpenCargo();
		TestTrue(TEXT("OPEN delivers stalled and burglar cars on police roof"),!Ops->HasCargo() && RoofCar->HasMissionResolutionReported());
		TestTrue(TEXT("Delivered car remains on roof"),RoofCar->GetActorLocation().Equals(PoliceRoof+FVector(0,0,65)));
		Missions->MissionSystem.Tick(0);
		TestFalse(TEXT("Police roof delivery completes corresponding mission"),Missions->IsMissionEventActive(Event));
		TestTrue(TEXT("Successful police roof delivery awards completion points"),Missions->MissionSystem.GetScore()>BeforeScore);
		const int32 Score=Missions->MissionSystem.GetScore(); Ops->OpenCargo(); Missions->MissionSystem.Tick(0);
		TestEqual(TEXT("Repeated OPEN cannot reward twice"),Missions->MissionSystem.GetScore(),Score);
		RoofCar->Destroy();
	}

	Boat.bVisible=true; Boat.RepairDisplaySeconds=0; Boat.World=FVector(4000,4000,60);
	Boat.TowEventId=Missions->CreateMissionAt(70,70,TYPE_BoatTow);
	const int32 RoofBoatEvent=Boat.TowEventId;
	Ambient->SetBoatMeshTransform(Boat,Boat.World,FRotator::ZeroRotator);
	OpenEmptyCage(Boat.World); Ops->Update(0); Ops->Update(.4f);
	TestTrue(TEXT("Second recovered hull is in the cage"),Ops->HasBoat());
	PositionHull(PoliceRoof+FVector(850,0,250));
	TestFalse(TEXT("Boat outside police roof is rejected"),Ops->TryDeliverCargo());
	PositionHull(PoliceRoof+FVector(420,0,250));
	const int32 BeforeRoofBoat=Missions->MissionSystem.GetScore();
	Ops->OpenCargo(); Missions->MissionSystem.Tick(0);
	TestTrue(TEXT("Police roof accepts a short drop of a boat"),!Boat.bTowAttached && Boat.RepairDisplaySeconds>0 && !Ops->HasCargo());
	TestFalse(TEXT("Police boat delivery completes its recovery mission"),Missions->IsMissionEventActive(RoofBoatEvent));
	TestEqual(TEXT("Police boat recovery rewards once"),Missions->MissionSystem.GetScore(),BeforeRoofBoat+100);
	FBox DeliveredHull; Ambient->GetBoatTowBounds(0,DeliveredHull);
	TestTrue(TEXT("Delivered upright hull rests on the police roof"),FMath::Abs(DeliveredHull.Min.Z-PoliceRoof.Z)<5);
	Ops->OpenCargo(); Ambient->FinishBoatTow(0); Missions->MissionSystem.Tick(0);
	TestEqual(TEXT("Repeated police boat delivery cannot pay twice"),Missions->MissionSystem.GetScore(),BeforeRoofBoat+100);

	// Exercise the actual SOS hull and survivors. Optional towing cannot freeze rescue
	// timers or reserve the original's sole capsized-boat slot indefinitely.
	Ambient->HideBoat(Boat); Boat.RepairDisplaySeconds=0; Boat.TowEventId=INDEX_NONE; Boat.EventId=INDEX_NONE;
	City->WaterGameplayTerrainClasses.Init(0,128*128);
	Traffic->PeopleTileClasses.Init(0,128*128); Traffic->WaterTileFlags.Init(1,128*128);
	Missions->MissionSystem.Initialize(Missions,1); Missions->MissionSystem.BeginSession();
	const int32 Sos=Missions->CreateMissionAt(64,64,TYPE_BoatRescue);
	TestTrue(TEXT("Original SOS creates a real boat and survivors"),Sos!=INDEX_NONE && Ambient->BoatRiders.Num()>=3);
	const float RescueTimer=Boat.MissionTimerSeconds;
	Ambient->UpdateBoat(Boat,1);
	TestEqual(TEXT("Occupied SOS hull has no competing recovery job"),Boat.TowEventId,INDEX_NONE);
	TestTrue(TEXT("SOS timer still advances"),Boat.MissionTimerSeconds<RescueTimer);
	Boat.TowEventId=Missions->CreateMissionAt(64,64,TYPE_BoatTow); // Legacy premature recovery.
	const int32 PrematureRecovery=Boat.TowEventId;
	Ambient->UpdateBoat(Boat,0);
	Missions->MissionSystem.Tick(0);
	TestFalse(TEXT("Premature recovery from an old save retires while survivors wait"),Missions->IsMissionEventActive(PrematureRecovery));
	TestEqual(TEXT("A second SOS never replaces people still awaiting rescue"),Missions->CreateMissionAt(65,65,TYPE_BoatRescue),INDEX_NONE);
	const int32 Rescued=Ambient->BoatRiders.Num();
	for (const auto& Rider:Ambient->BoatRiders) if (auto* P=Rider.Person.Get()) P->Destroy();
	Ambient->UpdateBoatRiders(Boat);
	Missions->PostMissionEvent(EVT_VictimPickedUp,Sos,Rescued,true);
	Missions->PostMissionEvent(EVT_RescueDelivered,Sos,Rescued,true);
	Missions->MissionSystem.Tick(0);
	Ambient->UpdateBoat(Boat,0);
	const int32 HullRecovery=Boat.TowEventId;
	TestTrue(TEXT("Empty hull can still be recovered"),HullRecovery!=INDEX_NONE && Boat.bVisible);
	TestTrue(TEXT("Recovered survivors leave the hull towable"),Ambient->SetBoatTow(0,true,Boat.World));
	TestEqual(TEXT("A new SOS does not steal attached cargo"),Missions->CreateMissionAt(65,65,TYPE_BoatRescue),INDEX_NONE);
	Ambient->SetBoatTow(0,false,Boat.World);
	Traffic->WaterTileFlags.Init(0,128*128);
	TestEqual(TEXT("Inland placement fails without consuming the existing hull"),Missions->CreateMissionAt(65,65,TYPE_BoatRescue),INDEX_NONE);
	TestTrue(TEXT("Failed SOS placement preserves recovery"),Boat.bVisible && Boat.TowEventId==HullRecovery);
	Traffic->WaterTileFlags.Init(1,128*128);
	const int32 BeforeSuperseding=Missions->MissionSystem.GetScore();
	TestTrue(TEXT("A new SOS takes precedence over unclaimed hull recovery"),Missions->CreateMissionAt(65,65,TYPE_BoatRescue)!=INDEX_NONE);
	Missions->MissionSystem.Tick(0);
	TestFalse(TEXT("Superseded hull recovery is retired"),Missions->IsMissionEventActive(HullRecovery));
	TestEqual(TEXT("Superseding optional recovery awards no points or penalty"),Missions->MissionSystem.GetScore(),BeforeSuperseding);
	TestTrue(TEXT("Replacement SOS has actual survivors"),Ambient->BoatRiders.Num()>=3);

	using namespace SimCopterMap;
	FSimCopterMapCity MapCity; MapCity.Xbld.Init(0,128*128); MapCity.TerrainClass.Init(0x30,128*128);
	MapCity.AltitudeShade.Init(4,128*128); MapCity.OnFire.Init(0,128*128);
	FSimCopterMapFrame Frame; Frame.City=&MapCity; Frame.CentreTile=FIntPoint(64,64);
	FSimCopterMapSettings Settings; Settings.Zoom=0;
	FSimCopterMapRaster Raster;
	Frame.RecoveryWaypoints.Add({FIntPoint(64,44),false,true}); Raster.Render(Frame,Settings);
	TestEqual(TEXT("Junkyard ring appears at correct map coordinate"),Raster.GetPixel(CentreX+25,CentreY),Color::Heading);
	TestEqual(TEXT("Cargo route leads to active ring"),Raster.GetPixel(CentreX+8,CentreY),Color::Heading);
	Frame.RecoveryWaypoints[0]={FIntPoint(64,-1000),true,true}; Raster.Render(Frame,Settings);
	TestEqual(TEXT("Distant harbor pins on its bearing at map edge"),Raster.GetPixel(CentreX+49,CentreY),Color::PoliceStation);
	Frame.RecoveryWaypoints[0].bCargoDestination=false; Raster.Render(Frame,Settings);
	TestNotEqual(TEXT("Inactive marker shrinks on the map"),Raster.GetPixel(CentreX+49,CentreY),Color::PoliceStation);
	TestEqual(TEXT("Inactive site remains visible as a small marker"),Raster.GetPixel(CentreX+47,CentreY),Color::PoliceStation);
	Frame.RecoveryWaypoints[0].bActive=true; Raster.Render(Frame,Settings);
	TestEqual(TEXT("Active mission enlarges marker before cargo pickup"),Raster.GetPixel(CentreX+49,CentreY),Color::PoliceStation);

	if (FParse::Param(FCommandLine::Get(),TEXT("SimCageRecoveryPreview")))
	{
		auto* Prisoner=World->SpawnActor<ASimCopterGroundAgent>(); Prisoner->InitialPersonState=12;
		Prisoner->StunForArrest(); TestTrue(TEXT("Preview prisoner boards"),Prisoner->BoardCarrier(Heli,false,true));
		TStrongObjectPtr<USimCopterHangarArt> Art(NewObject<USimCopterHangarArt>());
		Art->SetOriginalGameRoot(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame")));
		for (float Scale:{1.0f,2.0f,3.0f})
		{
			auto Dashboard=SNew(SSimCopterDashboard).Pawn(Heli).Art(Art.Get()).Scale(Scale);
			FWidgetRenderer Renderer(true); auto* Target=Renderer.DrawWidget(Dashboard,FVector2D(660*Scale,145*Scale));
			TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
			const FString Path=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/cage-towing")/FString::Printf(TEXT("custody-%.0f.png"),Scale);
			TestTrue(TEXT("Actual prisoner dashboard preview saved"),FImageUtils::SaveImageByExtension(*Path,FImageView(Pixels.GetData(),Target->SizeX,Target->SizeY)));
		}
		auto* PreviewCar=NewCar(FVector(0,0,150));
		PreviewCar->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT("AUTO"),SimCopterOriginalGame::ResolveRoot(),600);
		PreviewCar->SetVehicleStalled(false);
		OpenEmptyCage(PreviewCar->GetActorLocation()); Ops->Update(0); Ops->Update(.4f);
		Ops->CableLength=140;
		Heli->AddActorWorldOffset(Ops->CargoLocation+FVector(0,0,140)-Heli->GetCaptureCageMountTransform().GetLocation());
		// Advance the normal cargo update after positioning the aircraft. Otherwise this
		// isolated preview leaves the car at its previous pose while drawing the new cage.
		Ops->Update(0);
		AddInfo(FString::Printf(TEXT("Preview mount %s cargo %s car %s bounds %s"),
			*Heli->GetCaptureCageMountTransform().GetLocation().ToString(),*Ops->CargoLocation.ToString(),
			*PreviewCar->GetActorLocation().ToString(),*Ops->GetCageCargoBounds().ToString()));
		FBox CarBounds; TestTrue(TEXT("Preview loads actual vehicle geometry"),PreviewCar->TryGetBodyLocalBoundsCm(CarBounds));
		TestTrue(TEXT("Cage floor is below the captured car body"),Ops->GetCageCargoBounds().Min.Z<CarBounds.Min.Z-25);
		World->UpdateParameterCollectionInstances(true,true);
		bDeferredCleanup=true;
		ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand([this,World,Heli,Ops,PreviewCar,Yard,Harbor,Rig]()
		{
			ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
			auto* Capture=NewObject<USceneCaptureComponent2D>(Rig);
			Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false; Capture->bAlwaysPersistRenderingState=true;
			Capture->CaptureSource=SCS_FinalColorLDR;
			Capture->PostProcessSettings.bOverride_AutoExposureMethod=true; Capture->PostProcessSettings.AutoExposureMethod=AEM_Manual;
			Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=true;
			Capture->PostProcessSettings.bOverride_CameraISO=true; Capture->PostProcessSettings.CameraISO=100;
			Capture->PostProcessSettings.bOverride_CameraShutterSpeed=true; Capture->PostProcessSettings.CameraShutterSpeed=125;
			Capture->PostProcessSettings.bOverride_DepthOfFieldFstop=true; Capture->PostProcessSettings.DepthOfFieldFstop=8;
			Capture->PostProcessSettings.AmbientCubemap=LoadObject<UTextureCube>(nullptr,TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
			Capture->PostProcessSettings.bOverride_AmbientCubemapIntensity=true; Capture->PostProcessSettings.AmbientCubemapIntensity=1;
			Capture->ShowFlags.SetTemporalAA(false); Capture->ShowFlags.SetMotionBlur(false);
			Capture->ShowFlags.SetDynamicShadows(false);
			Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
			auto* Target=NewObject<UTextureRenderTarget2D>(Rig); Target->RenderTargetFormat=RTF_RGBA8;
			Target->InitAutoFormat(1600,1000); Target->UpdateResourceImmediate(true); Capture->TextureTarget=Target; Capture->RegisterComponent();
			// Neutral staging floor for documentation captures; all aircraft, cage and car
			// geometry and the sealed cargo state come from the running gameplay components.
			auto* Floor=NewObject<UStaticMeshComponent>(Rig);
			Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
			Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Floor->SetWorldLocation(Ops->CargoLocation-FVector(0,0,160)); Floor->SetWorldScale3D(FVector(80,80,1)); Floor->RegisterComponent();
			for (int32 Stage=0;Stage<3;++Stage)
			{
				Capture->ClearShowOnlyComponents();
				FVector Focus,Eye;
				if (Stage==0)
				{
					Capture->CaptureSource=SCS_FinalColorLDR; Ops->DrawCargo();
					Capture->ShowOnlyActorComponents(Heli); Capture->ShowOnlyActorComponents(PreviewCar);
					Capture->ShowOnlyComponent(Floor);
					Capture->PostProcessSettings.DepthOfFieldFstop=5.6f;
					Focus=Ops->GetCargoLocation()+FVector(0,0,65); Eye=Focus+FVector(350,580,160);
				}
				else
				{
					Capture->PostProcessSettings.DepthOfFieldFstop=8;
					Capture->CaptureSource=SCS_FinalColorLDR;
					auto* Site=Stage==1 ? Yard : Harbor; Site->SetWaypointActive(true); Capture->ShowOnlyActorComponents(Site);
					Focus=Site->GetActorLocation(); Eye=Focus+FVector(500,550,850);
				}
				Capture->SetWorldLocationAndRotation(Eye,(Focus-Eye).Rotation()); Capture->FOVAngle=48;
				World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
				if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
				FAssetCompilingManager::Get().FinishAllCompilation(); FlushRenderingCommands();
				Capture->CaptureScene(); FlushRenderingCommands();
				TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
				int32 Colored=0; for (auto& Pixel:Pixels) { Pixel.A=255; if(FMath::Max3(Pixel.R,Pixel.G,Pixel.B)-FMath::Min3(Pixel.R,Pixel.G,Pixel.B)>30) ++Colored; }
				TestTrue(TEXT("Actual cage/site has visible colored geometry"),Colored>200);
				const FString Path=FPaths::ProjectDir()/TEXT("../Docs/scratchpad/cage-towing")/FString::Printf(TEXT("world-%d.png"),Stage);
				TestTrue(TEXT("World recovery preview saved"),FImageUtils::SaveImageByExtension(*Path,FImageView(Pixels.GetData(),Target->SizeX,Target->SizeY)));
				if (Stage==0)
				{
					PreviewCar->SetActorHiddenInGame(true); World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
					TArray<FColor> Empty; Target->GameThread_GetRenderTargetResource()->ReadPixels(Empty);
					int32 CargoPixels=0;
					for (int32 K=0;K<Pixels.Num();++K)
						if (FMath::Abs(int32(Pixels[K].R)-Empty[K].R)+FMath::Abs(int32(Pixels[K].G)-Empty[K].G)+FMath::Abs(int32(Pixels[K].B)-Empty[K].B)>80) ++CargoPixels;
					TestTrue(TEXT("Captured car is visibly enclosed by the cage"),CargoPixels>100);
					AddInfo(FString::Printf(TEXT("Captured car: %d visible cargo pixels"),CargoPixels));
				}
				else
				{
					auto* Site=Stage==1 ? Yard : Harbor;
					Site->SetWaypointActive(false); World->SendAllEndOfFrameUpdates(); Capture->CaptureScene(); FlushRenderingCommands();
					TArray<FColor> Inactive; Target->GameThread_GetRenderTargetResource()->ReadPixels(Inactive);
					int32 RingPixels=0;
					for (int32 K=0;K<Pixels.Num();++K)
						if (FMath::Abs(int32(Pixels[K].R)-Inactive[K].R)+FMath::Abs(int32(Pixels[K].G)-Inactive[K].G)+FMath::Abs(int32(Pixels[K].B)-Inactive[K].B)>80) ++RingPixels;
					TestTrue(TEXT("Active light ring is visibly different from inactive site"),RingPixels>1000);
					AddInfo(FString::Printf(TEXT("Recovery site %d: %d visible ring pixels"),Stage,RingPixels));
				}
			}
		},1.0f));
	}
	return true;
}
#endif
