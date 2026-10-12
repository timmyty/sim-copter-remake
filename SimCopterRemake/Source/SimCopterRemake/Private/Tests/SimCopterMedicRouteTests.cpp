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
		TestTrue(TEXT("Uphill detour has a route"), Medic->GetMedicWaypoint(Goal,Heli,Waypoint));
		if (SeatsFree != 2) Wall->Destroy();
		Medic->MedicPath.Reset();
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
#endif
