#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Components/BoxComponent.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Flight/SimCopterAirOperations.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterPeopleReader.h"
#include "../UI/SSimCopterMapPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Game/SimCopterCheckup.h"

namespace
{
UWorld* PoliceTestWorld()
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	return World;
}
void PoliceTestBox(UWorld* World, FVector Center, FVector Extent)
{
	auto* Actor = World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Actor);
	Actor->SetRootComponent(Box); Box->SetBoxExtent(Extent);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
	Actor->SetActorLocation(Center);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPoliceRecoveryTest, "SimCopter.PoliceRecovery.DeployedOfficerRetrievesTasedSuspect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPoliceRecoveryTest::RunTest(const FString& Parameters)
{
	auto* World = PoliceTestWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	PoliceTestBox(World, FVector(0,0,-10), FVector(10000,10000,10));
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame"));
	auto Model = MakeShared<FPeopleBehaviorModel>(); FString Error;
	if (!TestTrue(TEXT("Original police programs load"), FSimCopterPeopleReader::LoadFromFile(
		FSimCopterPeopleReader::ResolvePeoplePath(Root), *Model, Error))) return false;
	auto Person = [&](int32 State, FVector At)
	{
		auto* Person = World->SpawnActor<ASimCopterGroundAgent>(); Person->SetOwner(Traffic);
		Person->InitialPersonState = State;
		Person->ConfigureAgent(ESimCopterGroundAgentKind::Pedestrian, TEXT(""), Root, 85);
		Person->BehaviorModel = Model; Person->bBehaviorActive = true;
		Person->BehaviorContext.ResetToState(State);
		Person->BehaviorContext.Attributes[EBhavAttr::Visible] = 1;
		Person->BehaviorContext.Attributes[EBhavAttr::MedevacHealth] = 100;
		Person->SetActorLocation(At + FVector(0,0,Person->GetCapsuleHalfHeightCm()+1));
		Traffic->PedestrianAgents.Add(Person); return Person;
	};
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SetActorLocation(FVector(-550,0,Heli->GetSimpleCollisionHalfHeight()+7.5f));
	auto* PC = World->SpawnActor<ASimCopterPlayerController>();
	PC->Player = NewObject<ULocalPlayer>(GEngine); World->AddController(PC); PC->Possess(Heli);
	for (const int32 State : {11,10,12,13})
	{
		auto* Suspect = Person(State,FVector(State==13 ? 5000 : 350,0,0));
		TestTrue(TEXT("Tase criminal before deploying officer"), Suspect->StunForArrest());
		auto* Officer = Person(7,FVector(-300,0,0));
		TestTrue(TEXT("Officer starts in helicopter"), Officer->BoardCarrier(Heli,false));
		TestTrue(TEXT("Officer deploys"), Officer->AlightFromCarrier(false));
		Officer->SetActorLocation(FVector(-300,0,Officer->GetCapsuleHalfHeightCm()+1));
		TArray<ASimCopterGroundAgent*> SeatBlockers;
		if (State==11 || State==10)
			while (Heli->GetAvailablePassengerSeats()>(State==10 ? 0 : 1))
			{
				auto* Blocker=Person(0,FVector(-2000,0,0));
				if (!Blocker->BoardCarrier(Heli,false)) { Blocker->Destroy(); break; }
				SeatBlockers.Add(Blocker);
			}
		bool bCarried = false;
		int32 FullCabinTicks=0;
		for (int32 Tick = 0; Tick < 2400 && Officer->GetBehaviorCarrier() != Heli; ++Tick)
		{
			World->Tick(LEVELTICK_TimeOnly,1.f/15);
			Officer->Tick(1.f/15); Suspect->Tick(1.f/15);
			bCarried |= Suspect->GetBehaviorCarrier() == Officer;
			if (State==10 && Heli->GetAvailablePassengerSeats()==0 && Suspect->GetBehaviorCarrier()==Officer &&
				FVector::Dist2D(Officer->GetActorLocation(),Heli->GetActorLocation())<150 && ++FullCabinTicks==30)
			{
				TestTrue(TEXT("Full cabin retains suspect in officer custody"),Suspect->GetBehaviorCarrier()==Officer);
				auto* Leaving=SeatBlockers.Pop(); Leaving->AlightFromCarrier(false); Leaving->Destroy();
			}
			if (!SeatBlockers.IsEmpty() && Suspect->GetBehaviorCarrier()==Heli) break;
		}
		TestTrue(FString::Printf(TEXT("State %d is physically carried by the officer"),State), bCarried);
		TestTrue(TEXT("Same suspect returns in helicopter"), Suspect->GetBehaviorCarrier() == Heli);
		if (SeatBlockers.IsEmpty()) TestTrue(TEXT("Officer returns alongside suspect"), Officer->GetBehaviorCarrier() == Heli);
		else TestNull(TEXT("One free seat loads suspect and leaves officer outside"),Officer->GetBehaviorCarrier());
		TestTrue(TEXT("Suspect is handcuffed without premature mission completion"), Suspect->IsHandcuffed() && !Suspect->HasMissionResolutionReported());
		Suspect->AlightFromCarrier(false); Officer->AlightFromCarrier(false);
		Suspect->Destroy(); Officer->Destroy();
		for(auto* Blocker:SeatBlockers) { Blocker->AlightFromCarrier(false); Blocker->Destroy(); }
	}
	// Beyond the old two-tile close search, follow a moving criminal and use the
	// original 1060 arrest reaction / 1053 armed hold once physical contact is reached.
	auto* Runner = Person(10,FVector(5000,0,0));
	auto* Pursuer = Person(7,FVector(0,0,0));
	bool bChased = false;
	for (int32 Tick=0; Tick<2700 && Runner->BehaviorContext.LastReactionProgramId!=1060; ++Tick)
	{
		World->Tick(LEVELTICK_TimeOnly,1.f/15);
		Runner->AddActorWorldOffset(FVector(4,0,0));
		Pursuer->Tick(1.f/15);
		bChased |= Pursuer->GetActorLocation().X > 1000;
	}
	TestTrue(TEXT("Officer pursues a moving criminal from fifty metres away"),bChased);
	TestEqual(TEXT("Contact invokes original caught reaction"),Runner->BehaviorContext.LastReactionProgramId,1060);
	TestTrue(TEXT("Arrest requires reaching suspect rather than arresting remotely"),FVector::Dist2D(Pursuer->GetActorLocation(),Runner->GetActorLocation())<150);
	Runner->Tick(1.f/15);
	Pursuer->Tick(1.f/15);
	TestTrue(TEXT("Officer resumes original armed hold"),Pursuer->BehaviorContext.Stack.ContainsByPredicate(
		[](const auto& Frame) { return Frame.ProgramId==1053 && Frame.RecordIndex>=26; }));
	Runner->Destroy(); Pursuer->Destroy();
	// A disabled burglar remains inside its car, so a people-only scan cannot arrest it.
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
	const int32 Crime=Missions->CreateMissionAt(60,60,SimCopterMissions::TYPE_Burglar);
	auto* Car=World->SpawnActor<ASimCopterGroundAgent>(); Car->AgentKind=ESimCopterGroundAgentKind::Vehicle;
	Car->SetOwner(Traffic); Car->SetActorLocation(FVector(500,900,24));
	Car->MakeCriminalCar(Crime,65536); Traffic->CriminalCars.Add(Car); Car->DisableVehicleWithTaser();
	auto* Officer=Person(7,FVector(0,900,0));
	TestTrue(TEXT("Burglar car is disabled before the officer approaches"),Car->IsVehicleTaserDisabled());
	for (int32 Tick=0;Tick<150 && !Car->IsActorBeingDestroyed();++Tick)
	{
		World->Tick(LEVELTICK_TimeOnly,1.f/15); Officer->Tick(1.f/15); Car->Tick(1.f/15);
	}
	TestTrue(TEXT("Dropped officer walks five metres and arrests disabled burglar car"),Car->IsActorBeingDestroyed());
	Missions->MissionSystem.Tick(0);
	TestFalse(TEXT("Disabled-car arrest completes burglar mission"),Missions->IsMissionEventActive(Crime));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPoliceDestinationTest, "SimCopter.PoliceRecovery.NearestStationMapRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPoliceDestinationTest::RunTest(const FString& Parameters)
{
	using namespace SimCopterMap;
	auto* World = PoliceTestWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>(); Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(7,FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0,FSimCity2000City::TileCount);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	auto* Recovery = World->GetSubsystem<USimCopterAirOperationsSubsystem>();
	Recovery->ServiceRoofs.Add(0xD2, {FVector(0,8000,300), FVector(0,-16000,300)});
	auto Panel = SNew(SSimCopterMapPanel).Pawn(Heli);
	Panel->CityData.Xbld.Init(0,MapTiles*MapTiles);
	Panel->CityData.TerrainClass.Init(10,MapTiles*MapTiles);
	Panel->CityData.AltitudeShade.Init(0,MapTiles*MapTiles);
	Panel->CityData.OnFire.Init(0,MapTiles*MapTiles);
	auto HasRay = [&](const FVector& Roof)
	{
		FSimCopterMapFrame Frame;
		TestTrue(TEXT("Actual cockpit panel builds frame"),Panel->BuildFrame(Frame));
		Panel->RefreshMissionLabel(Frame);
		int32 X=0,Y=0; Traffic->TryGetPeopleTileCoordinateAtWorldLocation(Roof,X,Y);
		const FIntPoint Delta(-(Y-Frame.CentreTile.Y),-(X-Frame.CentreTile.X));
		FSimCopterMapSettings Settings; Settings.Zoom=0;
		Settings.bShowMissionBlips=false; Settings.bShowServiceBlips=false;
		FSimCopterMapRaster Raster; Raster.Render(Frame,Settings);
		const int32 PX=CentreX+FMath::Sign(Delta.X)*FMath::Min(8,FMath::Abs(Delta.X)), PY=CentreY+FMath::Sign(Delta.Y)*FMath::Min(8,FMath::Abs(Delta.Y));
		return Raster.GetPixels()[PY*BufferWidth+PX] == Color::PoliceStation;
	};
	TestFalse(TEXT("Empty cabin has no police route"),HasRay(FVector(0,8000,300)));
	for (int32 State : {11,10,12,13})
	{
		Heli->SetActorLocation(FVector::ZeroVector);
		auto* Suspect=World->SpawnActor<ASimCopterGroundAgent>(); Suspect->SetOwner(Traffic);
		Suspect->InitialPersonState=State; Suspect->BehaviorContext.ResetToState(State); Suspect->MissionEventId=State;
		TestFalse(TEXT("Active criminal remains a threat"),Missions->IsCrimeInCustody(State));
		TestTrue(TEXT("Suspect is disabled"),Suspect->StunForArrest());
		TestTrue(TEXT("Tased criminal cannot commit crimes"),Missions->IsCrimeInCustody(State));
		TestTrue(TEXT("Suspect boards"),Suspect->BoardCarrier(Heli,false));
		TestTrue(TEXT("Cuffed cabin prisoner cannot commit crimes"),Missions->IsCrimeInCustody(State));
		TArray<uint8> Saved; TestTrue(TEXT("Prisoner state saves"),Suspect->CaptureRuntimeSaveState(Saved));
		TestTrue(TEXT("Prisoner state restores"),Suspect->RestoreRuntimeSaveState(Saved));
		TMap<FName,AActor*> References; Suspect->ResolveRuntimeSaveReferences(References,Heli);
		TestTrue(TEXT("Restored custody still stops crime"),Missions->IsCrimeInCustody(State));
		TestTrue(TEXT("Prisoner produces nearest station line even with blips hidden"),HasRay(FVector(0,8000,300)));
		TestEqual(TEXT("Route label identifies police destination"),Panel->MissionLabel->GetText().ToString(),FString(TEXT("POLICE STATION")));
		Heli->SetActorLocation(FVector(0,-14000,0));
		TestTrue(TEXT("Flying closer to another station updates destination"),HasRay(FVector(0,-16000,300)));
		Suspect->CompletePoliceDelivery(FVector(0,-16000,300));
		TestFalse(TEXT("Delivery removes police route"),HasRay(FVector(0,-16000,300)));
		TestFalse(TEXT("Delivered target no longer counted in custody"),Missions->IsCrimeInCustody(State));
		Suspect->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterCheckupCrewTest, "SimCopter.Checkup.PaidServiceCrew",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterCheckupCrewTest::RunTest(const FString& Parameters)
{
	auto* World=PoliceTestWorld();
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	PoliceTestBox(World,FVector(0,0,-10),FVector(10000,10000,10));
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>(); Traffic->ActiveTileSize=400;
	Traffic->PeopleTileClasses.Init(1,FSimCity2000City::TileCount); // Airport apron, with no ambient walking row.
	Traffic->TileCenterWorldZ.Init(0,FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0,FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0xde,FSimCity2000City::TileCount);
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession(); Missions->AddSessionCash(10000);
	auto* Heli=World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SetActorLocation(FVector(0,0,Heli->GetSimpleCollisionHalfHeight()+7.5f)); Heli->GroundClearanceCm=7.5f;
	auto* PC=World->SpawnActor<ASimCopterPlayerController>(); PC->Player=NewObject<ULocalPlayer>(GEngine);
	World->AddController(PC); PC->Possess(Heli);
	Heli->SetCareerEquipmentOwned(ESimCopterHelicopterTool::TearGas,true);
	for (int32 Service=0;Service<4;++Service)
	{
		Heli->FlightModel.HitPoints=100; Heli->FlightModel.Fuel=0; Heli->EquipmentState.CareerTearGasRounds=0;
		FSimCopterCheckupOrder Order;
		Order.DamageDollars=(Service==0 || Service==3) ? 20 : 0;
		Order.FuelDollars=(Service==1 || Service==3) ? 20 : 0;
		Order.TearGasRounds=(Service==2 || Service==3) ? 1 : 0;
		Heli->ApplyCheckupOrder(Order);
		TArray<ASimCopterGroundAgent*> Crew;
		for (auto Weak:Traffic->PedestrianAgents) if(auto* Worker=Weak.Get();Worker && !Worker->IsActorBeingDestroyed()) Crew.Add(Worker);
		TestEqual(TEXT("One original mechanic per paid service, including teargas"),Crew.Num(),Service==3 ? 3 : 1);
		for (auto* Worker:Crew)
		{
			TestEqual(TEXT("Mechanic uses original program"),Worker->InitialBehaviorProgramId,1204);
			TestTrue(TEXT("Mechanic has a visible original figure"),Worker->IsUsingOriginalMesh() && !Worker->IsHidden());
			TestFalse(TEXT("Scripted mechanics can cross airport apron"),Worker->IsAmbientStreetWalker());
			TestFalse(TEXT("Worker is not a passenger"),Worker->GetBehaviorCarrier()!=nullptr);
			for (auto* Other:Crew) if(Other!=Worker)
				TestTrue(TEXT("Crew have separate positions after grounding"),FVector::Dist2D(Worker->GetActorLocation(),Other->GetActorLocation())>=
					Worker->GetCollisionRadiusCm()+Other->GetCollisionRadiusCm()+12);
		}
		TestEqual(TEXT("Repeated order cannot pile another crew onto existing workers"),Traffic->SpawnCheckupCrew(*Heli,3),0);
		bool bWorked=false;
		for(int32 Tick=0;Tick<900 && !bWorked;++Tick)
		{
			World->Tick(LEVELTICK_TimeOnly,1.f/15);
			for(auto* Worker:Crew) if(!Worker->IsActorBeingDestroyed())
			{ Worker->Tick(1.f/15); bWorked |= Worker->GetBehaviorAttribute(35)>0; }
		}
		for(auto* Worker:Crew)
			AddInfo(FString::Printf(TEXT("Service %d: work=%d, active=%d, location=%s, selected=%s, top program=%d"),
				Service,Worker->GetBehaviorAttribute(35),Worker->IsBehaviorActive(),*Worker->GetActorLocation().ToString(),
				*GetNameSafe(Worker->BehaviorContext.SelectedObject.Get()),
				Worker->BehaviorContext.Stack.IsEmpty() ? -1 : Worker->BehaviorContext.Stack.Last().ProgramId));
		TestTrue(FString::Printf(TEXT("Service %d original work program reaches its working counter"),Service),bWorked);
		for(auto* Worker:Crew) Worker->Destroy();
		Traffic->PedestrianAgents.Reset();
	}
	Heli->ApplyCheckupOrder({});
	TestEqual(TEXT("Empty order spawns nobody"),Traffic->PedestrianAgents.Num(),0);
	Heli->GroundClearanceCm=500;
	TestEqual(TEXT("No servicing crew materializes in midair"),Traffic->SpawnCheckupCrew(*Heli,3),0);
	return true;
}
#endif
