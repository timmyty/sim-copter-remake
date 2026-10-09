#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Missions/SimCopterMissionSystemActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterAircraftRoadUpdateTest, "SimCopter.AircraftRoadUpdate.AllCitiesAndAircraft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterAircraftRoadUpdateTest::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame"));
	const FString Output = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Docs/scratchpad/aircraft-road-update"));
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Controller = World->SpawnActor<ASimCopterPlayerController>();
	World->AddController(Controller); Controller->Player = NewObject<ULocalPlayer>(GEngine); Controller->ChangeState(NAME_Playing);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>(); Controller->Possess(Heli);
	for (int32 Type = 0; Type <= SimCopterHelicopterRegistry::PlaneTypeIndex; ++Type)
	{
		Heli->RemoveMissionPassengers(100);
		TestTrue(FString::Printf(TEXT("Model %d loads"), Type), Heli->SwitchHelicopterModel(Type));
		Heli->RefreshCabinOccupants();
		const auto* Cabin = Heli->CabinOccupantsMesh->GetProcMeshSection(0);
		const int32 PilotVertices = Cabin ? Cabin->ProcVertexBuffer.Num() : 0;
		TestTrue(TEXT("Pilot has physical body"), PilotVertices > 0);
		TestNotNull(TEXT("Model has transparent cabin glazing"), Heli->HeliBodyMeshComponent->GetProcMeshSection(1));
		const int32 Capacity = Heli->GetPassengerSeatCount();
		TestEqual(TEXT("Every passenger seat can be occupied"), Heli->AddMissionPassengers(Capacity), Capacity);
		TestEqual(TEXT("Cannot overfill aircraft"), Heli->AddMissionPassengers(1), 0);
		Heli->RefreshCabinOccupants(); Cabin = Heli->CabinOccupantsMesh->GetProcMeshSection(0);
		TestEqual(TEXT("All boarded passengers have physical bodies"), Cabin ? Cabin->ProcVertexBuffer.Num() : 0, PilotVertices*(Capacity+1));
		FString Obj; int32 Base=1;
		auto Export = [&](const TCHAR* Name, const FProcMeshSection* Part)
		{
			if (!Part) return;
			Obj += FString::Printf(TEXT("g %s\n"), Name);
			for (const auto& V : Part->ProcVertexBuffer) Obj += FString::Printf(TEXT("v %f %f %f %f %f %f\n"),
				V.Position.X,V.Position.Y,V.Position.Z,V.Color.R/255.f,V.Color.G/255.f,V.Color.B/255.f);
			for (int32 I=0; I<Part->ProcIndexBuffer.Num(); I+=3) Obj += FString::Printf(TEXT("f %d %d %d\n"),Base+Part->ProcIndexBuffer[I],Base+Part->ProcIndexBuffer[I+1],Base+Part->ProcIndexBuffer[I+2]);
			Base += Part->ProcVertexBuffer.Num();
		};
		Export(TEXT("body"),Heli->HeliBodyMeshComponent->GetProcMeshSection(0));
		Export(TEXT("glass"),Heli->HeliBodyMeshComponent->GetProcMeshSection(1)); Export(TEXT("occupants"),Cabin);
		FFileHelper::SaveStringToFile(Obj, *(Output/FString::Printf(TEXT("model-%d.obj"), Type)));
	}
	Heli->RemoveMissionPassengers(100);
	TestEqual(TEXT("Plane carries one fare"), Heli->GetPassengerSeatCount(), 1);
	for (int32 Tool=0;Tool<int32(ESimCopterHelicopterTool::Count);++Tool)
		TestFalse(TEXT("Plane has no emergency tools"),Heli->IsToolAvailable(static_cast<ESimCopterHelicopterTool>(Tool)));
	Heli->RequestAirSupport(true);
	TestTrue(TEXT("Plane cannot order emergency air support"),Heli->LastToolStatus.Contains(TEXT("Transport only")));
	TestEqual(TEXT("Plane rejects medical manifest"), Heli->AddMissionPassengersForMission(1,12,ESimCopterMissionPassengerKind::Medevac), 0);
	auto* Person = World->SpawnActor<ASimCopterGroundAgent>();
	Person->BehaviorContext.ResetToState(4); Person->MissionEventId=12;
	TestTrue(TEXT("Plane accepts a transport fare"),Heli->CanAcceptPassenger(ESimCopterMissionPassengerKind::Transport,Person));
	Person->BehaviorContext.ResetToState(5);
	TestFalse(TEXT("Plane rejects a medic"),Heli->CanAcceptPassenger(ESimCopterMissionPassengerKind::Transport,Person));
	Person->SetInitialBehaviorClass(17); Person->SetPedestrianFigureName(TEXT("Coww"));
	TestTrue(TEXT("Cow recognized"),Person->IsCow());
	Person->SetMissionInjuredPose(); Person->SetMissionDeadPose();
	TestFalse(TEXT("Cow cannot become injured patient"),Person->IsMedevacVictim());
	TestFalse(TEXT("Cow cannot become dead patient"),Person->IsMissionPatientDead());
	TestFalse(TEXT("Cow cannot board plane"),Person->BoardCarrier(Heli,false));
	Heli->SwitchHelicopterModel(5);
	TestFalse(TEXT("Cow cannot board helicopter"),Person->BoardCarrier(Heli,false));
	auto* Pilot = World->SpawnActor<ASimCopterOnFootPawn>();
	TestFalse(TEXT("Cow cannot be carried"),Pilot->PickUpMissionPerson(Person));
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	{
		using namespace SimCopterMissions;
		FSimCopterMissionSystem System; System.Initialize(nullptr,1); System.BeginSession();
		const int32 Medical=System.CreateIncidentMedevacAt(20,20);
		const int32 Fare=System.CreateIncidentMedevacAt(25,25);
		auto* FareRecord=const_cast<FSimCopterMissionRecord*>(System.FindRecord(Fare));
		FareRecord->TypeMask=TYPE_Transport;
		System.bTransportOnly=true;
		const auto& Records=System.GetRecords();
		int32 MedicalIndex=INDEX_NONE;
		for(int32 I=0;I<Records.Num();++I) if(Records[I].EventId==Medical) MedicalIndex=I;
		System.SetMapFocusRecordIndex(MedicalIndex,EMapFocusReason::Created);
		TestEqual(TEXT("Plane focus skips unavailable emergency"),Records[System.GetMapFocusRecordIndex()].EventId,Fare);
		System.FocusNextMapRecord(); System.FocusPreviousMapRecord();
		TestEqual(TEXT("Plane mission cycling remains Transport only"),Records[System.GetMapFocusRecordIndex()].EventId,Fare);
		const int32 Age=System.FindRecord(Medical)->TimeAccum;
		System.Tick(.1f);
		TestEqual(TEXT("Emergency deadline pauses during plane sortie"),System.FindRecord(Medical)->TimeAccum,Age);
	}
	TestFalse(TEXT("Cow cannot create incidental medical mission"),Missions->CreateIncidentMedevacForVictim(Person));
	TestFalse(TEXT("Cow cannot create player-caused medical mission"),Missions->CreatePlayerCausedMedevacForVictim(Person));
	Person->SetActorLocation(Heli->GetActorLocation()+FVector(100,0,30));
	Person->BehaviorContext.Attributes[EBhavAttr::Visible]=1;
	TestTrue(TEXT("Cow still bounces from helicopter impact"),Person->ApplyHelicopterKnockdown(*Heli,FVector(500,0,0)));
	TestTrue(TEXT("Cow receives physical launch velocity"),Person->KnockdownVelocityCmPerSec.Size()>0);
	Person->Destroy(); Pilot->Destroy(); Missions->Destroy();

	Heli->SwitchHelicopterModel(SimCopterHelicopterRegistry::PlaneTypeIndex);
	Heli->PlaceOnHelipad(FVector::ZeroVector,0); Heli->bEngineRunning=true;
	FSimCopterFlightEnvironment Environment; FSimCopterFlightInputs Input;
	for (int32 I=0; I<40; ++I) Heli->StepFixedWing(.05f,Input,Environment);
	TestEqual(TEXT("Idling plane stays stopped for passenger boarding"),Heli->FlightModel.ForwardSpeed,0);
	Input.bPitchForwardKey=true; Input.ClimbCommand=1;
	for (int32 I=0; I<80; ++I) Heli->StepFixedWing(.05f,Input,Environment);
	TestTrue(TEXT("Plane takes off using forward speed"),Heli->FlightModel.State==ESimCopterFlightState::Flying);
	TestTrue(TEXT("Plane climbs"),SimCopterFixed::ToFloat(Heli->FlightModel.Altitude)*Heli->OriginalUnitToCm>1000);
	Input={}; Input.bTurnRightKey=true;
	for (int32 I=0; I<20; ++I) Heli->StepFixedWing(.05f,Input,Environment);
	TestTrue(TEXT("Plane turns more than ninety degrees in one second"),SimCopterFixed::ToFloat(Heli->FlightModel.Heading)>900);
	Input={}; Input.bPitchBackKey=true; Input.ClimbCommand=-1;
	for (int32 I=0; I<400; ++I) Heli->StepFixedWing(.05f,Input,Environment);
	TestTrue(TEXT("Plane can land and brake to a stop"),Heli->FlightModel.State==ESimCopterFlightState::Parked && Heli->FlightModel.ForwardSpeed==0);
	TArray<uint8> SavedPlane;
	TestTrue(TEXT("Plane runtime state can be saved"),Heli->CaptureRuntimeSaveState(SavedPlane));
	auto* RestoredPlane=World->SpawnActor<ASimCopterHelicopterPawn>();
	// ApplyPendingAircraftState restores the outer save's aircraft type before its
	// runtime archive (which deliberately contains motion/passengers, not the type).
	RestoredPlane->RestoreSavedCareerState(Heli->GetHelicopterTypeIndex(),0,0,1,0,0);
	TestTrue(TEXT("Plane runtime state can be restored"),RestoredPlane->RestoreRuntimeSaveState(SavedPlane));
	TestTrue(TEXT("Restored plane retains aircraft type and capacity"),RestoredPlane->IsFixedWingAircraft() && RestoredPlane->GetPassengerSeatCount()==1);
	RestoredPlane->Destroy();
	Environment.bHostileSurface=true; Heli->StepFixedWing(.05f,Input,Environment);
	TestTrue(TEXT("Plane cannot land on water"),Heli->FlightModel.State==ESimCopterFlightState::Dying);
	Controller->UnPossess(); Heli->Destroy();

	TArray<FString> Cities;
	IFileManager::Get().FindFilesRecursive(Cities, *(Root/TEXT("cities")),TEXT("*.sc2"),true,false);
	Cities.Sort(); TestTrue(TEXT("All supplied cities discovered"),Cities.Num()>=45);
	FString Report=TEXT("City,Road nodes,Service,Stations,Conservative maximum seconds,Measured response seconds\n");
	for (const FString& City : Cities)
	{
		auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
		Traffic->CityFile.FilePath=City; Traffic->OriginalGameRoot.Path=Root; Traffic->bSimulateWholeMap=false;
		if (!TestTrue(City,Traffic->RebuildSpawnData())) { Traffic->Destroy(); continue; }
		ASimCopterHelicopterPawn::EnsureTransportPlane(World);
		int32 PlaneCount=0;
		for (TActorIterator<ASimCopterHelicopterPawn> It(World);It;++It) if (It->IsFixedWingAircraft()) ++PlaneCount;
		TestEqual(TEXT("Every city has a boardable airport plane"),PlaneCount,1);
		for (int32 Service=0; Service<3; ++Service)
		{
			// Independently walk the complete, real road graph from all service origins.
			TArray<float> Distance; Distance.Init(TNumericLimits<float>::Max(),Traffic->RoadNodes.Num());
			TArray<int32> Queue;
			for (const auto& Station:Traffic->DispatchStations[Service])
				if (const int32* Node=Traffic->RoadNodeIndexByTile.Find(Station.RoadTile)) { Distance[*Node]=0; Queue.Add(*Node); }
			TArray<int32> Neighbors;
			for (int32 Cursor=0; Cursor<Queue.Num(); ++Cursor)
			{
				const int32 Node=Queue[Cursor]; Traffic->GetRoadRoutingNeighbors(Node,Neighbors);
				for (int32 Next:Neighbors)
				{
					const float Cost=Distance[Node]+Traffic->RoadLinkTravelSeconds(Node,Next);
					if (Cost<Distance[Next]) { Distance[Next]=Cost; Queue.Add(Next); }
				}
			}
			int32 Farthest=INDEX_NONE; float Worst=-1;
			for (int32 I=0;I<Distance.Num();++I) if (Distance[I]>Worst) { Worst=Distance[I]; Farthest=I; }
			TestTrue(FString::Printf(TEXT("%s service %d covers every road within 34 seconds"),*FPaths::GetBaseFilename(City),Service),Worst<=SimCopterDispatch::CoverageTravelSeconds+.01f);
			float Elapsed=0;
			if (Traffic->RoadNodes.IsValidIndex(Farthest))
			{
				const auto& Node=Traffic->RoadNodes[Farthest];
				const auto Result=Traffic->RequestEmergencyDispatch(static_cast<SimCopterDispatch::EService>(Service),FIntPoint(Node.FileX,Node.FileY),false);
				TestTrue(TEXT("Dispatcher selects reachable headquarters for worst-covered road"),Result==SimCopterDispatch::EDispatchResult::Dispatched);
				for (auto& Vehicle:Traffic->DispatchVehicles[Service])
				{
					if (!Vehicle.Agent.IsValid()) continue;
					while (Elapsed<45 && Vehicle.State!=ESimCopterDispatchVehicleState::OnScene)
					{
						Vehicle.Agent->Tick(.05f); Traffic->UpdateDispatchVehicles(.05f); Traffic->UpdateTunnelTransits(.05f); Elapsed+=.05f;
					}
					TestTrue(FString::Printf(TEXT("%s service %d arrives in under 45 seconds (%.2f)"),*FPaths::GetBaseFilename(City),Service,Elapsed),Vehicle.State==ESimCopterDispatchVehicleState::OnScene && Elapsed<45);
					break;
				}
			}
			Report+=FString::Printf(TEXT("%s,%d,%d,%d,%.3f,%.3f\n"),*FPaths::GetBaseFilename(City),Traffic->RoadNodes.Num(),Service,Traffic->DispatchStations[Service].Num(),Worst,Elapsed);
		}
		for (TActorIterator<AActor> It(World);It;++It)
			if (It->GetOwner()==Traffic || Cast<ASimCopterHelicopterPawn>(*It)) It->Destroy();
		Traffic->Destroy();
	}
	FFileHelper::SaveStringToFile(Report,*(Output/TEXT("city-response-times.csv")));
	GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
	return true;
}
#endif
