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
		Agent->SetInitialBehaviorClass(State == 5 ? 12 : 0);
		Agent->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
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
	TestTrue(TEXT("Fire margin is not permission to walk back into flames"), Missions->ShouldAvoidFireStep(FVector(575,-200,0), FVector(530,-200,0)));
	TestTrue(TEXT("Escape cannot cross through the centre to the opposite side"), Missions->ShouldAvoidFireStep(FVector(550,-200,0), FVector(300,-200,0)));
	TestFalse(TEXT("Escape toward the nearest safe edge is allowed"), Missions->ShouldAvoidFireStep(FVector(550,-200,0), FVector(620,-200,0)));
	auto* Arsonist = Person(11, FVector(500,-200,0));
	Arsonist->ThrowProjectileAtSelection(Arsonist->BehaviorContext, false, true);
	TestEqual(TEXT("Lighting a firebomb starts ten seconds of fire protection"), Arsonist->ArsonFireGraceSeconds, 10.0f);
	Arsonist->Tick(0);
	TestFalse(TEXT("Own ignition does not immediately injure the arsonist"), Arsonist->IsMedevacVictim());
	TestTrue(TEXT("Arsonist chooses an escape route while lighting the fire"), Arsonist->IsAvoidanceMoveActive());
	TestFalse(TEXT("Escape target lies outside flames and their safety margin"), Missions->IsPedestrianFireHazard(
		Arsonist->AvoidanceMoveTargetLocation-FVector(0,0,Arsonist->GetCapsuleHalfHeightCm()),40));
	const FVector ArsonStart = Arsonist->GetActorLocation();
	Arsonist->UpdateMovement(.5f);
	TestTrue(TEXT("Arsonist moves away during the grace period"), FVector::Dist2D(ArsonStart,Arsonist->GetActorLocation()) > 1);
	Arsonist->bMissionStationary = true; // Keep this one in the hazard to check finite protection.
	Arsonist->SetActorLocation(ArsonStart);
	Arsonist->bBehaviorActive = false;
	Arsonist->Tick(9.9f);
	TestFalse(TEXT("Protection lasts almost ten seconds"), Arsonist->IsMedevacVictim());
	TArray<uint8> ArsonSave;
	TestTrue(TEXT("Remaining fire protection saves"), Arsonist->CaptureRuntimeSaveState(ArsonSave));
	Arsonist->ArsonFireGraceSeconds = 10;
	TestTrue(TEXT("Remaining fire protection restores"), Arsonist->RestoreRuntimeSaveState(ArsonSave));
	TestTrue(TEXT("Load cannot renew the ten-second window"), Arsonist->ArsonFireGraceSeconds < .11f);
	// This synthetic world has no physical floor; put the saved body back in the test fire.
	Arsonist->SetActorLocation(FVector(500,-200,Arsonist->GetCapsuleHalfHeightCm()));
	TestTrue(TEXT("Expiry fixture is in physical fire contact"),Missions->IsPedestrianFireHazard(Arsonist->GetActorLocation()-FVector(0,0,Arsonist->GetCapsuleHalfHeightCm())));
	Arsonist->bBehaviorActive = true; // The real medical conversion hands control to the patient's VM.
	Arsonist->Tick(.11f);
	TestEqual(TEXT("Grace expires after ten seconds of simulation time"),Arsonist->ArsonFireGraceSeconds,0.0f);
	TestTrue(TEXT("An arsonist still trapped in fire is vulnerable after expiry"), Arsonist->IsMedevacVictim());
	Arsonist->Destroy();
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
	TestTrue(TEXT("Visible cabin tracks two additional people"), Cabin && Cabin->ProcVertexBuffer.Num() > PilotVertices);
	TestNotNull(TEXT("First patient's own head is in the cabin"), Heli->CabinOccupantsMesh->GetProcMeshSection(2));
	TestNotNull(TEXT("Second patient's own head is in the cabin"), Heli->CabinOccupantsMesh->GetProcMeshSection(3));
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
	Victim->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 80;
	BurnVictim->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 15;
	auto* First=Traffic->FindMedevacPassengerAboard(Heli,MedicA);
	TestTrue(TEXT("Hospital medic unloads the most wounded living patient first, regardless of spawn or seat order"), First == BurnVictim);
	MedicA->BehaviorContext.SelectedObject=Heli; // BHAV 263 changes selection during approach
	auto* Second=Traffic->FindMedevacPassengerAboard(Heli,MedicB);
	TestTrue(TEXT("Second medic takes the next unclaimed patient"), Second == Victim);
	TestTrue(TEXT("Two roof workers reserve different patients while approaching"), First && Second && First!=Second);
	TestTrue(TEXT("First medic retains own reservation"), Traffic->FindMedevacPassengerAboard(Heli,MedicA)==First);
	if(First && Second)
	{
		First->AlightFromCarrier(); Second->AlightFromCarrier();
		TestTrue(TEXT("First patient can be carried immediately"),First->BoardCarrier(MedicA,false,false,true));
		TestTrue(TEXT("Second patient can be carried at the same time"),Second->BoardCarrier(MedicB,false,false,true));
		TestEqual(TEXT("Parallel handoff frees both seats"),Heli->GetMissionPassengerCount(MedicalEvent,ESimCopterMissionPassengerKind::Medevac),0);
	}
	// A written-off passenger must remain recoverable, but must not delay a living casualty.
	auto* TriageCarrier = World->SpawnActor<AActor>();
	auto* Body = Person(0,FVector::ZeroVector);
	Body->BehaviorCarrier = TriageCarrier;
	Body->BehaviorContext.Attributes[EBhavAttr::WrittenOff] = 1;
	Body->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 0;
	auto* Critical = Person(6,FVector::ZeroVector);
	Critical->BehaviorCarrier = TriageCarrier;
	Critical->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 1;
	TestTrue(TEXT("A living critical patient takes priority over a body"), Traffic->FindMedevacPassengerAboard(TriageCarrier) == Critical);
	Critical->BehaviorCarrier.Reset();
	TestTrue(TEXT("Bodies are still recoverable after living patients"), Traffic->FindMedevacPassengerAboard(TriageCarrier) == Body);
	Body->Destroy(); Critical->Destroy(); TriageCarrier->Destroy();
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
	Traffic->AirportOriginTile = FIntPoint(62,62);
	Missions->MarchingBandAgents.Add(Band);
	Missions->UpdateMarchingBandApproach(FVector(800,0,0),1);
	TestTrue(TEXT("Band starts in formation"),Band->bBandFormationActive);
	Missions->UpdateMarchingBandApproach(FVector(800,0,0),10);
	TestTrue(TEXT("Spacing remains active in the next ten-second formation"),Band->bBandFormationActive);
	Missions->UpdateMarchingBandApproach(FVector(800,0,0),10);
	TestTrue(TEXT("Band keeps guidance through another ten-second formation"),Band->bBandFormationActive);
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
	TestTrue(TEXT("A distant listener beyond the local grid is reachable after going around the wall"),
		SimCopterBandNavigation::FindPath(FVector::ZeroVector,FVector(6000,0,0),CanWalk,Path));
	Previous = FVector::ZeroVector;
	for (const FVector& Next : Path)
	{
		TestTrue(TEXT("Long approach shortcuts still respect physical obstacles"), CanWalk(Previous, Next));
		Previous = Next;
	}
	TestTrue(TEXT("Long approach reaches the real listener"), Previous.Equals(FVector(6000,0,0)));
	TestFalse(TEXT("Unreachable formation target never teleports"),SimCopterBandNavigation::FindPath(FVector::ZeroVector,FVector(600,0,0),[](const FVector&,const FVector&){return false;},Path));
	const int32 OriginalCounts[] = {5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,26,26,28,28,30,30,32,32,51,51,54,54,57,57};
	for (int32 City = 0; City < UE_ARRAY_COUNT(OriginalCounts); ++City)
		TestEqual(TEXT("Roster matches original city spawn loop"), SimCopterBandNavigation::GetOriginalBandCount(City), OriginalCounts[City]);
	for (const int32 Count : {5, 8, 26, 57})
		for (int32 Shape = 0; Shape < 3; ++Shape)
			for (int32 A = 0; A < Count; ++A)
			{
				const FVector Offset = SimCopterBandNavigation::FormationOffset(A, Count, Shape);
				TestTrue(TEXT("Every shape leaves space for the listener"), Offset.Size2D() >= 250);
				TestTrue(TEXT("Group clusters on its approach side instead of forming a full ring"), Offset.X > 0);
				for (int32 B = A + 1; B < Count; ++B)
					TestTrue(TEXT("Even the largest band has distinct spaced slots"),
						FVector::Dist2D(Offset, SimCopterBandNavigation::FormationOffset(B, Count, Shape)) >= 120);
			}
	TestEqual(TEXT("First shape lasts ten seconds"), SimCopterBandNavigation::GetFormationIndex(9.99f), 0);
	TestEqual(TEXT("Second shape starts at ten seconds"), SimCopterBandNavigation::GetFormationIndex(10), 1);
	TestEqual(TEXT("Third shape starts at twenty seconds"), SimCopterBandNavigation::GetFormationIndex(20), 2);
	TestEqual(TEXT("Shapes repeat at thirty seconds"), SimCopterBandNavigation::GetFormationIndex(30), 0);
	TestFalse(TEXT("A route cannot cut through personal space with both endpoints outside"),
		SimCopterBandNavigation::KeepsDistance(FVector(-400,0,0), FVector(400,0,0), FVector::ZeroVector, 250));
	TestTrue(TEXT("An overlapped musician may retreat"),
		SimCopterBandNavigation::KeepsDistance(FVector(100,0,0), FVector(150,0,0), FVector::ZeroVector, 250));
	TestFalse(TEXT("An overlapped musician cannot move closer"),
		SimCopterBandNavigation::KeepsDistance(FVector(100,0,0), FVector(50,0,0), FVector::ZeroVector, 250));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterBandCelebrationTest,"SimCopter.NpcMedical.BandClusterAndSpacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterBandCelebrationTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
	Traffic->ActiveTileSize = 400;
	Traffic->AirportOriginTile = FIntPoint(62,62);
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr, 1);
	FVector Center;
	TestTrue(TEXT("Airport pad is available"), Traffic->TryGetAirportPadWorldLocation(0, Center));
	TestTrue(TEXT("First-island completion data loads"),Missions->MissionSystem.LoadCareerData(
		FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame/tweak/career.twk")));
	Missions->MissionSystem.SelectCareerCity(0); Missions->MissionSystem.BeginSession();
	Missions->SessionMode=ESimCopterMissionSessionMode::CityJobs;
	Missions->MissionSystem.AddScore(Missions->MissionSystem.GetCareerCity().PointsNeeded);
	TestTrue(TEXT("Reaching the initial island's score triggers completion"),Missions->MissionSystem.CheckLevelCompletion());
	Missions->ProcessLevelCompleteLanding(0);
	TestEqual(TEXT("First-island completion spawns three musicians from an empty roster"), Missions->MarchingBandAgents.Num(), 3);
	// Reproduce a final job completed across town: the traffic tick runs before the return
	// flight. The band has no event ID, but must not be discarded as ambient pedestrians.
	const auto AirportBand=Missions->MarchingBandAgents;
	const FVector DistantJob=Center+FVector(20000,0,0);
	Traffic->PruneAgentArray(Traffic->PedestrianAgents,DistantJob);
	for (auto Weak : AirportBand)
		TestTrue(TEXT("Airport musician survives distant final-job population cleanup"),Weak.IsValid() && !Weak->IsActorBeingDestroyed());
	// Existing saves/running cities may already have lost their band. The completed flag
	// must not prevent replacement; retain valid members and retry only the missing seats.
	if (AirportBand.Num()>0 && AirportBand[0].IsValid()) AirportBand[0]->Destroy();
	Missions->UpdateMarchingBandApproach(Center,0);
	Missions->NextMarchingBandSpawnTime=0;
	Missions->ProcessLevelCompleteLanding(0);
	TestEqual(TEXT("Returning to the airport repairs an already-spawned incomplete band"),Missions->MarchingBandAgents.Num(),3);
	int32 RepairedLeaders=0;
	for (auto Weak : Missions->MarchingBandAgents)
		if (Weak.IsValid() && !Weak->IsActorBeingDestroyed()) RepairedLeaders+=Weak->GetBehaviorAttribute(EBhavAttr::State)==17;
	TestEqual(TEXT("Repaired greeting includes the original marching-music leader"),RepairedLeaders,1);
	Missions->MarchingBandLastPhase=INDEX_NONE;
	for (auto Weak : Missions->MarchingBandAgents) if (auto* Agent=Weak.Get()) Agent->Destroy();
	Missions->MarchingBandAgents.Reset(); Missions->bMarchingBandSpawned=false; Missions->NextMarchingBandSpawnTime=0;
	for (int32 I = 0; I < 8; ++I)
	{
		auto* Agent = World->SpawnActor<ASimCopterGroundAgent>();
		Agent->SetOwner(Traffic);
		Agent->BehaviorContext.ResetToState(I == 0 ? 17 : 18);
		Agent->SetActorLocation(Center + FVector(-900, I * 60, Agent->GetCapsuleHalfHeightCm() + 1));
		Missions->MarchingBandAgents.Add(Agent);
	}
	Missions->SpawnMarchingBandAtAirport();
	TestEqual(TEXT("An old first-city save restores the requested three musicians"), Missions->MarchingBandAgents.Num(), 3);
	Missions->SpawnMarchingBandAtAirport();
	TestEqual(TEXT("Repeated initialization does not duplicate musicians"), Missions->MarchingBandAgents.Num(), 3);
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame"));
	TestTrue(TEXT("Original career data loads"), Missions->MissionSystem.LoadCareerData(Root / TEXT("tweak/career.twk")));
	TestTrue(TEXT("Last city can be selected"), Missions->MissionSystem.SelectCareerCity(29));
	Missions->bMarchingBandSpawned = false;
	Missions->NextMarchingBandSpawnTime = 0;
	Missions->SpawnMarchingBandAtAirport();
	TestEqual(TEXT("Last city restores all 57 original musicians"), Missions->MarchingBandAgents.Num(), 57);
	for (auto Weak : Missions->MarchingBandAgents)
	{
		const FVector Location = Weak->GetActorLocation();
		TestTrue(TEXT("Even the largest band spawns entirely on the base"),
			FMath::Abs(Location.X) < 800 && FMath::Abs(Location.Y) < 800);
	}
	int32 Leaders = 0;
	for (auto Weak : Missions->MarchingBandAgents) Leaders += Weak->GetBehaviorAttribute(EBhavAttr::State) == 17;
	TestEqual(TEXT("Restoring the roster preserves one music leader"), Leaders, 1);
	// Exercise free formation changes with room on the approach side. The old fixture
	// put the listener at an outer pad and expected musicians to walk past the perimeter.
	Center = FVector(-400, 0, 0);
	Missions->MarchingBandApproachDirection = FVector::ForwardVector;
	Missions->UpdateMarchingBandApproach(Center, 0);
	TArray<FVector> Targets;
	for (auto Weak : Missions->MarchingBandAgents)
	{
		auto* Agent = Weak.Get();
		TestTrue(TEXT("Every musician keeps a buffer around the player"), FVector::Dist2D(Center, Agent->BandFormationTarget) >= 250);
		for (const FVector& Other : Targets)
			TestTrue(TEXT("Resolved slots never overlap another reservation"), FVector::Dist2D(Other, Agent->BandFormationTarget) >= 120);
		Targets.Add(Agent->BandFormationTarget);
		const FVector TowardPlayer = (Center - Agent->BandFormationTarget).GetSafeNormal2D();
		TestTrue(TEXT("Musicians face the player when they arrive"), FVector::DotProduct(TowardPlayer, FRotator(0,Agent->BandFormationYaw,0).Vector()) > 0.99);
		TestTrue(TEXT("All members use obstacle-aware approach guidance"), Agent->bBandFormationActive);
	}
	const FVector MovedCenter = Center + FVector(350, 0, 0);
	Missions->UpdateMarchingBandApproach(MovedCenter, 1);
	TestTrue(TEXT("Cluster follows a moving listener before the next formation change"),
		(Missions->MarchingBandAgents[0]->BandFormationTarget - Targets[0]).Equals(FVector(350,0,0), 1));
	const FVector FirstTarget = Missions->MarchingBandAgents[0]->BandFormationTarget;
	Missions->UpdateMarchingBandApproach(MovedCenter, 8.99f);
	TestTrue(TEXT("Shape is stable just before ten seconds"), Missions->MarchingBandAgents[0]->BandFormationTarget.Equals(FirstTarget, 1));
	Missions->UpdateMarchingBandApproach(MovedCenter, 0.02f);
	const FVector SecondTarget = Missions->MarchingBandAgents[0]->BandFormationTarget;
	TestTrue(TEXT("Formation actually changes at ten seconds"), FVector::Dist2D(SecondTarget, FirstTarget) > 40);
	Missions->UpdateMarchingBandApproach(MovedCenter, 10);
	TestTrue(TEXT("Another distinct shape appears after ten more seconds"), FVector::Dist2D(Missions->MarchingBandAgents[0]->BandFormationTarget, SecondTarget) > 40);
	auto* Musician = Missions->MarchingBandAgents[0].Get();
	auto* Listener = World->SpawnActor<ASimCopterGroundAgent>();
	Listener->SetActorLocation(MovedCenter);
	Musician->SetActorLocation(Musician->BandFormationTarget);
	Musician->BehaviorContext.bHasSelection = true;
	Musician->BehaviorContext.SelectedObject = Listener;
	TestTrue(TEXT("Original BHAV approach completes at its spaced target, allowing instrument routines to run"),
		Musician->StepTowardSelectedObject(Musician->BehaviorContext) == ESimCopterBehaviorStepResult::Arrived);
	const FVector MarchStart = Musician->BandFormationTarget + Missions->MarchingBandApproachDirection * 300;
	Musician->SetActorLocation(MarchStart);
	Musician->BandNeighbors.Reset();
	Musician->BandPath.Reset(); Musician->bBandPathAttempted = false;
	for (int32 Tick = 0; Tick < 100; ++Tick) Musician->UpdateMovement(0.1f);
	TestTrue(TEXT("Musician marches into the cluster at the original walking speed"), FVector::Dist2D(Musician->GetActorLocation(), Musician->BandFormationTarget) <= 40);
	TestTrue(TEXT("Arrival leaves the listener's personal space clear"), FVector::Dist2D(Musician->GetActorLocation(), MovedCenter) >= 250);
	auto* Neighbor = Missions->MarchingBandAgents[1].Get();
	Neighbor->SetActorLocation(Musician->GetActorLocation() + FVector(120,0,0));
	Musician->BandNeighbors.Add(Neighbor);
	TestFalse(TEXT("Marching musicians yield before overcrowding a neighbor"), Musician->CanTakeBandStep(Musician->GetActorLocation() + FVector(30,0,0)));
	Musician->BandNeighbors.Reset();
	Musician->BandListenerClearance = 0; // The following fixtures isolate wall and aircraft routing.
	Musician->BandPath.Reset(); Musician->bBandPathAttempted = false;
	Musician->SetActorLocation(MovedCenter + FVector(-900, 0, Musician->GetCapsuleHalfHeightCm() + 1));
	FVector Waypoint;
	const FVector FirstGoal = MovedCenter + FVector(250, 0, 0);
	TestTrue(TEXT("Initial approach builds a usable path"), Musician->GetBandWaypoint(FirstGoal, Waypoint));
	TestFalse(TEXT("Moving listener cannot leave the musician walking toward an obsolete endpoint"),
		Musician->GetBandWaypoint(FirstGoal + FVector(350,0,0), Waypoint));
	// An actual world collision at a cluster slot should choose a nearby open position in that sector.
	auto* Wall = World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(45,45,100));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	const FVector Desired = MovedCenter + FVector(250,0,0);
	Wall->SetActorLocation(Desired + FVector(0,0,100));
	FVector Resolved;
	TestTrue(TEXT("Blocked cluster slot finds a safe nearby alternative"), Musician->TryResolveBandStandLocation(Desired, MovedCenter, Resolved));
	TestTrue(TEXT("Alternative stays outside the obstacle"), FVector::Dist2D(Resolved, Desired) > 50);
	Wall->Destroy();
	auto* Helicopter = World->SpawnActor<ASimCopterHelicopterPawn>();
	Helicopter->SetActorLocation(MovedCenter + FVector(0,0,100));
	Helicopter->AddActorWorldOffset(FVector(0,0,-Helicopter->GetParkingWorldBounds().Min.Z));
	const FBox Aircraft = Helicopter->GetParkingWorldBounds();
	const float BodyZ = Musician->GetCapsuleHalfHeightCm() + 1;
	const FVector RouteStart(Aircraft.Min.X - 150, Aircraft.GetCenter().Y, BodyZ);
	const FVector RouteEnd(Aircraft.Max.X + 150, Aircraft.GetCenter().Y, BodyZ);
	TestFalse(TEXT("Band does not march through an aircraft that ignores the Camera collision channel"),
		Musician->CanWalkBandSegment(RouteStart, RouteEnd));
	TArray<FVector> AircraftPath;
	TestTrue(TEXT("Band can route around the parked aircraft"), SimCopterBandNavigation::FindPath(RouteStart, RouteEnd,
		[Musician](const FVector& A, const FVector& B) { return Musician->CanWalkBandSegment(A, B); }, AircraftPath));
	FVector Last = RouteStart;
	for (const FVector& Next : AircraftPath)
	{
		TestTrue(TEXT("All aircraft detour segments remain clear"), Musician->CanWalkBandSegment(Last, Next));
		Last = Next;
	}
	TestTrue(TEXT("A slot blocked by an aircraft is replaced with a nearby clear slot"),
		Musician->TryResolveBandStandLocation(FVector(Aircraft.GetCenter().X,Aircraft.GetCenter().Y,BodyZ),
			MovedCenter - FVector(100,0,0), Resolved));
	TestTrue(TEXT("Resolved slot also clears aircraft collision"), Musician->CanWalkBandSegment(Resolved, Resolved));
	Helicopter->AddActorWorldOffset(FVector(0,0,1000));
	TestTrue(TEXT("An aircraft overhead does not obstruct the ground route"), Musician->CanWalkBandSegment(RouteStart, RouteEnd));
	Helicopter->Destroy();
	// The airport in this fixture covers [-800,800] on both world axes. A listener
	// just outside it used to drag the entire greeting into the surrounding city.
	for (const FVector Player : {FVector(1800,600,0), FVector(-1800,600,0),
		FVector(0,1800,0), FVector(0,-1800,0), DistantJob})
	{
		for (int32 Shape = 0; Shape < 3; ++Shape)
		{
			Missions->UpdateMarchingBandApproach(Player, 10);
			bool bAllAtBase = true;
			for (auto Weak : Missions->MarchingBandAgents)
			{
				const FVector Target = Weak->BandFormationTarget;
				bAllAtBase &= FMath::Abs(Target.X) < 800 && FMath::Abs(Target.Y) < 800;
			}
			TestTrue(TEXT("Every formation stays on the hangar base when the player leaves"), bAllAtBase);
		}
	}
	Musician->BandListenerClearance = 0;
	Musician->BandNeighbors.Reset();
	const FVector InsideEdge(700, 0, BodyZ), OutsideEdge(900, 0, BodyZ);
	TestFalse(TEXT("Band paths cannot leave the base perimeter"), Musician->CanWalkBandSegment(InsideEdge, OutsideEdge));
	TestFalse(TEXT("An outside point cannot become a standing slot"), Musician->CanWalkBandSegment(OutsideEdge, OutsideEdge));
	TestTrue(TEXT("A musician displaced outside may walk back toward the base"),
		Musician->CanWalkBandSegment(OutsideEdge, FVector(850,0,BodyZ)));
	TestFalse(TEXT("A displaced musician cannot stray farther"),
		Musician->CanWalkBandSegment(OutsideEdge, FVector(950,0,BodyZ)));
	Musician->SetActorLocation(InsideEdge);
	Musician->SetBandFormationTarget(OutsideEdge, 0, true);
	Musician->BandPath.Reset(); Musician->bBandPathAttempted = false;
	for (int32 Tick = 0; Tick < 50; ++Tick) Musician->UpdateMovement(0.1f);
	TestTrue(TEXT("Actual marching steps stay inside even with an obsolete outside target"), Musician->GetActorLocation().X < 800);
	Musician->SetActorLocation(OutsideEdge);
	Musician->SetBandFormationTarget(InsideEdge, 180, true);
	Musician->BandPath.Reset(); Musician->bBandPathAttempted = false;
	for (int32 Tick = 0; Tick < 50; ++Tick) Musician->UpdateMovement(0.1f);
	TestTrue(TEXT("An old saved straggler physically returns to the base"), Musician->GetActorLocation().X < 800);
	Musician->SetBandFormationTarget(Resolved, 180, true);
	Musician->BehaviorContext.ResetToState(6);
	Musician->UpdateMovement(0);
	TestFalse(TEXT("An injury immediately releases formation control"), Musician->bBandFormationActive);
	TestFalse(TEXT("Patient no longer has a celebration target"), Musician->bHasBandCelebrationTarget);
	// Reuse the hangar's airport basis, including transformed and off-map fallback bases.
	Traffic->ActiveCityToWorldTransform = FTransform(FRotator(0,37,0), FVector(4300,-700,120), FVector(1.5,0.8,1));
	for (const FIntPoint Origin : {FIntPoint(62,62), FIntPoint(128,128)})
	{
		Traffic->AirportOriginTile = Origin;
		SimCopterBandNavigation::FBaseArea Base;
		TestTrue(TEXT("Rotated and fallback airport boundaries resolve"), SimCopterBandNavigation::TryGetBaseArea(Traffic, Base));
		for (int32 PadIndex = 0; PadIndex < 12; ++PadIndex)
		{
			FVector Pad;
			Traffic->TryGetAirportPadWorldLocation(PadIndex, Pad);
			TestTrue(TEXT("Every transformed airport pad remains in bounds"), Base.Contains(Pad));
		}
		for (const FVector Direction : {Base.AxisX, -Base.AxisX, Base.AxisY, -Base.AxisY})
		{
			const FVector Outside = Base.Center + Direction * 3000;
			TestFalse(TEXT("Rotated perimeter excludes the surrounding city"), Base.Contains(Outside));
			TestTrue(TEXT("Clamped targets include body clearance"), Base.Contains(Base.Clamp(Outside, 30), 30));
			TestFalse(TEXT("Path cannot leave a rotated base"), Base.AllowsStep(Base.Center, Outside, 30));
		}
	}
	GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}
#endif
