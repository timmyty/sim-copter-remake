#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Formats/SimCity2000Reader.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterAmbientDispatchTest, "SimCopter.Dispatch.AmbientAvailability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterAmbientDispatchTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterDispatch;
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0, FSimCity2000City::TileCount);
	Traffic->ActiveOriginalGameRootPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../Reference/SimCopterOriginalGame"));
	const TCHAR* Models[] = { TEXT("CARFIRET"), TEXT("CARPOLIC"), TEXT("CARAMBUL") };
	Traffic->VehicleMeshNames = { TEXT("AUTO"), TEXT("AUTO2"), Models[0], Models[1], Models[2] };
	Traffic->RandomStream.Initialize(1937);
	int32 EmergencyDraws = 0;
	for (int32 I = 0; I < 10000; ++I)
		if (Traffic->ChooseAmbientVehicleMesh().StartsWith(TEXT("CAR"))) ++EmergencyDraws;
	AddInfo(FString::Printf(TEXT("Emergency ambient model draws: %d / 10000"), EmergencyDraws));
	TestTrue(TEXT("Emergency traffic is occasional, at most three percent of ambient choices"), EmergencyDraws > 0 && EmergencyDraws <= 300);
	for (int32 X = 20; X <= 40; ++X)
	{
		FSimCopterGroundRouteNode Node;
		Node.FileX = X; Node.FileY = 64;
		Traffic->TryGetTileCenterWorldLocation(X,64,Node.Location);
		if (X > 20) Node.Neighbors.Add(X - 21);
		if (X < 40) Node.Neighbors.Add(X - 19);
		Traffic->RoadNodeIndexByTile.Add(FIntPoint(X,64),Traffic->RoadNodes.Add(Node));
		Traffic->XbldTileIds[64 * 128 + X] = 0x1d;
	}
	auto Ambient = [&](int32 Service, int32 X)
	{
		auto* Agent = World->SpawnActor<ASimCopterGroundAgent>();
		Agent->SetOwner(Traffic);
		Agent->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle, Models[Service], Traffic->ActiveOriginalGameRootPath, 259);
		Agent->SetActorLocation(Traffic->RoadNodes[X-20].Location);
		Agent->SetRouteState(X-20,INDEX_NONE);
		Traffic->VehicleAgents.Add(Agent);
		return Agent;
	};
	for (int32 Service = 0; Service < 3; ++Service)
	{
		Traffic->DispatchVehicles[Service].SetNum(VehiclesPerService);
		FStation Station;
		Station.Service = static_cast<EService>(Service);
		Station.Tile = Station.RoadTile = FIntPoint(20,64);
		Traffic->DispatchStations[Service] = { Station };
		auto* Far = Ambient(Service,25);
		auto* Near = Ambient(Service,38);
		TestEqual(TEXT("Dispatch succeeds with nearby ambient responder"),
			Traffic->RequestEmergencyDispatch(Station.Service,FIntPoint(40,64),false), EDispatchResult::Dispatched);
		TestTrue(TEXT("Closest ambient responder becomes active"), Traffic->IsDispatchVehicle(*Near));
		TestFalse(TEXT("Farther ambient responder remains unassigned"), Traffic->IsDispatchVehicle(*Far));
		TestFalse(TEXT("Activated responder leaves ambient routing and despawn ownership"), Traffic->VehicleAgents.Contains(Near));
		TestEqual(TEXT("Reusing an ambient responder does not launch an HQ vehicle"), Traffic->DispatchStations[Service][0].Outstanding, 0);
		TestEqual(TEXT("A later request closer to HQ succeeds"),
			Traffic->RequestEmergencyDispatch(Station.Service,FIntPoint(21,64),false), EDispatchResult::Dispatched);
		TestEqual(TEXT("HQ launches when its road journey is shorter"), Traffic->DispatchStations[Service][0].Outstanding, 1);
		TestFalse(TEXT("HQ dispatch does not steal the other ambient responder"), Traffic->IsDispatchVehicle(*Far));
		TestTrue(TEXT("First active responder retains its original destination"), Traffic->DispatchVehicles[Service].ContainsByPredicate(
			[Near](const auto& Slot) { return Slot.Agent.Get() == Near && Slot.DestinationTile == FIntPoint(40,64); }));
		// An existing ambient unit of this service caps further decorative copies.
		bool bDuplicate = false;
		for (int32 Draw = 0; Draw < 200; ++Draw)
			bDuplicate |= Traffic->ChooseAmbientVehicleMesh() == Models[Service];
		TestFalse(TEXT("Ambient fleet never duplicates an already circulating service"), bDuplicate);
		Far->MissionEventId = 900 + Service;
		TestEqual(TEXT("Busy responders and mission-owned ambient cars are not reassigned"),
			Traffic->RequestEmergencyDispatch(Station.Service,FIntPoint(40,64),false), EDispatchResult::NoUnitAvailable);
		Far->MissionEventId = INDEX_NONE;
		Far->SetVehicleStalled();
		TestEqual(TEXT("An immobilized ambient responder cannot be dispatched"),
			Traffic->RequestEmergencyDispatch(Station.Service,FIntPoint(40,64),false), EDispatchResult::NoUnitAvailable);
		Far->Destroy();
	}
	TArray<uint8> Saved;
	TestTrue(TEXT("Promoted responders save with the existing traffic layout"), Traffic->CaptureRuntimeSaveState(Saved));
	TestTrue(TEXT("Promoted responders restore with their assignments"), Traffic->RestoreRuntimeSaveState(Saved, nullptr));
	for (int32 Service = 0; Service < 3; ++Service)
	{
		int32 AmbientResponses = 0;
		for (const auto& Slot : Traffic->DispatchVehicles[Service])
			if (Slot.Agent.IsValid() && Slot.StationIndex == INDEX_NONE && Slot.State == ESimCopterDispatchVehicleState::Responding)
			{
				++AmbientResponses;
				TestFalse(TEXT("Restored dispatch stays out of ambient routing"), Traffic->VehicleAgents.Contains(Slot.Agent));
				TestEqual(TEXT("Restored responder retains its target"), Slot.DestinationTile, FIntPoint(40,64));
			}
		TestEqual(TEXT("One promoted responder per service survives load"), AmbientResponses, 1);
	}
	return true;
}
#endif
