#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Game/SimCopterPlayerController.h"
#include "Components/BoxComponent.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterPeopleReader.h"

namespace
{
UWorld* CreateMedicalWorld()
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	return World;
}
AActor* MedicalBox(UWorld* World, const FVector& Center, const FVector& Extent)
{
	auto* Actor = World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Actor);
	Actor->SetRootComponent(Box);
	Box->SetBoxExtent(Extent);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	Actor->SetActorLocation(Center);
	return Actor;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPatientMarkerTest, "SimCopter.MedicalNavigation.PatientMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPatientMarkerTest::RunTest(const FString& Parameters)
{
	auto* World = CreateMedicalWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	MedicalBox(World, FVector(0,0,-10), FVector(10000,10000,10));
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
	Traffic->bRequireOriginalPopulationMeshes = false;
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr, 1);
	Missions->MissionSystem.BeginSession();
	const int32 Event = Missions->MissionSystem.CreateIncidentMedevacAt(64,64);
	ASimCopterGroundAgent* Patient = nullptr;
	if (!TestTrue(TEXT("Patient spawns"), Traffic->TrySpawnMissionPerson(6,-1,64,64,Event,FString(),&Patient))) return false;
	// Deliberately far from the record's tile center, including another elevation.
	Patient->SetActorLocation(FVector(137,-263,392));
	TArray<FSimCopterMissionWorldMarkerEntry> Markers;
	auto PatientMarkers = [&]()
	{
		Missions->BuildMissionWorldMarkers(Markers);
		Markers.RemoveAll([](const auto& M) { return M.Label != TEXT("PATIENT"); });
	};
	PatientMarkers();
	TestEqual(TEXT("One real casualty has one marker"), Markers.Num(), 1);
	if (Markers.Num()) TestTrue(TEXT("Marker follows exact patient position and elevation"), Markers[0].WorldLocation.Equals(Patient->GetActorLocation(), 1));
	Patient->SetActorLocation(FVector(310,-85,92));
	PatientMarkers();
	if (Markers.Num()) TestTrue(TEXT("Marker follows moved patient without waiting for mission coordinates"), Markers[0].WorldLocation.Equals(Patient->GetActorLocation(), 1));
	ASimCopterGroundAgent* Second = nullptr;
	if (!TestTrue(TEXT("Second patient spawns"), Traffic->TrySpawnMissionPerson(6,-1,64,64,Event,FString(),&Second))) return false;
	PatientMarkers();
	TestEqual(TEXT("Each patient in a shared mission is marked"), Markers.Num(), 2);
	Patient->SetMissionResolutionReported(true);
	PatientMarkers();
	TestEqual(TEXT("Resolved patient loses marker"), Markers.Num(), 1);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	TestTrue(TEXT("Remaining patient boards"), Second->BoardCarrier(Heli,false));
	PatientMarkers();
	TestEqual(TEXT("Carried patient leaves no phantom tile marker"), Markers.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterMedicRouteTest, "SimCopter.MedicalNavigation.DroppedMedics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterMedicRouteTest::RunTest(const FString& Parameters)
{
	auto* World = CreateMedicalWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	MedicalBox(World, FVector(0,0,-10), FVector(10000,10000,10));
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame"));
	auto Model = MakeShared<FPeopleBehaviorModel>();
	FString Error;
	if (!TestTrue(TEXT("Original people programs load"), FSimCopterPeopleReader::LoadFromFile(FSimCopterPeopleReader::ResolvePeoplePath(Root),*Model,Error))) return false;
	auto Person = [&](int32 State, const FVector& At)
	{
		auto* Agent = World->SpawnActor<ASimCopterGroundAgent>();
		Agent->SetOwner(Traffic);
		Agent->InitialPersonState = State;
		Agent->SetInitialBehaviorClass(State == 5 ? 12 : 0);
		Agent->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian,TEXT(""),Root,85);
		Agent->BehaviorModel = Model;
		Agent->bBehaviorActive = true;
		Agent->BehaviorContext.ResetToState(State);
		Agent->BehaviorContext.Attributes[EBhavAttr::Visible] = 1;
		Agent->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 100;
		Agent->BehaviorContext.Attributes[EBhavAttr::MoveSpeed] = 16;
		Agent->SetActorLocation(At + FVector(0,0,Agent->GetCapsuleHalfHeightCm()+1));
		Traffic->PedestrianAgents.Add(Agent);
		return Agent;
	};
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SetActorLocation(FVector(-1200,0,50));
	const FBox WallBounds(FVector(-100,-450,0), FVector(100,450,400));
	MedicalBox(World, WallBounds.GetCenter(), WallBounds.GetExtent());
	// The exit is behind the medic: greedy facing retries cannot solve this cul-de-sac.
	MedicalBox(World, FVector(-600,-450,200), FVector(500,75,200));
	MedicalBox(World, FVector(-600,450,200), FVector(500,75,200));
	for (const int32 Count : {1,3})
	{
		TArray<ASimCopterGroundAgent*> Medics, Patients;
		for (int32 I=0; I<Count; ++I)
		{
			auto* Medic = Person(5,FVector(-800,I*80,0));
			TestTrue(TEXT("Medic boards before deployment"), Medic->BoardCarrier(Heli,false));
			TestTrue(TEXT("Medic alights from helicopter"), Medic->AlightFromCarrier(false));
			Medic->SetActorLocation(FVector(-700,I*80,Medic->GetCapsuleHalfHeightCm()+1));
			Medic->BehaviorContext.Attributes[EBhavAttr::MoveSpeed] = 16;
			Medics.Add(Medic);
			Patients.Add(Person(6,FVector(700,I*400,0)));
		}
		TSet<AActor*> Selected;
		for (auto* Medic : Medics)
		{
			int32 Distance;
			TestTrue(TEXT("Medic selects a casualty"), Medic->SelectObjectOfClass(Medic->BehaviorContext,EBhavObjectClass::MedevacVictim,Distance));
			Selected.Add(Medic->BehaviorContext.SelectedObject.Get());
		}
		TestEqual(TEXT("Medics reserve different casualties"), Selected.Num(), Count);
		TSet<ASimCopterGroundAgent*> Arrived;
		bool bEnteredWall = false;
		float FurthestBack = -700;
		for (int32 Tick=0; Tick<1200 && Arrived.Num()<Count; ++Tick)
		{
			World->Tick(LEVELTICK_TimeOnly,1.0f/15);
			for (auto* Medic : Medics)
			{
				if (Arrived.Contains(Medic)) continue;
				if (Medic->StepTowardSelectedObject(Medic->BehaviorContext) == ESimCopterBehaviorStepResult::Arrived)
				{
					Arrived.Add(Medic);
					continue;
				}
				Medic->UpdateMovement(1.0f/15);
				bEnteredWall |= WallBounds.IsInside(Medic->GetActorLocation());
				FurthestBack = FMath::Min(FurthestBack, float(Medic->GetActorLocation().X));
			}
		}
		TestEqual(FString::Printf(TEXT("All %d dropped medics route around wall to contact"),Count), Arrived.Num(), Count);
		TestFalse(TEXT("No medic walks through wall"), bEnteredWall);
		TestTrue(TEXT("Route goes backwards out of the cul-de-sac"), FurthestBack < -1100);
		AddInfo(FString::Printf(TEXT("Dropped %d medics; %d unique targets; %d arrivals; detour reaches X=%.1f"),
			Count, Selected.Num(), Arrived.Num(), FurthestBack));
		for (auto* Medic : Medics) Medic->Destroy();
		for (auto* Patient : Patients) Patient->Destroy();
	}
	// Exercise the shipped BHAV 801 -> 262 approach and actual patient pickup as well.
	auto* Controller = World->SpawnActor<ASimCopterPlayerController>();
	World->AddController(Controller);
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->ChangeState(NAME_Playing);
	Controller->Possess(Heli);
	auto* ProgramMedic = Person(5,FVector(-700,0,0));
	ProgramMedic->SetBehaviorStartingVehicle(Heli);
	ProgramMedic->SetPersistentHospitalRoofCrew(true);
	auto* ProgramPatient = Person(6,FVector(700,0,0));
	for (int32 Frame=0; Frame<7200 && !ProgramPatient->GetBehaviorCarrier() && !ProgramMedic->IsActorBeingDestroyed(); ++Frame)
	{
		World->Tick(LEVELTICK_TimeOnly,1.0f/60);
		ProgramMedic->Tick(1.0f/60);
	}
	TestTrue(TEXT("Original medic program navigates the cul-de-sac and picks up the same patient"),
		ProgramPatient->GetBehaviorCarrier() == ProgramMedic || ProgramPatient->GetBehaviorCarrier() == Heli);
	AddInfo(FString::Printf(TEXT("Original-program medic at %s; patient carrier %s"),
		*ProgramMedic->GetActorLocation().ToString(), *GetNameSafe(ProgramPatient->GetBehaviorCarrier())));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterDeployedMedicRiotTest, "SimCopter.MedicalNavigation.DeployedMedicRiot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterDeployedMedicRiotTest::RunTest(const FString& Parameters)
{
	auto* World = CreateMedicalWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	MedicalBox(World, FVector(0,0,-10), FVector(10000,10000,10));
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr, 1);
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame"));
	auto Model = MakeShared<FPeopleBehaviorModel>();
	FString Error;
	if (!TestTrue(TEXT("Original medic and riot programs load"), FSimCopterPeopleReader::LoadFromFile(FSimCopterPeopleReader::ResolvePeoplePath(Root), *Model, Error))) return false;
	auto Person = [&](int32 State, const FVector& At)
	{
		auto* Agent = World->SpawnActor<ASimCopterGroundAgent>();
		Agent->SetOwner(Traffic);
		Agent->InitialPersonState = State;
		Agent->SetInitialBehaviorClass(State == 5 ? 12 : 0);
		Agent->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
		Agent->BehaviorModel = Model;
		Agent->bBehaviorActive = true;
		Agent->BehaviorContext.ResetToState(State);
		Agent->BehaviorContext.Attributes[EBhavAttr::Visible] = 1;
		Agent->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 100;
		Agent->SetActorLocation(At + FVector(0,0,Agent->GetCapsuleHalfHeightCm()+1));
		Traffic->PedestrianAgents.Add(Agent);
		return Agent;
	};
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SetActorLocation(FVector(-700,0,Heli->GetSimpleCollisionHalfHeight()+1));
	Heli->GroundClearanceCm = 1;
	auto* Controller = World->SpawnActor<ASimCopterPlayerController>();
	World->AddController(Controller);
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->ChangeState(NAME_Playing);
	Controller->Possess(Heli);
	// The original BHAV 262 probes an eight-tile square. A Manhattan-nearer casualty just
	// outside that square must not hide an eligible injured rioter on its diagonal.
	auto* OutsideSearch = Person(6, FVector(3500,0,0));
	auto* Patient = Person(3, FVector(1900,2600,0));
	TestTrue(TEXT("An injured rioter becomes a live medical patient"), Patient->CollapseIntoMedevacVictim(Patient->BehaviorContext));
	TestTrue(TEXT("Former rioter is eligible for medical rescue"), Patient->IsMedevacVictim() && !Patient->HasMissionResolutionReported());
	auto* Medic = Person(5, FVector(-900,0,0));
	Medic->SetPersistentHospitalRoofCrew(true);
	TestTrue(TEXT("Medic boards the player's helicopter"), Medic->BoardCarrier(Heli,false));
	TestTrue(TEXT("Player deploys medic through the actual passenger drop action"), Heli->DropPassengerAtSlot(0));
	bool bPickedUp = false;
	for (int32 Frame = 0; Frame < 5400 && Patient->GetBehaviorCarrier() != Heli && !Medic->IsActorBeingDestroyed(); ++Frame)
	{
		World->Tick(LEVELTICK_TimeOnly,1.0f/60);
		Medic->Tick(1.0f/60);
		bPickedUp |= Patient->GetBehaviorCarrier() == Medic;
	}
	AddInfo(FString::Printf(TEXT("Deployed medic %s, state %d, falling %d; patient carrier %s; stack %d/%d"),
		*Medic->GetActorLocation().ToString(), Medic->GetBehaviorAttribute(EBhavAttr::State), Medic->IsPassengerFalling(),
		*GetNameSafe(Patient->GetBehaviorCarrier()), Medic->BehaviorContext.Stack.IsEmpty() ? -1 : Medic->BehaviorContext.Stack.Last().ProgramId,
		Medic->BehaviorContext.Stack.IsEmpty() ? -1 : Medic->BehaviorContext.Stack.Last().RecordIndex));
	TestTrue(TEXT("Manually deployed medic physically retrieves the injured rioter"), bPickedUp);
	TestTrue(TEXT("Manually deployed medic brings the injured rioter to the helicopter"), Patient->GetBehaviorCarrier() == Heli);
	TestNull(TEXT("A casualty outside the initial rescue radius remains unclaimed"), OutsideSearch->HospitalPatientClaim.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterMedicHillTest, "SimCopter.MedicalNavigation.SteepHillReturn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterMedicHillTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame"));
	auto Model = MakeShared<FPeopleBehaviorModel>();
	FString Error;
	if (!TestTrue(TEXT("Original people programs load"), FSimCopterPeopleReader::LoadFromFile(FSimCopterPeopleReader::ResolvePeoplePath(Root), *Model, Error))) return false;
	for (const int32 SeatsFree : {2, 1, 0})
	{
		auto* World = CreateMedicalWorld();
		ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		MedicalBox(World, FVector(-2750,0,-20), FVector(2250,2000,20));
		// A continuous 45-degree hill rises ten metres to a level helicopter clearing.
		auto* Ramp = MedicalBox(World, FVector(14.142136,0,485.857864), FVector(707.106781,2000,20));
		Ramp->SetActorRotation(FRotator(45,0,0));
		MedicalBox(World, FVector(2750,0,980), FVector(2250,2000,20));
		auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
		Traffic->ActiveTileSize = 400;
		Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
		Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
		Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
		Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
		auto Person = [&](int32 State, const FVector& At)
		{
			auto* Agent = World->SpawnActor<ASimCopterGroundAgent>();
			Agent->SetOwner(Traffic);
			Agent->InitialPersonState = State;
			Agent->SetInitialBehaviorClass(State == 5 ? 12 : 0);
			Agent->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
			Agent->BehaviorModel = Model;
			Agent->bBehaviorActive = true;
			Agent->BehaviorContext.ResetToState(State);
			Agent->BehaviorContext.Attributes[EBhavAttr::Visible] = 1;
			Agent->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 100;
			Agent->BehaviorContext.Attributes[EBhavAttr::MoveSpeed] = 18;
			Agent->SetActorLocation(At + FVector(0,0,Agent->GetCapsuleHalfHeightCm()+1));
			Traffic->PedestrianAgents.Add(Agent);
			return Agent;
		};
		auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
		Heli->SetActorLocation(FVector(850,0,1000 + Heli->GetSimpleCollisionHalfHeight() + 1));
		Heli->GroundClearanceCm = 1;
		auto* Controller = World->SpawnActor<ASimCopterPlayerController>();
		World->AddController(Controller);
		Controller->Player = NewObject<ULocalPlayer>(GEngine);
		Controller->ChangeState(NAME_Playing);
		Controller->Possess(Heli);
		auto* Medic = Person(5, FVector(-650,0,0));
		TestTrue(TEXT("Medic initially boards"), Medic->BoardCarrier(Heli,false));
		TestTrue(TEXT("Medic deploys"), Medic->AlightFromCarrier(false));
		Medic->SetActorLocation(FVector(-650,0,Medic->GetCapsuleHalfHeightCm()+1));
		Medic->SetPersistentHospitalRoofCrew(true);
		auto* Patient = Person(6, FVector(-650,0,0));
		TestTrue(TEXT("Medic holds actual patient"), Patient->BoardCarrier(Medic,false,false,true));
		TArray<ASimCopterGroundAgent*> Occupants;
		while (Heli->GetAvailablePassengerSeats() > SeatsFree)
		{
			auto* Occupant = Person(0,FVector(850,0,1000));
			if (!TestTrue(TEXT("Cabin filler boards"),Occupant->BoardCarrier(Heli,false))) return false;
			Occupants.Add(Occupant);
		}
		const FVector Goal(850,0,Medic->GetActorLocation().Z);
		TestTrue(TEXT("Continuous steep uphill is walkable"), Medic->CanWalkMedicSegment(Medic->GetActorLocation(),Goal,Heli));
		TestTrue(TEXT("Continuous steep downhill is walkable"), Medic->CanWalkMedicSegment(
			FVector(850,0,1000+Medic->GetCapsuleHalfHeightCm()+1),Medic->GetActorLocation(),Heli));
		// Require A* to preserve elevation when it detours around an obstacle halfway up.
		auto* Wall = MedicalBox(World,FVector(0,0,700),FVector(60,180,350));
		TestFalse(TEXT("Hill routing still rejects a direct path through a wall"),
			Medic->CanWalkMedicSegment(Medic->GetActorLocation(),Goal,Heli));
		FVector Waypoint;
		bool bHasRoute = false;
		for (int32 Update = 0; Update < 200 && !bHasRoute; ++Update)
		{
			World->Tick(LEVELTICK_TimeOnly,1.0f/15);
			bHasRoute = Medic->GetMedicWaypoint(Goal,Heli,Waypoint);
			if (!bHasRoute && !Medic->MedicPathSearch.IsPending()) break;
		}
		TestTrue(TEXT("Uphill detour has a route after bounded search updates"), bHasRoute);
		if (SeatsFree != 2) Wall->Destroy();
		Medic->MedicPath.Reset();
		Medic->MedicPathSearch.Reset();
		Medic->bMedicPathAttempted = false;
		// Resume BHAV 262 at the post-pickup vehicle selection, as in the real rescue loop.
		Medic->BehaviorContext.Stack.Reset();
		FSimCopterPersonContext::FFrame ReturnFrame;
		ReturnFrame.ProgramId = 262;
		ReturnFrame.RecordIndex = 9;
		Medic->BehaviorContext.Stack.Add(ReturnFrame);
		Medic->BehaviorContext.SelectedObject = Heli;
		Medic->BehaviorContext.SelectedLocation = Heli->GetActorLocation();
		Medic->BehaviorContext.bHasSelection = true;
		const float Delta = SeatsFree == 1 ? 1.0f/15 : 1.0f/60;
		bool bCrossedWall = false;
		for (int32 Frame=0; Frame<FMath::RoundToInt(90/Delta) && !Medic->GetBehaviorCarrier(); ++Frame)
		{
			World->Tick(LEVELTICK_TimeOnly,Delta);
			Medic->Tick(Delta);
			bCrossedWall |= SeatsFree == 2 && FBox(FVector(-60,-180,350),FVector(60,180,1050)).IsInside(Medic->GetActorLocation());
			if (Patient->GetBehaviorCarrier() == Heli && SeatsFree < 2) break;
		}
		TestFalse(TEXT("Returning medic never crosses hill obstacle"),bCrossedWall);
		AddInfo(FString::Printf(TEXT("Seats=%d medic=%s carrier=%s patient=%s"),SeatsFree,
			*Medic->GetActorLocation().ToString(),*GetNameSafe(Medic->GetBehaviorCarrier()),*GetNameSafe(Patient->GetBehaviorCarrier())));
		if (SeatsFree > 0) TestTrue(TEXT("Patient delivered up hill"), Patient->GetBehaviorCarrier() == Heli);
		else
		{
			TestTrue(TEXT("Full helicopter cannot board patient"), Patient->GetBehaviorCarrier() != Heli);
			TestFalse(TEXT("Full-cabin handoff preserves the patient"), Patient->IsActorBeingDestroyed());
		}
		if (SeatsFree == 2) TestTrue(TEXT("Medic boards after patient when seat remains"), Medic->GetBehaviorCarrier() == Heli);
		else TestNull(TEXT("Medic cannot overfill cabin"), Medic->GetBehaviorCarrier());
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterResponderRepathTest, "SimCopter.MedicalNavigation.MovingTargetRepathBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterResponderRepathTest::RunTest(const FString& Parameters)
{
	auto* World = CreateMedicalWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	MedicalBox(World, FVector(0,0,-10), FVector(20000,20000,10));
	auto* Wall = MedicalBox(World, FVector(500,0,200), FVector(50,10000,200));
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
	auto* Responder = World->SpawnActor<ASimCopterGroundAgent>();
	Responder->SetOwner(Traffic);
	Responder->SetActorLocation(FVector(0,0,Responder->GetCapsuleHalfHeightCm()+1));
	auto* Target = World->SpawnActor<ASimCopterHelicopterPawn>();
	FVector Waypoint;
	int32 Searches = 0;
	double WorstMs = 0;
	const double Begin = FPlatformTime::Seconds();
	for (int32 Step = 0; Step < 12; ++Step)
	{
		World->Tick(LEVELTICK_TimeOnly, 1.0f/15);
		Target->SetActorLocation(FVector(1000,Step*100,1000));
		const double PreviousDeadline = Responder->NextMedicPathTime;
		const double Start = FPlatformTime::Seconds();
		TestFalse(TEXT("Responder cannot walk through the wall"), Responder->GetMedicWaypoint(Target->GetActorLocation(), Target, Waypoint));
		WorstMs = FMath::Max(WorstMs, (FPlatformTime::Seconds()-Start)*1000);
		Searches += Responder->NextMedicPathTime != PreviousDeadline;
	}
	AddInfo(FString::Printf(TEXT("Moving blocked aircraft: %d searches in 0.8 simulated seconds; %.3f ms total; %.3f ms worst query"),
		Searches,(FPlatformTime::Seconds()-Begin)*1000,WorstMs));
	TestEqual(TEXT("Moving aircraft must not bypass the one-second search interval"), Searches, 1);
	const double FirstDeadline = Responder->NextMedicPathTime;
	auto* OtherTarget = World->SpawnActor<ASimCopterHelicopterPawn>();
	OtherTarget->SetActorLocation(FVector(-300,0,1000));
	World->Tick(LEVELTICK_TimeOnly, 0.01f);
	Responder->GetMedicWaypoint(OtherTarget->GetActorLocation(), OtherTarget, Waypoint);
	TestEqual(TEXT("Switching targets must not bypass the search interval"), Responder->NextMedicPathTime, FirstDeadline);
	// Keep changing the unreachable goal for six simulated minutes. Completed failures
	// must retire their work and retry without accumulating searches or returning a partial route.
	int32 Restarts = 0;
	bool bCrossedWall = false;
	for (int32 Minute = 0; Minute < 6; ++Minute)
	{
		double TotalMs = 0, PeakMs = 0;
		for (int32 Step = 0; Step < 900; ++Step)
		{
			World->Tick(LEVELTICK_TimeOnly,1.0f/15);
			Target->SetActorLocation(FVector(1000,(Step%13-6)*100,1000));
			const bool bWasPending = Responder->MedicPathSearch.IsPending();
			const double Start = FPlatformTime::Seconds();
			bCrossedWall |= Responder->GetMedicWaypoint(Target->GetActorLocation(),Target,Waypoint);
			const double Ms = (FPlatformTime::Seconds()-Start)*1000;
			TotalMs += Ms; PeakMs = FMath::Max(PeakMs,Ms);
			Restarts += !bWasPending && Responder->MedicPathSearch.IsPending();
		}
		AddInfo(FString::Printf(TEXT("Blocked-flight soak minute %d: %.3f ms mean query, %.3f ms peak"),Minute+1,TotalMs/900,PeakMs));
	}
	TestFalse(TEXT("Repeated searches never return a route through the wall"),bCrossedWall);
	TestTrue(TEXT("Failed searches finish and retry within the fixed interval"),Restarts>1 && Restarts<=360);
	Wall->SetActorEnableCollision(false);
	Wall->Destroy();
	// The production search is sliced and rate-limited. Advance ordinary frames through
	// its retry deadline instead of requiring completion in a single (possibly clamped) tick.
	bool bFoundReplacement = false;
	for (int32 Frame=0; Frame<60 && !bFoundReplacement; ++Frame)
	{
		World->Tick(LEVELTICK_TimeOnly,1.f/15);
		bFoundReplacement = Responder->GetMedicWaypoint(OtherTarget->GetActorLocation(),OtherTarget,Waypoint);
	}
	TestTrue(TEXT("Retry finds a route to the reachable replacement target"),bFoundReplacement);
	TestTrue(TEXT("Retry routes to the current target position"), FVector::Dist2D(Waypoint,OtherTarget->GetActorLocation())<1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSlicedPathTest, "SimCopter.MedicalNavigation.SlicedSearchPreservesRoutes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterSlicedPathTest::RunTest(const FString& Parameters)
{
	for (const bool bReachable : {false, true})
	{
		int32 Probes = 0;
		auto CanWalk = [&](const FVector& A, FVector& B)
		{
			++Probes;
			for (int32 I = 0; I <= 64; ++I)
			{
				const FVector P = FMath::Lerp(A,B,float(I)/64);
				if (P.X >= 150 && P.X <= 300 && (!bReachable || FMath::Abs(P.Y)<200)) return false;
			}
			B.Z = B.X * 0.1; // Endpoints still retain the physical walk-probe height.
			return true;
		};
		const FVector Goal(600,0,0);
		TArray<FVector> Eager, Sliced;
		TestEqual(TEXT("Synchronous route result"), SimCopterBandNavigation::FindPath(FVector::ZeroVector,Goal,CanWalk,Eager), bReachable);
		SimCopterBandNavigation::FPathSearch Search;
		Search.Begin(FVector::ZeroVector,Goal);
		int32 Updates = 0;
		while (Search.IsPending() && Updates++ < 2048)
		{
			Probes = 0;
			Search.Advance(CanWalk,Sliced,4,0);
			TestTrue(TEXT("Each update bounds expensive world probes"), Probes <= 4*9);
			if (Search.IsPending()) TestTrue(TEXT("No incomplete route is used for movement"),Sliced.IsEmpty());
		}
		TestFalse(TEXT("Search finishes without losing its original node limit"), Search.IsPending());
		TestTrue(TEXT("Sliced and synchronous routes match including elevations"), Eager == Sliced);
		TestTrue(TEXT("Difficult search spans multiple updates"), Updates > 1);
		Search.Begin(FVector::ZeroVector,Goal);
		Search.Reset();
		TestFalse(TEXT("Target cancellation discards pending work"),Search.IsPending());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterMedicDeploymentTest,"SimCopter.MedicalNavigation.MedicDeploymentPriorities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterMedicDeploymentTest::RunTest(const FString&)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame"));
	auto Model = MakeShared<FPeopleBehaviorModel>(); FString Error;
	if (!TestTrue(TEXT("Original medical programs load"),FSimCopterPeopleReader::LoadFromFile(FSimCopterPeopleReader::ResolvePeoplePath(Root),*Model,Error))) return false;
	for (int32 Scenario=0; Scenario<7; ++Scenario)
	{
		auto* World = CreateMedicalWorld();
		ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		MedicalBox(World,FVector(0,0,-10),FVector(10000,10000,10));
		auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
		Traffic->ActiveTileSize = 400;
		Traffic->PeopleTileClasses.Init(7,FSimCity2000City::TileCount);
		Traffic->TileCenterWorldZ.Init(0,FSimCity2000City::TileCount);
		Traffic->WaterTileFlags.Init(0,FSimCity2000City::TileCount);
		Traffic->XbldTileIds.Init(0x1d,FSimCity2000City::TileCount);
		auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
		Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
		auto Person = [&](int32 State,const FVector& At)
		{
			auto* Agent = World->SpawnActor<ASimCopterGroundAgent>(); Agent->SetOwner(Traffic);
			Agent->InitialPersonState = State; Agent->SetInitialBehaviorClass(State==5 ? 12 : 0);
			Agent->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian,TEXT(""),Root,85);
			Agent->BehaviorModel = Model; Agent->bBehaviorActive = true; Agent->BehaviorContext.ResetToState(State);
			Agent->BehaviorContext.Attributes[EBhavAttr::Visible] = 1;
			Agent->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 100;
			Agent->SetActorLocation(At+FVector(0,0,Agent->GetCapsuleHalfHeightCm()+1));
			Traffic->PedestrianAgents.Add(Agent); return Agent;
		};
		auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
		TestTrue(TEXT("Actual aircraft loads with its real seat capacity"),Heli->SwitchHelicopterModel(Scenario==5 ? 5 : 4));
		Heli->SetActorLocation(FVector(0,0,Heli->GetSimpleCollisionHalfHeight()+1));
		auto* Controller = World->SpawnActor<ASimCopterPlayerController>(); World->AddController(Controller);
		Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->ChangeState(NAME_Playing); Controller->Possess(Heli);
		TArray<ASimCopterGroundAgent*> Medics, Patients;
		const int32 CrewCount = Scenario==4 || Scenario==6 ? 1 : 2;
		const int32 PatientCount = 1+Scenario%2;
		for (int32 I=0; I<CrewCount; ++I)
		{
			auto* Medic = Person(5,FVector(-200,I*100,0)); Medic->SetPersistentHospitalRoofCrew(true);
			TestTrue(TEXT("Crew boards before deployment"),Medic->BoardCarrier(Heli,false)); Medics.Add(Medic);
		}
		for (int32 I=0; I<PatientCount; ++I)
		{
			auto* Patient = Person(6,FVector(I==1 ? 2500 : 650,I*150,0)); Patients.Add(Patient);
			TestTrue(TEXT("Patient has a real medical mission"),Missions->CreatePlayerCausedMedevacForVictim(Patient,true));
			if (Scenario>=4) TestTrue(TEXT("Patient initially shares cabin with medic"),Patient->BoardCarrier(Heli,false));
		}
		if (Scenario==6)
		{
			Heli->SetActorLocation(Heli->GetActorLocation()+FVector(0,0,5000));
			Heli->GroundClearanceCm = 5000; // Time-only fixture does not run the flight/altimeter tick.
		}
		for (auto* Medic : Medics)
		{
			const int32 Slot = Heli->GetMissionPassengerSlots().IndexOfByPredicate([&](const FSimCopterMissionPassengerSlot& S){return S.Person.Get()==Medic;});
			TestTrue(TEXT("Actual passenger action deploys medic"),Heli->DropPassengerAtSlot(Slot));
			if (Scenario>=2 && Scenario<=5)
			{
				int32 X,Y;
				if (Medic->TryGetTileCoordinate(X,Y)) Traffic->XbldTileIds[Y*FSimCity2000City::MapSize+X] = 0xd1;
			}
		}
		if (Scenario==6)
		{
			TestTrue(TEXT("An airborne crew drop does not pull the patient out of the cabin"),Patients[0]->GetBehaviorCarrier()==Heli);
			continue;
		}
		if (Scenario>=4)
		{
			TSet<AActor*> Carriers;
			for (auto* Patient : Patients)
			{
				TestTrue(TEXT("Disembarking medic takes the onboard patient along"),Medics.Contains(Cast<ASimCopterGroundAgent>(Patient->GetBehaviorCarrier())));
				Carriers.Add(Patient->GetBehaviorCarrier());
			}
			TestEqual(TEXT("Each deployed medic takes a different patient"),Carriers.Num(),PatientCount);
		}
		TSet<ASimCopterGroundAgent*> Retrieved, Loaded;
		for (int32 Frame=0; Frame<5400; ++Frame)
		{
			World->Tick(LEVELTICK_TimeOnly,1.f/60);
			for (auto* Medic : Medics) Medic->Tick(1.f/60);
			for (auto* Patient : Patients)
			{
				if (Medics.Contains(Cast<ASimCopterGroundAgent>(Patient->GetBehaviorCarrier()))) Retrieved.Add(Patient);
				if (Patient->GetBehaviorCarrier()==Heli) Loaded.Add(Patient);
				Patient->Tick(1.f/60);
			}
			if (Scenario<4 && Loaded.Num()==PatientCount) break;
			if (Scenario>=4 && Frame>1200) break;
		}
		if (Scenario<4)
		{
			TestEqual(FString::Printf(TEXT("Scenario %d: both crew prioritize physical rescue"),Scenario),Retrieved.Num(),PatientCount);
			TestEqual(FString::Printf(TEXT("Scenario %d: patients reach cabin before idle crew fill it"),Scenario),Loaded.Num(),PatientCount);
		}
		else
			for (auto* Patient : Patients)
			{
				TestNull(TEXT("Medic finishes carrying the patient away and sets them down"),Patient->GetBehaviorCarrier());
				TestTrue(TEXT("Manual handoff records delivery for the same patient"),Patient->HasMissionResolutionReported());
				TestFalse(TEXT("Delivery cannot score twice"),Missions->NotifyMissionPersonDelivered(Patient));
			}
		for (auto* Medic : Medics) AddInfo(FString::Printf(TEXT("Scenario %d medic carrier=%s stack=%d/%d"),Scenario,
			*GetNameSafe(Medic->GetBehaviorCarrier()),Medic->BehaviorContext.Stack.IsEmpty()?-1:Medic->BehaviorContext.Stack.Last().ProgramId,
			Medic->BehaviorContext.Stack.IsEmpty()?-1:Medic->BehaviorContext.Stack.Last().RecordIndex));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterMedicReboardingTest,"SimCopter.MedicalNavigation.MedicReboardingDelay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterMedicReboardingTest::RunTest(const FString&)
{
	for (int32 Scenario = 0; Scenario < 6; ++Scenario)
	{
		auto* World = CreateMedicalWorld();
		ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		MedicalBox(World, FVector(0,0,-10), FVector(10000,10000,10));
		auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
		Traffic->ActiveTileSize = 400;
		Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
		Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
		auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
		TestTrue(TEXT("Two-seat starter loads"), Heli->SwitchHelicopterModel(4));
		Heli->SetActorLocation(FVector(0,0,Heli->GetSimpleCollisionHalfHeight()+1));
		const auto Person = [&](int32 State)
		{
			auto* Agent = World->SpawnActor<ASimCopterGroundAgent>(); Agent->SetOwner(Traffic);
			Agent->InitialPersonState = State; Agent->BehaviorContext.ResetToState(State);
			Agent->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 100;
			Agent->SetActorLocation(FVector(500,0,100)); Traffic->PedestrianAgents.Add(Agent);
			return Agent;
		};
		auto* Medic = Person(5);
		TestTrue(TEXT("Medic initially boards"), Medic->BoardCarrier(Heli, false));
		TestTrue(TEXT("Passenger action deploys the real medic"), Heli->DropPassengerAtSlot(0));
		TestFalse(TEXT("Empty-handed medic cannot immediately undo deployment"), Medic->BoardCarrier(Heli, false));
		TestFalse(TEXT("Harness cannot bypass deployment delay"), Medic->BoardCarrier(Heli, true));
		if (Scenario == 0)
		{
			for (int32 Frame = 0; Frame < 149; ++Frame) World->Tick(LEVELTICK_TimeOnly, .1f);
			TestFalse(TEXT("Empty-handed medic still waits before fifteen seconds"), Medic->BoardCarrier(Heli, false));
			World->Tick(LEVELTICK_TimeOnly, .2f);
			AddInfo(FString::Printf(TEXT("Medic return time %.2f, deadline %.2f"), World->GetTimeSeconds(), Medic->CrewReboardAfterTime));
			TestTrue(TEXT("Empty-handed medic can return after fifteen seconds"), Medic->BoardCarrier(Heli, false));
			TestTrue(TEXT("Medic can be deployed again"), Heli->DropPassengerAtSlot(0));
			TestFalse(TEXT("A second drop restarts the delay"), Medic->BoardCarrier(Heli, false));
			continue;
		}
		auto* Patient = Person(6);
		TestTrue(TEXT("Medic carries patient"), Patient->BoardCarrier(Medic, false, false, true));
		if (Scenario == 1)
		{
			TestFalse(TEXT("Even the final patient cannot shorten the fifteen-second crew delay"), Medic->BoardCarrier(Heli, false));
			TestTrue(TEXT("Patient can board during the crew delay"), Patient->BoardCarrier(Heli, false));
			for(int32 Frame=0;Frame<151;++Frame) World->Tick(LEVELTICK_TimeOnly,.1f);
			TestTrue(TEXT("Medic can follow after the full delay"), Medic->BoardCarrier(Heli, false));
			TestTrue(TEXT("Patient and medic occupy their own seats"), Patient->GetBehaviorCarrier() == Heli && Heli->GetAvailablePassengerSeats() == 0);
		}
		else if (Scenario == 2)
		{
			ISimCopterBehaviorWorld& Actions = *Medic;
			FSimCopterPersonContext Context; Context.SelectedObject = Heli; Context.bHasSelection = true;
			TestTrue(TEXT("Actual opcode hands patient to cabin first"), Actions.SelectCarriedPerson(Context, false));
			Context.SelectedObject = Heli;
			TestFalse(TEXT("Completed handoff cannot bypass the crew delay"), Actions.BoardSelection(Context));
			for(int32 Frame=0;Frame<151;++Frame) World->Tick(LEVELTICK_TimeOnly,.1f);
			TestTrue(TEXT("Actual boarding opcode succeeds after the delay"), Actions.BoardSelection(Context));
		}
		else if (Scenario == 3)
		{
			auto* Other = Person(6);
			TestFalse(TEXT("Another waiting patient blocks early medic boarding"), Medic->BoardCarrier(Heli, false));
			TestTrue(TEXT("Refused return keeps the patient carried"), Patient->GetBehaviorCarrier() == Medic);
			TestTrue(TEXT("Patients can still load during the medic cooldown"), Patient->BoardCarrier(Heli, false));
			TestFalse(TEXT("Loading own patient does not bypass the other patient's reservation"), Medic->BoardCarrier(Heli, false));
			TestTrue(TEXT("Other patient can use the remaining seat"), Other->BoardCarrier(Heli, false));
			TestFalse(TEXT("No early return into a full cabin"), Medic->BoardCarrier(Heli, false));
		}
		else if (Scenario == 4)
		{
			Heli->AddMissionPassengersForMission(1, INDEX_NONE, ESimCopterMissionPassengerKind::Transport);
			TestFalse(TEXT("A carried patient requires space for both patient and medic"), Medic->BoardCarrier(Heli, false));
			TestTrue(TEXT("Lack of room does not drop the patient"), Patient->GetBehaviorCarrier() == Medic);
		}
		else
		{
			Patient->BoardCarrier(Heli, false);
			auto* Idle = Person(5); Idle->PrepareEmergencyCrewDeployment(Heli, false);
			TestFalse(TEXT("Each medic has an independent deployment cooldown"), Idle->BoardCarrier(Heli, false));
		}
	}
	for(int32 State:{7,8,0xe})
	{
		auto* World=CreateMedicalWorld();
		ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
		MedicalBox(World,FVector(0,0,-10),FVector(10000,10000,10));
		auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
		auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
		TestTrue(TEXT("Police test aircraft loads"),Heli->SwitchHelicopterModel(4));
		Heli->SetActorLocation(FVector(0,0,Heli->GetSimpleCollisionHalfHeight()+1));
		auto* Officer=World->SpawnActor<ASimCopterGroundAgent>(); Officer->SetOwner(Traffic);
		Officer->InitialPersonState=State; Officer->BehaviorContext.ResetToState(State);
		Officer->SetActorLocation(FVector(500,0,100));
		TestTrue(TEXT("Officer initially boards"),Officer->BoardCarrier(Heli,false));
		TestTrue(TEXT("Passenger action ejects officer"),Heli->DropPassengerAtSlot(0));
		TestFalse(TEXT("Ejected officer cannot immediately reboard"),Officer->BoardCarrier(Heli,false));
		TestFalse(TEXT("Harness cannot bypass police delay"),Officer->BoardCarrier(Heli,true));
		for(int32 Frame=0;Frame<149;++Frame) World->Tick(LEVELTICK_TimeOnly,.1f);
		TestFalse(TEXT("Officer still waits at 14.9 seconds"),Officer->BoardCarrier(Heli,false,true));
		World->Tick(LEVELTICK_TimeOnly,.2f);
		TestTrue(TEXT("Officer can reboard after fifteen seconds"),Officer->BoardCarrier(Heli,false));
		TestTrue(TEXT("Officer can be dropped again"),Heli->DropPassengerAtSlot(0));
		TestFalse(TEXT("Repeated police drop restarts cooldown"),Officer->BoardCarrier(Heli,false));
	}
	return true;
}
#endif
