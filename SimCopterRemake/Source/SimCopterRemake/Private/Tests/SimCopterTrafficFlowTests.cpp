#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterCriminalCar.h"
#include "Formats/SimCopterOriginalGamePaths.h"
#include "Misc/Paths.h"

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSimCopterTrafficFlowTest, "SimCopter.Traffic.Flow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FSimCopterTrafficFlowTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (const TCHAR* Name : {TEXT("Following"), TEXT("Recovery"), TEXT("Routing"), TEXT("Intersection"),
		TEXT("BurglarSpeed"), TEXT("BurglarRecovery"), TEXT("BurglarSpawn"), TEXT("KeithlyBurglarRoute")})
	{
		Names.Add(Name); Commands.Add(Name);
	}
}

bool FSimCopterTrafficFlowTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	const auto Car = [&](FVector Position, FVector Target)
	{
		auto* Agent = World->SpawnActor<ASimCopterGroundAgent>();
		Agent->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle, TEXT(""), TEXT(""), 300);
		Agent->SetOwner(Traffic); Agent->SetActorLocation(Position); Agent->bSnapToGround = false;
		Agent->SetMoveTarget(Target); Agent->SetActorRotation((Target - Position).Rotation());
		Traffic->VehicleAgents.Add(Agent);
		return Agent;
	};
	if (Parameters == TEXT("Following"))
	{
		auto* A = Car(FVector(0,0,100), FVector(2000,0,100));
		auto* B = Car(FVector(60,0,100), FVector(-2000,0,100));
		Traffic->SyncVehicleTrafficStates(.1f);
		for (const bool Normal : {false, true})
		{
			A->SetTrafficSpeedScale(1);
			B->SetMoveTarget(FVector(-2000,0,100));
			Traffic->ApplyVehicleFollowing(600,82,305,Normal,.1f);
			TestEqual(TEXT("Oncoming cars do not stop each other"), A->TrafficSpeedScale, 1.0f);
			B->SetMoveTarget(FVector(60,2000,100));
			A->SetTrafficSpeedScale(1);
			Traffic->ApplyVehicleFollowing(600,82,305,Normal,.1f);
			TestEqual(TEXT("Cross traffic is not a same-lane leader, even with stale mesh yaw"), A->TrafficSpeedScale, 1.0f);
			B->SetMoveTarget(FVector(2000,0,100));
			A->SetTrafficSpeedScale(1);
			Traffic->ApplyVehicleFollowing(600,82,305,Normal,.1f);
			TestEqual(TEXT("Close same-lane follower stops without creep"), A->TrafficSpeedScale, 0.0f);
			B->SetActorLocation(FVector(60,0,600)); B->SetMoveTarget(FVector(2000,0,600));
			A->SetTrafficSpeedScale(1);
			Traffic->ApplyVehicleFollowing(600,82,305,Normal,.1f);
			TestEqual(TEXT("Bridge traffic does not stop lower road"), A->TrafficSpeedScale, 1.0f);
			const FVector Before = A->GetActorLocation();
			Traffic->ResolveVehicleOverlaps();
			TestTrue(TEXT("Separate decks never push each other"), A->GetActorLocation().Equals(Before));
			B->SetActorLocation(FVector(60,0,100));
		}
	}
	else if (Parameters == TEXT("Recovery"))
	{
		auto* A = Car(FVector(0,0,100), FVector(2000,0,100));
		auto* B = Car(FVector(60,0,100), FVector(-2000,0,100));
		Traffic->SyncVehicleTrafficStates(.05f);
		// Oscillation exceeds the old per-frame speed threshold but makes no real progress.
		for (int32 Frame = 0; Frame < 100; ++Frame)
		{
			A->SetActorLocation(FVector(Frame % 2 ? 15 : -15,0,100));
			Traffic->UpdateTrafficInteractions(.05f);
		}
		auto& State = Traffic->VehicleTrafficStates.FindChecked(TObjectKey<ASimCopterGroundAgent>(A));
		TestTrue(TEXT("Default AI recovers from jitter without frame-speed false negatives"), State.CarPhaseSeconds > 0);
		TestTrue(TEXT("Recovery retains the road target"), A->GetMoveTargetLocation().Equals(FVector(2000,0,100)));
		A->SetActorLocation(B->GetActorLocation());
		const FVector Before = A->GetActorLocation();
		Traffic->ResolveVehicleOverlaps();
		TestTrue(TEXT("Phasing pair gets no separation shove"), A->GetActorLocation().Equals(Before));
		Traffic->SyncVehicleTrafficStates(4.1f);
		TestEqual(TEXT("Phasing expires"), State.CarPhaseSeconds, 0.0f);
		TestTrue(TEXT("Cooldown remains after phase ends"), State.RecoveryCooldownSeconds > 0);
		for (int32 Hold = 0; Hold < 6; ++Hold)
		{
			State.CarPhaseSeconds = 2; State.NoProgressSeconds = 10; State.RecoveryCooldownSeconds = 0;
			State.bMissionJammed = Hold == 0; State.bJamQueued = Hold == 1;
			State.bInTrafficLightLine = Hold == 2; State.bMissionOnFire = Hold == 3;
			A->SetAircraftTrafficBlocked(Hold == 4);
			if (Hold == 5) A->DisableVehicleWithTaser();
			Traffic->UpdateVehicleBlockageRecovery();
			TestEqual(TEXT("Intentional holds cancel and prevent recovery"), State.CarPhaseSeconds, 0.0f);
		}
		// The other car must not phase through an intentionally stopped vehicle either.
		auto& BState = Traffic->VehicleTrafficStates.FindChecked(TObjectKey<ASimCopterGroundAgent>(B));
		BState.CarPhaseSeconds = 2;
		B->SetActorLocation(FVector(-60,0,100)); B->SetMoveTarget(FVector(2000,0,100));
		A->SetActorLocation(FVector(0,0,100)); B->SetTrafficSpeedScale(1);
		Traffic->ApplyVehicleFollowing(600,82,305,false,.1f);
		TestEqual(TEXT("Phasing cannot bypass a tasered leader"), B->TrafficSpeedScale, 0.0f);
	}
	else if (Parameters == TEXT("Routing"))
	{
		Traffic->RoadNodes.SetNum(8);
		const FVector Points[] = {{0,0,0},{-400,0,0},{400,0,0},{0,400,0},{0,-400,0},{800,0,0},{0,800,0},{0,-800,0}};
		for (int32 Index = 0; Index < 8; ++Index)
		{
			auto& Node = Traffic->RoadNodes[Index]; Node.Location = Node.LocalLocation = Points[Index]; Node.BuildingId = 0x1d;
		}
		Traffic->RoadNodes[0].Neighbors = {1,2,3,4};
		Traffic->RoadNodes[1].Neighbors = {0};
		Traffic->RoadNodes[2].Neighbors = {0,5}; Traffic->RoadNodes[5].Neighbors = {2};
		Traffic->RoadNodes[3].Neighbors = {0,6}; Traffic->RoadNodes[6].Neighbors = {3};
		Traffic->RoadNodes[4].Neighbors = {0,7}; Traffic->RoadNodes[7].Neighbors = {4};
		auto* A = Car(FVector(0,0,100), FVector(400,0,100));
		auto* B = Car(FVector(200,0,100), FVector(400,0,100)); B->SetRouteState(2,0,5);
		Traffic->SyncVehicleTrafficStates(.1f);
		for (int32 Attempt = 0; Attempt < 30; ++Attempt)
		{
			A->SetRouteState(0,1,2); Traffic->AssignNextTarget(*A,Traffic->RoadNodes);
			TestTrue(TEXT("A crowded planned exit is replaced by a clear connected road"), A->GetRouteTargetNode()==3 || A->GetRouteTargetNode()==4);
		}
		B->SetRouteState(0,2,1); A->SetRouteState(0,1,2);
		Traffic->AssignNextTarget(*A,Traffic->RoadNodes);
		TestEqual(TEXT("Oncoming traffic does not penalize an exit"), A->GetRouteTargetNode(),2);
		A->SetRouteState(5,2,INDEX_NONE); Traffic->AssignNextTarget(*A,Traffic->RoadNodes);
		TestEqual(TEXT("Dead end still permits the necessary U-turn"), A->GetRouteTargetNode(),2);
	}
	else if (Parameters == TEXT("BurglarSpeed"))
	{
		for (const bool Speeder : {false, true})
		{
			auto* Suspect = Car(FVector(0,0,100), FVector(2000,0,100));
			if (Speeder) Suspect->MakeSpeeder();
			else { Suspect->MakeCriminalCar(17, 100 << 16); Traffic->CriminalCars.Add(Suspect); }
			// Inside the stopping band but outside body-separation padding.
			auto* Leader = Car(FVector(92,0,100), FVector(2000,0,100));
			Leader->DisableVehicleWithTaser();
			// Same order as the live coordinator: traffic brakes, then the crime state machine.
			Traffic->UpdateTrafficInteractions(.1f);
			Traffic->UpdateSpeeders(.1f); Traffic->UpdateCriminalCars(.1f);
			TestEqual(TEXT("Criminal speed cannot erase the stopped traffic ahead"), Suspect->TrafficSpeedScale, 0.0f);
			Suspect->Tick(.1f);
			TestTrue(TEXT("Burglar/speeder stays behind a blocked leader"), Suspect->GetActorLocation().Equals(FVector(0,0,100)));
			Traffic->VehicleAgents.Remove(Leader); Leader->Destroy();
			Traffic->UpdateTrafficInteractions(.1f);
			Traffic->UpdateSpeeders(.1f); Traffic->UpdateCriminalCars(.1f);
			TestEqual(TEXT("Open road retains the original fleeing speed"), Suspect->TrafficSpeedScale, 1.75f);
			Traffic->VehicleAgents.Remove(Suspect); Traffic->CriminalCars.Remove(Suspect); Suspect->Destroy();
		}
	}
	else if (Parameters == TEXT("BurglarRecovery"))
	{
		auto* Suspect = Car(FVector(0,0,100), FVector(2000,0,100));
		Suspect->MakeCriminalCar(17, 100 << 16); Traffic->CriminalCars.Add(Suspect);
		auto* Other = Car(FVector(60,0,100), FVector(-2000,0,100));
		Traffic->SyncVehicleTrafficStates(.1f);
		auto& State = Traffic->VehicleTrafficStates.FindChecked(TObjectKey<ASimCopterGroundAgent>(Suspect));
		State.NoProgressSeconds = 20;
		Traffic->UpdateVehicleBlockageRecovery();
		TestTrue(TEXT("Driving burglar can escape a car deadlock without losing the mission"), State.CarPhaseSeconds > 0);
		for (int32 Frame=0; Frame<300; ++Frame)
		{
			Traffic->UpdateTrafficInteractions(.05f); Traffic->UpdateCriminalCars(.05f);
			Suspect->Tick(.05f); Other->Tick(.05f);
		}
		TestTrue(TEXT("Burglar physically clears the overlapping traffic"), Suspect->GetActorLocation().X > 500);
		TestTrue(TEXT("Other traffic clears the burglar"), Other->GetActorLocation().X < -500);
		TestEqual(TEXT("Recovery preserves criminal event identity"), Suspect->GetCriminalEventId(), 17);
		Suspect->SetCriminalState(uint8(SimCopterCriminalCar::EState::BurglarOutside));
		TestFalse(TEXT("Parked getaway car cannot phase while its burglar is outside"), Traffic->CanVehicleRecoverFromTraffic(*Suspect));
		Suspect->SetCriminalState(uint8(SimCopterCriminalCar::EState::SeekingBurglaryStop));
		Suspect->TryOrderStop(INDEX_NONE);
		TestFalse(TEXT("Ordered burglary/police stop remains protected"), Traffic->CanVehicleRecoverFromTraffic(*Suspect));
		Suspect->ResumeCriminalCarDriving(); Suspect->SetCriminalState(uint8(SimCopterCriminalCar::EState::Cruising));
		Suspect->DisableVehicleWithTaser();
		TestFalse(TEXT("Taser still prevents recovery"), Traffic->CanVehicleRecoverFromTraffic(*Suspect));
	}
	else if (Parameters == TEXT("BurglarSpawn"))
	{
		Traffic->XbldTileIds.Init(0,128*128); Traffic->RoadNodes.SetNum(3);
		const FIntPoint Tiles[] = {{64,64},{65,64},{65,65}};
		for (int32 Index=0; Index<3; ++Index)
		{
			auto& Node=Traffic->RoadNodes[Index]; Node.FileX=Tiles[Index].X; Node.FileY=Tiles[Index].Y;
			Node.BuildingId=0x1d; Node.Location=Node.LocalLocation=FVector((Node.FileX-63.5f)*400,-(Node.FileY-63.5f)*400,0);
			Traffic->RoadNodeIndexByTile.Add(Tiles[Index],Index); Traffic->XbldTileIds[Node.FileY*128+Node.FileX]=0x1d;
		}
		Traffic->RoadNodes[0].Neighbors={1}; Traffic->RoadNodes[1].Neighbors={0,2}; Traffic->RoadNodes[2].Neighbors={1};
		if (!TestTrue(TEXT("Clear connected road accepts the burglar"),Traffic->TryActivateBurglarCar(17,64,64,100<<16))) return false;
		auto* Suspect=Traffic->CriminalCars.Last().Get();
		TestTrue(TEXT("Spawn starts with a real first-hop target instead of skipping the corner"),Suspect->HasMoveTarget());
		TestFalse(TEXT("Occupied spawn cannot stack a second burglar on the first"),Traffic->TryActivateBurglarCar(18,64,64,100<<16));
		TestEqual(TEXT("Rejected spawn consumes no criminal pool slot"),Traffic->CriminalCars.Num(),1);
	}
	else if (Parameters == TEXT("KeithlyBurglarRoute"))
	{
		Traffic->CityFile.FilePath = FPaths::Combine(SimCopterOriginalGame::ResolveRoot(), TEXT("cities/career/city7.sc2"));
		if (!TestTrue(TEXT("Actual Keithly road graph loads"), Traffic->RebuildSpawnData())) return false;
		// The user's live log repeatedly places the burglary stop at this road tile.
		if (!TestTrue(TEXT("Reported road supports the getaway car"), Traffic->TryActivateBurglarCar(17,15,79,1000<<16))) return false;
		auto* Suspect = Traffic->CriminalCars.Last().Get();
		for (int32 Cycle=0; Cycle<3; ++Cycle)
		{
			Suspect->SetCriminalState(uint8(SimCopterCriminalCar::EState::SeekingBurglaryStop));
			Suspect->TryOrderStop(INDEX_NONE); Suspect->MarkStopped();
			Suspect->SetCriminalState(uint8(SimCopterCriminalCar::EState::BurglarOutside));
			Suspect->SetBurglarOutsideSeconds(120); Suspect->SignalCriminalDriverReturned(1);
			for (int32 Phase=0; Phase<3; ++Phase) Traffic->RunBurglarOutsidePhase(*Suspect,.1f);
			TestFalse(TEXT("Returning driver releases the deliberate stop"), Suspect->IsStopped() || Suspect->IsStopOrdered());
			const FVector Start = Suspect->GetActorLocation();
			float Furthest = 0;
			for (int32 Frame=0; Frame<400; ++Frame)
			{
				if (!Suspect->HasMoveTarget() || Suspect->IsNearMoveTarget()) Traffic->AssignNextTarget(*Suspect,Traffic->RoadNodes);
				Traffic->UpdateTrafficInteractions(.05f); Traffic->UpdateCriminalCars(.05f); Suspect->Tick(.05f);
				Furthest=FMath::Max(Furthest,float(FVector::Dist2D(Start,Suspect->GetActorLocation())));
			}
			TestTrue(TEXT("Each returned burglar drives away through Keithly's real turns"), Furthest>800);
		}
	}
	else
	{
		// Four competing approaches, with initially overlapping bodies: exercise actual car
		// ticks and every traffic pass at multiple frame rates, rather than only timer fields.
		for (const float Step : {.025f,.1f})
		{
			for (const auto& Ptr : Traffic->VehicleAgents) if (Ptr.IsValid()) Ptr->Destroy();
			Traffic->VehicleAgents.Reset(); Traffic->VehicleTrafficStates.Reset();
			TArray<ASimCopterGroundAgent*> Cars;
			for (int32 Index=0; Index<4; ++Index)
			{
				const FVector Direction = FVector::ForwardVector.RotateAngleAxis(Index*90,FVector::UpVector);
				Cars.Add(Car(-Direction*30+FVector(0,0,100), Direction*2500+FVector(0,0,100)));
			}
			for (int32 Frame=0; Frame<FMath::RoundToInt(15/Step); ++Frame)
			{
				Traffic->UpdateTrafficInteractions(Step);
				for (auto* Agent : Cars) Agent->Tick(Step);
			}
			for (auto* Agent : Cars)
				TestTrue(TEXT("All crossing cars clear the intersection within 15 seconds"), Agent->GetActorLocation().Size2D()>500);
		}
	}
	return true;
}
#endif
