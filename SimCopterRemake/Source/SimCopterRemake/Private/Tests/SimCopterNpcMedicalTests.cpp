#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterBandNavigation.h"
#include "Ground/SimCopterPopulationFigure.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterHelicopterPresentation.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterPeopleReader.h"
#include "UI/SimCopterHangarShop.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterNpcMedicalTest, "SimCopter.NpcMedical.IncidentsCrewAndCabin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterNpcMedicalTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterMissions;
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
	Traffic->ActiveTileSize = 400;
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr, 1);
	Missions->MissionSystem.BeginSession();
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame"));
	TSharedPtr<FPeopleBehaviorModel> Model = MakeShared<FPeopleBehaviorModel>();
	FString Error;
	TestTrue(TEXT("Original people programs load"), FSimCopterPeopleReader::LoadFromFile(FSimCopterPeopleReader::ResolvePeoplePath(Root), *Model, Error));
	auto Person = [&](int32 State, FVector Location)
	{
		auto* Agent = World->SpawnActor<ASimCopterGroundAgent>();
		Agent->SetOwner(Traffic); Agent->InitialPersonState = State;
		Agent->BehaviorModel = Model; Agent->bBehaviorActive = true;
		Agent->BehaviorContext.ResetToState(State);
		Agent->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 100;
		Agent->SetActorLocation(Location + FVector(0, 0, Agent->GetCapsuleHalfHeightCm()));
		Traffic->PedestrianAgents.Add(Agent);
		return Agent;
	};
	auto* Mugger = Person(12, FVector(200, -200, 0));
	auto* Victim = Person(0, FVector(240, -200, 0));
	Mugger->BehaviorContext.SelectedObject = Victim;
	Mugger->BehaviorContext.bHasSelection = true;
	TestTrue(TEXT("Original mugger death opcode now starts medical incident"), Mugger->PushReactionOnSelectedObject(Mugger->BehaviorContext, 903));
	TestFalse(TEXT("Mugging victim is alive"), Victim->IsMissionPatientDead());
	TestTrue(TEXT("Mugging victim needs evacuation"), Victim->IsMedevacVictim());
	TestEqual(TEXT("Mugging does not fine the pilot"), Missions->MissionSystem.GetCash(), FSimCopterMissionSystem::SessionStartingCash);
	const int32 MedicalEvent = Victim->MissionEventId;
	TestFalse(TEXT("Existing victim does not allocate twice"), Missions->CreateIncidentMedevacForVictim(Victim));
	TestEqual(TEXT("Victim retains the same mission"), Victim->MissionEventId, MedicalEvent);
	if (const auto* Rec = Missions->MissionSystem.FindRecord(MedicalEvent))
		TestFalse(TEXT("Incidental medevac can earn normal rewards"), Rec->bSuppressCompletionRewards);

	// Query the same cache consumed by movement and the physical-contact tick.
	const FBox Fire(FVector(450,-250,-30), FVector(550,-150,120));
	Missions->PedestrianFireHazards.FindOrAdd(FIntPoint(1,-1)).Add(Fire);
	Missions->PedestrianFireHazardsFrame = GFrameCounter;
	TestTrue(TEXT("Steps crossing flames are blocked even if endpoint is clear"), Missions->ShouldAvoidFireStep(FVector(300,-200,0), FVector(700,-200,0)));
	TestFalse(TEXT("Roof-height flame does not burn street below"), Missions->IsPedestrianFireHazard(FVector(500,-200,-100)));
	TestFalse(TEXT("A person caught in newly ignited fire may move out"), Missions->ShouldAvoidFireStep(FVector(500,-200,0), FVector(700,-200,0)));
	auto* BurnVictim = Person(0, FVector(500,-200,0));
	BurnVictim->Tick(0);
	TestTrue(TEXT("Physical fire contact creates medical victim"), BurnVictim->IsMedevacVictim());
	TestFalse(TEXT("Fire contact victim remains alive"), BurnVictim->IsMissionPatientDead());
	Missions->PedestrianFireHazards.Reset();
	auto* BurningCar=World->SpawnActor<ASimCopterGroundAgent>();
	BurningCar->SetOwner(Traffic); BurningCar->AgentKind=ESimCopterGroundAgentKind::Vehicle;
	BurningCar->SetActorLocation(FVector(1500,-200,35));
	Traffic->VehicleTrafficStates.FindOrAdd(TObjectKey<ASimCopterGroundAgent>(BurningCar)).bMissionOnFire=true;
	Missions->PedestrianFireHazardsFrame=MAX_uint64;
	TestTrue(TEXT("Burning vehicle flames overlap a street pedestrian's body"),Missions->IsPedestrianFireHazard(FVector(1500,-200,0)));
	TestFalse(TEXT("Vehicle fire does not reach an unrelated lower floor"),Missions->IsPedestrianFireHazard(FVector(1500,-200,-150)));
	Traffic->VehicleTrafficStates.Reset(); Missions->PedestrianFireHazards.Reset();

	// Train wrecks post doused, while traffic cars can post BOTH doused and cleared.
	for (const bool ClearedToo : {false, true})
	{
		FSimCopterMissionSystem System; System.Initialize(nullptr, 1); System.BeginSession();
		const int32 Event = System.CreateIncidentMedevacAt(64,64);
		auto* Rec = const_cast<FSimCopterMissionRecord*>(System.FindRecord(Event));
		Rec->TypeMask = TYPE_TrainCrash | TYPE_CarFire; Rec->MedevacVictims = 0;
		System.PostEvent(EVT_CarCrashed,Event,1); System.PostEvent(EVT_CarDoused,Event,1);
		if (ClearedToo) System.PostEvent(EVT_CarCleared,Event,1);
		const int32 Before = System.GetScore();
		System.UpdateLifecycle();
		TestEqual(TEXT("Saved burning wreck earns exactly one completion award"), System.GetScore()-Before, System.Tuning.CarFirePoints);
		TestFalse(TEXT("Extinguished train crash completes"), Rec->bActive);
	}
	{
		FSimCopterMissionSystem System; System.Initialize(nullptr,1); System.BeginSession();
		const int32 Event=System.CreateIncidentMedevacAt(64,64);
		auto* Rec=const_cast<FSimCopterMissionRecord*>(System.FindRecord(Event));
		Rec->TypeMask=TYPE_TrainCrash|TYPE_CarFire; Rec->MedevacVictims=0;
		System.PostEvent(EVT_CarCrashed,Event,1); System.PostEvent(EVT_CarBurned,Event,1);
		System.UpdateLifecycle();
		TestEqual(TEXT("Burned-out wreck cannot receive a saved-wreck reward"), System.GetScore(), 0);
	}

	auto* Controller = World->SpawnActor<ASimCopterPlayerController>();
	World->AddController(Controller);
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->ChangeState(NAME_Playing);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Controller->Possess(Heli);
	TestTrue(TEXT("Agusta actual mesh loads"), Heli->SwitchHelicopterModel(5));
	Heli->RefreshCabinOccupants();
	TestNotNull(TEXT("Agusta has separate transparent glazing"), Heli->HeliBodyMeshComponent->GetProcMeshSection(1));
	TestNotNull(TEXT("Agusta glass material is available"), Heli->CabinGlassMaterial.Get());
	const auto* Cabin = Heli->CabinOccupantsMesh->GetProcMeshSection(0);
	const int32 PilotVertices = Cabin ? Cabin->ProcVertexBuffer.Num() : 0;
	TestTrue(TEXT("Occupied Agusta has visible pilot geometry"), PilotVertices > 0);
	TestTrue(TEXT("Mugging patient boards real Agusta"), Victim->BoardCarrier(Heli,false));
	TestTrue(TEXT("Burn patient boards real Agusta"), BurnVictim->BoardCarrier(Heli,false));
	Heli->RefreshCabinOccupants();
	Cabin = Heli->CabinOccupantsMesh->GetProcMeshSection(0);
	TestEqual(TEXT("Visible cabin tracks two additional people"), Cabin ? Cabin->ProcVertexBuffer.Num() : 0, PilotVertices*3);
	// Export the actual prepared meshes for an offline cabin visibility inspection.
	FString Obj; int32 VertexBase=1;
	auto ExportPart=[&](const TCHAR* Name,const FProcMeshSection* Part)
	{
		if(!Part) return;
		Obj+=FString::Printf(TEXT("g %s\n"),Name);
		for(const auto& V:Part->ProcVertexBuffer)
			Obj+=FString::Printf(TEXT("v %f %f %f %f %f %f\n"),V.Position.X,V.Position.Y,V.Position.Z,V.Color.R/255.f,V.Color.G/255.f,V.Color.B/255.f);
		for(int32 I=0;I<Part->ProcIndexBuffer.Num();I+=3)
			Obj+=FString::Printf(TEXT("f %d %d %d\n"),VertexBase+Part->ProcIndexBuffer[I],VertexBase+Part->ProcIndexBuffer[I+1],VertexBase+Part->ProcIndexBuffer[I+2]);
		VertexBase+=Part->ProcVertexBuffer.Num();
	};
	ExportPart(TEXT("body"),Heli->HeliBodyMeshComponent->GetProcMeshSection(0));
	ExportPart(TEXT("glass"),Heli->HeliBodyMeshComponent->GetProcMeshSection(1));
	ExportPart(TEXT("occupants"),Cabin);
	FFileHelper::SaveStringToFile(Obj,*(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/npc-medical-update/agusta.obj")));
	auto* MedicA=Person(5,FVector(160,-200,0)); auto* MedicB=Person(5,FVector(180,-200,0));
	auto* First=Traffic->FindMedevacPassengerAboard(Heli,MedicA);
	MedicA->BehaviorContext.SelectedObject=Heli; // BHAV 263 changes selection during approach
	auto* Second=Traffic->FindMedevacPassengerAboard(Heli,MedicB);
	TestTrue(TEXT("Two roof workers reserve different patients while approaching"), First && Second && First!=Second);
	TestTrue(TEXT("First medic retains own reservation"), Traffic->FindMedevacPassengerAboard(Heli,MedicA)==First);
	if(First && Second)
	{
		First->AlightFromCarrier(); Second->AlightFromCarrier();
		TestTrue(TEXT("First patient can be carried immediately"),First->BoardCarrier(MedicA,false,false,true));
		TestTrue(TEXT("Second patient can be carried at the same time"),Second->BoardCarrier(MedicB,false,false,true));
		TestEqual(TEXT("Parallel handoff frees both seats"),Heli->GetMissionPassengerCount(MedicalEvent,ESimCopterMissionPassengerKind::Medevac),0);
	}
	Heli->RefreshCabinOccupants();
	Cabin=Heli->CabinOccupantsMesh->GetProcMeshSection(0);
	TestEqual(TEXT("Unloaded patients disappear from cabin"),Cabin ? Cabin->ProcVertexBuffer.Num():0,PilotVertices);
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>();
	Pilot->SetParkedHelicopter(Heli); Controller->Possess(Pilot);
	TestTrue(TEXT("Fixture exposes the current on-foot pawn to HUD"), UGameplayStatics::GetPlayerPawn(World,0)==Pilot);
	TArray<FSimCopterMissionWorldMarkerEntry> Markers; Missions->BuildMissionWorldMarkers(Markers);
	TestTrue(TEXT("On-foot HUD identifies parked helicopter"),Markers.ContainsByPredicate([](const auto& M){return M.Label==TEXT("Your helicopter");}));
	Controller->Possess(Heli); Missions->BuildMissionWorldMarkers(Markers);
	TestFalse(TEXT("Parked marker leaves HUD when flying"),Markers.ContainsByPredicate([](const auto& M){return M.Label==TEXT("Your helicopter");}));

	// A cached roof is insufficient: placement now requires real supporting geometry.
	auto* FloorActor = World->SpawnActor<AActor>();
	auto* Floor = NewObject<UBoxComponent>(FloorActor); FloorActor->SetRootComponent(Floor);
	Floor->SetBoxExtent(FVector(1500,1500,10)); Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Floor->SetCollisionResponseToAllChannels(ECR_Block); Floor->RegisterComponent(); FloorActor->SetActorLocation(FVector(200,-200,-10));
	Heli->SetActorLocation(FVector(5000,5000,5000));
	// Exercise the actual hospital spawner, including its existing-team count on repeated ticks.
	Traffic->XbldTileIds[64*128+64]=0xd1;
	auto& Hospital=Traffic->PedestrianNodes.AddDefaulted_GetRef();
	Hospital.FileX=64; Hospital.FileY=64; Hospital.BuildingId=0xd1; Hospital.PeopleFootprintSize=3;
	Hospital.Location=FVector(200,-200,0);
	Traffic->PedestrianNodeIndexByTile.Add(FIntPoint(64,64),Traffic->PedestrianNodes.Num()-1);
	Traffic->BuildingRoofPostByTile.Add(FIntPoint(64,64),FVector(200,-200,0));
	const int32 BeforeCrew=Traffic->PedestrianAgents.Num();
	TestNotNull(TEXT("Hospital posts a trauma team"),Traffic->EnsureHospitalParamedicAtTile(64,64,8));
	const int32 Posted=Traffic->PedestrianAgents.Num();
	TestTrue(TEXT("Existing two staff are supplemented to eight"),Posted>=BeforeCrew+6);
	Traffic->EnsureHospitalParamedicAtTile(64,64,8);
	TestEqual(TEXT("Repeated crew readiness does not spawn indefinitely"),Traffic->PedestrianAgents.Num(),Posted);

	auto* Band=Person(17,FVector(800,-200,0));
	Missions->MarchingBandAgents.Add(Band);
	Missions->UpdateMarchingBandApproach(FVector(800,0,0),1);
	TestTrue(TEXT("Band starts in formation"),Band->bBandFormationActive);
	Missions->UpdateMarchingBandApproach(FVector(800,0,0),150);
	TestFalse(TEXT("Band returns to original behavior after 150 seconds"),Band->bBandFormationActive);
	Missions->UpdateMarchingBandApproach(FVector(800,0,0),150);
	TestTrue(TEXT("Band returns to next formation after another 150 seconds"),Band->bBandFormationActive);
	Missions->CreateIncidentMedevacForVictim(Band);
	Missions->UpdateMarchingBandApproach(FVector::ZeroVector,0);
	TestFalse(TEXT("Medical injury cancels parade guidance"),Band->bBandFormationActive);
	TestTrue(TEXT("Band member retains medical state"),Band->IsMedevacVictim());

	auto* Dog=Person(0,FVector(1200,-200,0)); Dog->SetPedestrianFigureName(TEXT("2DOGG"));
	Dog->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian,TEXT(""),Root,85);
	if(TestTrue(TEXT("Original dog figure loads"),Dog->bUsingPedestrianFigure))
	{
		const auto& Figure=Dog->FigureShared->Model.Figures[Dog->FigureIndex];
		const auto* Walk=Dog->FigureShared->Model.FindClip(Figure,TEXT("1Wal"));
		if(!Walk) Walk=Dog->FigureShared->Model.FindClip(Figure,TEXT("DgSt"));
		if(TestNotNull(TEXT("Dog calibration clip exists"),Walk))
		{
			const auto Old=FSimCopterPopulationFigure::Calibrate(*Walk,176.0f*0.25f);
			TestEqual(TEXT("Dog geometry is one third its previous size"),Dog->FigureCalibration.ScaleCmPerUnit,Old.ScaleCmPerUnit/3,0.0001f);
		}
	}
	GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterBandNavigationTest,"SimCopter.NpcMedical.BandObstacleRoutes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterBandNavigationTest::RunTest(const FString& Parameters)
{
	TArray<FVector> Path;
	auto CanWalk=[](const FVector& A,const FVector& B)
	{
		for(int32 I=0;I<=32;++I)
		{
			const FVector P=FMath::Lerp(A,B,float(I)/32);
			if(P.X>=150 && P.X<=300 && FMath::Abs(P.Y)<200) return false;
		}
		return true;
	};
	TestTrue(TEXT("Band finds a route around a blocked direct approach"),SimCopterBandNavigation::FindPath(FVector::ZeroVector,FVector(600,0,0),CanWalk,Path));
	FVector Previous=FVector::ZeroVector;
	for(const auto& Next:Path){TestTrue(TEXT("Every route segment stays outside obstacle"),CanWalk(Previous,Next));Previous=Next;}
	TestTrue(TEXT("Route reaches target"),!Path.IsEmpty() && Path.Last().Equals(FVector(600,0,0)));
	TestFalse(TEXT("Unreachable formation target never teleports"),SimCopterBandNavigation::FindPath(FVector::ZeroVector,FVector(600,0,0),[](const FVector&,const FVector&){return false;},Path));
	for(int32 Shape=0;Shape<3;++Shape)
		for(int32 A=0;A<8;++A) for(int32 B=A+1;B<8;++B)
			TestTrue(TEXT("Each formation has body-sized spacing"),FVector::Dist(SimCopterBandNavigation::FormationOffset(A,8,Shape),SimCopterBandNavigation::FormationOffset(B,8,Shape))>=70);
	TestTrue(TEXT("Formation interval starts again at five minutes"),SimCopterBandNavigation::IsFormationPhase(300));
	TestFalse(TEXT("Original behavior occupies alternate interval"),SimCopterBandNavigation::IsFormationPhase(299));
	TArray<FColor> Original; Original.Init(FColor(15,15,15),256); Original[60]=FColor(195,195,195); Original[188]=FColor(174,164,169);
	TArray<FColor> Paint; SimCopterHelicopterPresentation::MakePaintPalette(Original,Paint);
	TestTrue(TEXT("Dark Agusta body swatch is corrected"),Paint[48].R>Original[48].R && Paint[176].R>Original[176].R);
	TestEqual(TEXT("Window and tire black stays black"),Paint[0],Original[0]);
	for(int32 Row=0;Row<8;++Row)
	{
		TestTrue(TEXT("Catalog has expanded history"),FCString::Strlen(SimCopterHangarShop::GetCatalogHistory(Row))>220);
		TestTrue(TEXT("Catalog has expanded specialties"),FCString::Strlen(SimCopterHangarShop::GetCatalogSpecialties(Row))>220);
		TestTrue(TEXT("Catalog retains specifications and adds description"),FCString::Strlen(SimCopterHangarShop::GetCatalogDescription(Row))>350);
	}
	return true;
}
#endif
