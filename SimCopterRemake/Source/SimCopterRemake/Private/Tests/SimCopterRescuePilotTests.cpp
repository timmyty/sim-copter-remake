#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Formats/MaxisMeshLibrary.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterPilotEquipment.h"
#include "Ground/SimCopterRescueDeck.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "City/SimCity2000CityActor.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRescueIdentityTest, "SimCopter.RescuePilot.PassengerIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRescueIdentityTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	TestTrue(TEXT("Seven-seat Agusta loads"), Heli->SwitchHelicopterModel(5));
	TArray<ASimCopterGroundAgent*> People;
	TestEqual(TEXT("Five survivors spawn"), Traffic->SpawnMissionSwimmersAtWorldLocation(5, FVector(2000,0,100), 777, 1, 50, false, &People), 5);
	TSet<int32> Heads;
	for (auto* Person : People)
	{
		const int32 Head = Person->GetHeadImageIndex();
		Heads.Add(Head);
		for (int32 Tick = 0; Tick < 60; ++Tick) Person->Tick(0.05f);
		TestEqual(TEXT("Rescue program preserves head identity"), Person->GetHeadImageIndex(), Head);
		TestTrue(TEXT("Survivor boards"), Person->BoardCarrier(Heli, false));
		const auto& Slot = Heli->GetMissionPassengerSlots().Last();
		TestEqual(TEXT("Cabin keeps the survivor's head"), Slot.HeadImageIndex, Head);
		AddInfo(FString::Printf(TEXT("%s head %d, seat %d"), *Person->GetName(), Head, Slot.HeadImageIndex));
	}
	TestTrue(TEXT("Rescue portraits are varied"), Heads.Num() >= 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterRescuePilotRuntimeTest, "SimCopter.RescuePilot.RuntimeRegression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterRescuePilotRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterMissions;
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction = City->bLoadOnBeginPlay = false; City->FinishSpawning(FTransform::Identity);
	City->WaterGameplayCornerZ.Init(0,129*129); City->WaterGameplayTerrainClasses.Init(0,128*128);
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->SourceCityActor = City;
	Traffic->PeopleTileClasses.Init(0,128*128); Traffic->TileCenterWorldZ.Init(0,128*128);
	Traffic->WaterTileFlags.Init(1,128*128); Traffic->XbldTileIds.Init(0,128*128); Traffic->ActiveTileSize = 400;
	auto* Ambient = World->SpawnActor<ASimCopterAmbientVehiclesActor>();
	int32 X=-1,Y=-1;
	TestTrue(TEXT("Real boat rescue activates"),Ambient->TryActivateBoatRescue(771,300,64,64,X,Y));
	TestTrue(TEXT("Boat carries at least three survivors"),Ambient->BoatRiders.Num()>=3);
	auto& Boat = Ambient->Boats[0];
	TArray<ASimCopterGroundAgent*> Survivors;
	TSet<int32> Portraits;
	for (const auto& Rider : Ambient->BoatRiders)
	{
		auto* Person=Rider.Person.Get(); if (!Person) continue;
		Survivors.Add(Person); Portraits.Add(Person->GetHeadImageIndex());
		FVector Surface;
		TestTrue(TEXT("Spawn is over actual hull triangles"),SimCopterRescueDeck::FindSurface(Boat.Mesh,Person->GetActorLocation(),Surface));
		TestTrue(TEXT("Feet rest on hull"),(Person->GetActorLocation()-FVector(0,0,Person->GetCapsuleHalfHeightCm())).Equals(Surface,0.1));
		TestFalse(TEXT("Hull occupant never uses swimming visuals"),Person->IsStandingInWater());
	}
	TestEqual(TEXT("Every boat passenger has a different portrait"),Portraits.Num(),Survivors.Num());
	for (int32 Frame=0; Frame<30; ++Frame)
	{
		Boat.World += FVector(3,2,0.5); Boat.Direction=FRotator(0,Frame*3,0).Vector();
		Ambient->SetBoatMeshTransform(Boat,Boat.World,FRotator(6,0,8));
		Ambient->UpdateBoatRiders(Boat);
		for (auto* Person : Survivors)
		{
			Person->Tick(0.05f);
			FVector Surface;
			TestTrue(TEXT("Moving and tilting hull retains its occupants"),SimCopterRescueDeck::FindSurface(Boat.Mesh,Person->GetActorLocation(),Surface));
			TestTrue(TEXT("Feet follow the tilted deck"),FMath::Abs(Person->GetActorLocation().Z-Person->GetCapsuleHalfHeightCm()-Surface.Z)<2);
		}
	}
	if (!Survivors.IsEmpty())
	{
		auto* Person=Survivors[0];
		Person->SetActorLocation(Person->GetActorLocation()+FVector(500,500,0));
		Person->FollowRescueDeck(true);
		FVector Surface;
		TestTrue(TEXT("A push cannot displace a survivor into water"),SimCopterRescueDeck::FindSurface(Boat.Mesh,Person->GetActorLocation(),Surface));
	}
	TestFalse(TEXT("Cannot tow an occupied rescue hull"),Ambient->SetBoatTow(0,true,Boat.World+FVector(0,0,200)));
	TArray<uint8> AmbientSave;
	TestTrue(TEXT("Boat state saves"),Ambient->CaptureRuntimeSaveState(AmbientSave));
	Ambient->ClearBoatRiders();
	TestTrue(TEXT("Boat state restores"),Ambient->RestoreRuntimeSaveState(AmbientSave));
	TestEqual(TEXT("Saved passengers reconnect to their hull"),Ambient->BoatRiders.Num(),Survivors.Num());

	auto* PC=World->SpawnActor<ASimCopterPlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine);
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	TestTrue(TEXT("Agusta loads"),Heli->SwitchHelicopterModel(5));
	PC->Possess(Heli); Heli->bIsLanded=true; Heli->GroundClearanceCm=0;
	for (auto* Person : Survivors) TestTrue(TEXT("Deck survivor boards"),Person->BoardCarrier(Heli,false));
	Ambient->UpdateBoatRiders(Boat);
	TestEqual(TEXT("Boarding releases deck ownership"),Ambient->BoatRiders.Num(),0);
	TestTrue(TEXT("Empty hull becomes towable"),Ambient->SetBoatTow(0,true,Boat.World+FVector(0,0,200)));
	for (auto* Person : Survivors) TestFalse(TEXT("Cabin occupant is no longer constrained to hull"),Person->RescueDeck.IsValid());
	TArray<uint8> HeliSave;
	TestTrue(TEXT("Cabin saves"),Heli->CaptureRuntimeSaveState(HeliSave));
	TestTrue(TEXT("Cabin restores"),Heli->RestoreRuntimeSaveState(HeliSave));
	for (auto* Person : Survivors) Heli->RelinkSavedMissionPassenger(Person);
	for (const auto& Slot : Heli->MissionPassengerSlots)
		TestTrue(TEXT("Restored cabin uses real occupant portrait"),Slot.Person.IsValid() && Slot.HeadImageIndex==Slot.Person->GetHeadImageIndex());

	// The slot list shrinks after each departure. Every passenger still needs a free position.
	TArray<FVector> Exits;
	for (auto* Person : Survivors)
	{
		TestTrue(TEXT("Passenger finds room to exit"),Person->AlightFromCarrier());
		for (const auto& Previous : Exits) TestTrue(TEXT("Exits never share a body-sized space"),FVector::Dist2D(Previous,Person->GetActorLocation())>=25);
		Exits.Add(Person->GetActorLocation());
	}
	if (!Survivors.IsEmpty())
	{
		auto* Person=Survivors[0]; TestTrue(TEXT("Queue fixture boards"),Person->BoardCarrier(Heli,false));
		auto* Blocker=World->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Blocker);
		Blocker->SetRootComponent(Box); Box->SetBoxExtent(FVector(2000)); Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
		Blocker->SetActorLocation(Heli->GetActorLocation());
		TestFalse(TEXT("Blocked exits leave passenger aboard"),Person->TryAlightHere());
		TestTrue(TEXT("Waiting passenger retains carrier and seat"),Person->GetBehaviorCarrier()==Heli);
		TestFalse(TEXT("Manual ground drop also waits for room"),Heli->DropPassengerAtSlot(0));
		auto* Delivery=World->SpawnActor<ASimCopterMissionSystemActor>();
		Traffic->WaterTileFlags.Init(0,128*128);
		const FVector DeliveryFeet=Heli->GetPassengerDropWorldLocation();
		TestTrue(TEXT("Blocked delivery fixture has valid dry destination"),Delivery->IsPassengerDeliveryLocationAllowed(Person->GetMissionPassengerKind(),DeliveryFeet,Person->MissionEventId));
		TestEqual(TEXT("Blocked mission delivery does not consume its queued passenger"),Delivery->ReleaseMissionPassengersFromHelicopter(Heli,Person->MissionEventId,Person->GetMissionPassengerKind(),1,DeliveryFeet,false),0);
		TestTrue(TEXT("Mission recovery retains queued passenger ownership"),Person->GetBehaviorCarrier()==Heli && Person->HasClaimedPassengerSeat());
		Delivery->Destroy();
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision); Blocker->Destroy();
		TestTrue(TEXT("Passenger retries successfully after obstruction clears"),Person->AlightFromCarrier());
	}

	Heli->SetActorLocation(FVector(5000,0,3000)); Heli->CurrentPitchDeg=20; Heli->CurrentRollDeg=-25;
	Heli->UpdateVisuals(0); Heli->SpotlightTarget.bValid=true; Heli->SpotlightTarget.WorldLocation=FVector(5400,200,0);
	Heli->SetCameraMode(ESimCopterCameraMode::Spotlight); Heli->UpdateCamera(0);
	TestTrue(TEXT("Spotlight eye sits below tilted fuselage"),Heli->SpotlightCameraComponent->GetComponentLocation().Z<Heli->HeliBodyMeshComponent->Bounds.GetBox().Min.Z);
	TestTrue(TEXT("Spotlight view excludes its own aircraft"),PC->HiddenActors.Contains(Heli));
	FMinimalViewInfo View; Heli->CalcCamera(0,View);
	TestTrue(TEXT("Actual pawn camera uses the spotlight eye"),View.Location.Equals(Heli->SpotlightCameraComponent->GetComponentLocation()));
	Heli->SetCameraMode(ESimCopterCameraMode::Chase); Heli->UpdateCamera(0);
	TestFalse(TEXT("Chase view restores the aircraft"),PC->HiddenActors.Contains(Heli));

	// Match both livery regions on the actual built mesh, rather than only testing a palette helper.
	int32 Dark=0,White=0;
	const auto* Body=Heli->HeliBodyMeshComponent->GetProcMeshSection(0);
	for (const auto& Vertex : Body->ProcVertexBuffer)
	{
		// Procedural mesh stores these as linear UNORM colors, not sRGB palette bytes.
		const FColor Color = Vertex.Color.ReinterpretAsLinear().ToFColor(true);
		if (Color.B>Color.R && Color.R>20 && Color.R<90) ++Dark;
		if (Color.R>140 && Color.G>140 && Color.B>140) ++White;
	}
	TestTrue(TEXT("Agusta has catalog slate trim and white cabin"),Dark>50 && White>50);

	Heli->CurrentPitchDeg=Heli->CurrentRollDeg=0; Heli->bIsLanded=false;
	Heli->SeedFlightModelFromActor(); Heli->bEngineRunning=true;
	Heli->ExitHelicopter();
	auto* Pilot=Cast<ASimCopterOnFootPawn>(PC->GetPawn());
	if (!TestNotNull(TEXT("Airborne exit gives control to pilot"),Pilot)) return false;
	TestTrue(TEXT("Abandoned aircraft enters crash descent"),Heli->FlightModel.State==ESimCopterFlightState::Dying);
	TestFalse(TEXT("Abandoned engine shuts down"),Heli->bEngineRunning);
	const double HeliZ=Heli->GetActorLocation().Z; Heli->SimulateFlightStep(0.05f);
	TestTrue(TEXT("Unpiloted helicopter loses altitude"),Heli->GetActorLocation().Z<HeliZ);

	Pilot->bFindOrSpawnParkedHelicopterOnBeginPlay=false;
	Pilot->ParkedHelicopter=nullptr;
	auto* Floor=World->SpawnActor<AActor>(); auto* FloorBox=NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(FloorBox); FloorBox->SetBoxExtent(FVector(10000,10000,20));
	FloorBox->SetCollisionResponseToAllChannels(ECR_Block); FloorBox->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-20));
	City->WaterGameplayTerrainClasses.Init(10,128*128);
	bool bCrashObserved=false;
	for (int32 Step=0;Step<1000 && !bCrashObserved;++Step)
	{
		Heli->SimulateFlightStep(0.05f);
		bCrashObserved=Heli->LastFlightEvents.bCrashed;
	}
	TestTrue(TEXT("Unpiloted descent reaches the crash landing path"),bCrashObserved);
	auto* Move=Pilot->GetCharacterMovement(); Move->bRunPhysicsWithNoController=true;
	auto Fall = [&](bool bChute, bool bLate)
	{
		Pilot->PilotHealth=100; Pilot->InjurySeconds=0;
		Pilot->BeginAirborneExit(FVector(7000,0,2000),FVector::ZeroVector);
		if (bChute && !bLate) Pilot->ToggleParachute();
		for (int32 I=0; I<1600 && Move->IsFalling(); ++I)
		{
			if (bLate && Pilot->GetActorLocation().Z<100 && !Pilot->bParachuteDeployed) Pilot->ToggleParachute();
			Pilot->UpdateAirOperations(0.02f); Move->TickComponent(0.02f,LEVELTICK_All,nullptr);
		}
		TestFalse(TEXT("Character movement reaches the floor"),Move->IsFalling());
	};
	Fall(false,false); TestTrue(TEXT("Real unprotected fall injures pilot"),Pilot->PilotHealth<100);
	Fall(true,false); TestEqual(TEXT("Early parachute deployment permits safe landing"),Pilot->PilotHealth,100.0f);
	TestFalse(TEXT("Canopy stows on touchdown"),Pilot->bParachuteDeployed);
	Fall(true,true); TestTrue(TEXT("Last-moment deployment does not erase impact damage"),Pilot->PilotHealth<100);
	Pilot->PilotHealth=100; Pilot->BeginAirborneExit(FVector(7000,0,2000),FVector(0,0,-100)); Pilot->ToggleParachute();
	TestTrue(TEXT("Deployed parachute has visible mesh"),Pilot->ParachuteMesh && Pilot->ParachuteMesh->IsVisible() && Pilot->ParachuteMesh->GetProcMeshSection(0)->ProcVertexBuffer.Num()>300);
	TArray<uint8> PilotSave;
	TestTrue(TEXT("Open canopy saves"),Pilot->CaptureRuntimeSaveState(PilotSave)); Pilot->ToggleParachute();
	TestTrue(TEXT("Open canopy restores"),Pilot->RestoreRuntimeSaveState(PilotSave));
	TestTrue(TEXT("Saved canopy stays open in midair"),Pilot->bParachuteDeployed && Pilot->ParachuteMesh->IsVisible());
	Pilot->ToggleParachute(); TestFalse(TEXT("Parachute toggle closes visibly"),Pilot->ParachuteMesh->IsVisible());
	TestEqual(TEXT("Closing restores gravity"),Move->GravityScale,Pilot->GravityScale);
	Pilot->StartTaserAim(); TestTrue(TEXT("Taser is a visible detailed mesh"),Pilot->TaserMesh && Pilot->TaserMesh->IsVisible() && Pilot->TaserMesh->GetProcMeshSection(0)->ProcVertexBuffer.Num()>300);
	Pilot->StopTaserAim(); TestFalse(TEXT("Taser stows when aim released"),Pilot->TaserMesh->IsVisible());

	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>(); Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
	TestTrue(TEXT("Tow mission created"),Missions->CreateMissionAt(64,64,TYPE_VehicleTow)!=INDEX_NONE);
	TestTrue(TEXT("Hull recovery mission created"),Missions->CreateMissionAt(65,65,TYPE_BoatTow)!=INDEX_NONE);
	TArray<FSimCopterMissionWorldMarkerEntry> Markers; Missions->BuildMissionWorldMarkers(Markers);
	TestTrue(TEXT("Vehicle marker has explicit name"),Markers.ContainsByPredicate([](const auto& M){return M.Label==TEXT("STALLED VEHICLE");}));
	TestTrue(TEXT("Boat marker has explicit name"),Markers.ContainsByPredicate([](const auto& M){return M.Label==TEXT("BOAT RECOVERY");}));
	TestEqual(TEXT("Tow display name is descriptive"),FString(FSimCopterMissionSystem::GetTypeDisplayName(TYPE_VehicleTow)),FString(TEXT("Stalled vehicle")));

	// Export exact generated geometry for an offline visual inspection.
	auto Export = [&](UProceduralMeshComponent* Mesh, const TCHAR* Name)
	{
		FString Obj; int32 Offset=1;
		for (int32 K=0; K<Mesh->GetNumSections(); ++K)
		{
			const auto* Section=Mesh->GetProcMeshSection(K); if (!Section) continue;
			Obj+=FString::Printf(TEXT("g section%d\n"),K);
			for (const auto& V : Section->ProcVertexBuffer) Obj+=FString::Printf(TEXT("v %f %f %f %f %f %f\n"),V.Position.X,V.Position.Y,V.Position.Z,V.Color.R/255.f,V.Color.G/255.f,V.Color.B/255.f);
			for (int32 I=0; I<Section->ProcIndexBuffer.Num(); I+=3) Obj+=FString::Printf(TEXT("f %d %d %d\n"),Offset+Section->ProcIndexBuffer[I],Offset+Section->ProcIndexBuffer[I+1],Offset+Section->ProcIndexBuffer[I+2]);
			Offset+=Section->ProcVertexBuffer.Num();
		}
		FFileHelper::SaveStringToFile(Obj,*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/rescue-pilot-fixes")/Name));
	};
	Export(Heli->HeliBodyMeshComponent,TEXT("agusta.obj")); Export(Pilot->TaserMesh,TEXT("taser.obj")); Export(Pilot->ParachuteMesh,TEXT("parachute.obj"));
	return true;
}
#endif
