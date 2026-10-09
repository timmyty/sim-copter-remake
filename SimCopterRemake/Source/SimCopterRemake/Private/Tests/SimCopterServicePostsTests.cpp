#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Game/SimCopterPlayerController.h"
#include "Ground/SimCopterOnFootPawn.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Components/BoxComponent.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterParticleFX.h"
#include "Missions/SimCopterMissionSystemActor.h"
#include "Formats/SimCity2000Reader.h"
#include "Formats/SimCopterPeopleReader.h"
#include "Sound/SoundWave.h"
#include "City/SimCity2000CityActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/StaticMesh.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterServicePostsTest, "SimCopter.ServicePosts.SpacingEntrancesAndImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterServicePostsTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto Box = [&](FVector Center, FVector Extent)
	{
		auto* Actor=World->SpawnActor<AActor>(); auto* Component=NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Component); Component->SetBoxExtent(Extent);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Component->SetCollisionObjectType(ECC_WorldStatic); Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->RegisterComponent(); Actor->SetActorLocation(Center); return Actor;
	};
	Box(FVector(0,0,-10),FVector(10000,10000,10));
	Box(FVector(200,-200,600),FVector(560,560,600));
	Box(FVector(2200,-200,400),FVector(360,360,400));
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize=400;
	Traffic->PeopleTileClasses.Init(7,FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0,FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0,FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d,FSimCity2000City::TileCount);
	auto Site=[&](int X,uint8 Type,FVector Center,int Size)
	{
		auto& Node=Traffic->PedestrianNodes.AddDefaulted_GetRef();
		Node.FileX=X;Node.FileY=64;Node.BuildingId=Type;Node.PeopleFootprintSize=Size;Node.Location=Center;
		Traffic->XbldTileIds[64*128+X]=Type;
		Traffic->PedestrianNodeIndexByTile.Add(FIntPoint(X,64),Traffic->PedestrianNodes.Num()-1);
	};
	Site(64,0xD1,FVector(200,-200,0),3);Site(69,0xD2,FVector(2200,-200,0),2);
	auto* RoofMedic=Traffic->EnsureHospitalParamedicAtTile(64,64,8);
	TestNotNull(TEXT("Hospital roof supports eight-person team"),RoofMedic);
	TestNotNull(TEXT("Ground medic exists independently"),Traffic->EnsureBuildingEntranceCrew(64,64));
	TestNotNull(TEXT("Police roof crew"),Traffic->EnsureServiceRoofCrew(69,64,3));
	TestNotNull(TEXT("Ground police exists independently"),Traffic->EnsureBuildingEntranceCrew(69,64));
	const int32 Count=Traffic->PedestrianAgents.Num();
	TestEqual(TEXT("Eight roof medics, three roof police, two ground staff"),Count,13);
	for(int32 A=0;A<Count;++A) for(int32 B=A+1;B<Count;++B)
	{
		const FVector PA=Traffic->PedestrianAgents[A]->GetActorLocation(), PB=Traffic->PedestrianAgents[B]->GetActorLocation();
		TestTrue(TEXT("Every same-floor pair has movement clearance"),FMath::Abs(PA.Z-PB.Z)>150 || FVector::Dist2D(PA,PB)>=89.9);
	}
	Traffic->EnsureHospitalParamedicAtTile(64,64,8);Traffic->EnsureServiceRoofCrew(69,64,3);
	TestEqual(TEXT("Repeated staffing does not multiply people"),Traffic->PedestrianAgents.Num(),Count);
	if (Count>=3)
	{
		Traffic->PedestrianAgents[2]->SetActorLocation(Traffic->PedestrianAgents[1]->GetActorLocation());
		Traffic->EnsureHospitalParamedicAtTile(64,64,8);
		TestTrue(TEXT("Coincident crew from older saves are separated"),FVector::Dist2D(Traffic->PedestrianAgents[1]->GetActorLocation(),Traffic->PedestrianAgents[2]->GetActorLocation())>=90);
	}
	FVector Entrance;
	TestTrue(TEXT("Hospital front entrance resolves"),Traffic->TryGetBuildingEntrancePost(64,64,Entrance));
	TestTrue(TEXT("Entrance on terrain level"),FMath::Abs(Entrance.Z)<1);
	TestTrue(TEXT("Ground floor accepted as hospital"),Traffic->IsAtHospitalEntrance(Entrance));
	TestFalse(TEXT("Air above entrance is not a delivery"),Traffic->IsAtHospitalEntrance(Entrance+FVector(0,0,500)));
	TestFalse(TEXT("Street away from entrance is not hospital"),Traffic->IsAtHospitalEntrance(Entrance+FVector(1000,0,0)));
	FVector Full;
	TestFalse(TEXT("Occupied roof point has no centre fallback"),Traffic->TryFindClearRoofSpawnPoint(
		RoofMedic ? RoofMedic->GetActorLocation()-FVector(0,0,RoofMedic->GetCapsuleHalfHeightCm()) : FVector(200,-200,1200), 0,0,Full));
	auto* GroundMedic=Traffic->EnsureBuildingEntranceCrew(64,64);
	TestTrue(TEXT("Ground medic uses hospital handoff program"),GroundMedic && static_cast<ISimCopterBehaviorWorld*>(GroundMedic)->GetCurrentTileBuildingId()==0xD1 && static_cast<ISimCopterBehaviorWorld*>(GroundMedic)->IsCurrentTileServiceable());
	TestTrue(TEXT("Height-aware lookup selects ground crew"),Traffic->FindNearestAvailablePersonInState(Entrance,5,500,true)==GroundMedic);
	auto* Missions=World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
	auto* Controller=World->SpawnActor<ASimCopterPlayerController>(); World->AddController(Controller);
	Controller->Player=NewObject<ULocalPlayer>(GEngine); Controller->ChangeState(NAME_Playing);
	auto* Pilot=World->SpawnActor<ASimCopterOnFootPawn>(); Controller->Possess(Pilot);
	Pilot->SetActorLocation(Entrance+FVector(90,0,88));
	TSharedPtr<FPeopleBehaviorModel> PeopleModel=MakeShared<FPeopleBehaviorModel>();
	FString PeopleError;
	const FString OriginalRoot=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame"));
	TestTrue(TEXT("Original patient behavior loads"),FSimCopterPeopleReader::LoadFromFile(
		FSimCopterPeopleReader::ResolvePeoplePath(OriginalRoot),*PeopleModel,PeopleError));
	auto PatientAt=[&](FVector Feet)
	{
		auto* Person=World->SpawnActor<ASimCopterGroundAgent>(); Person->SetOwner(Traffic);
		Person->BehaviorModel=PeopleModel; Person->bBehaviorActive=true;
		Person->BehaviorContext.ResetToState(0);
		Person->SetActorLocation(Feet+FVector(0,0,Person->GetCapsuleHalfHeightCm()));
		Traffic->PedestrianAgents.Add(Person);
		TestTrue(TEXT("Medical incident created"),Missions->CreateIncidentMedevacForVictim(Person));
		TestTrue(TEXT("Patient enters medical behavior"),Person->IsMedevacVictim());
		return Person;
	};
	auto* Carried=PatientAt(Entrance+FVector(90,0,0));
	TestTrue(TEXT("Pilot carries entrance patient"),Pilot->PickUpMissionPerson(Carried));
	Traffic->UpdateServiceEntrances(1.1f);
	TestTrue(TEXT("Entrance medic accepts pilot-carried patient"),Carried->HasMissionResolutionReported());
	TestFalse(TEXT("Pilot carry slot released after admission"),Pilot->IsCarryingMissionPerson());
	const int32 Delivered=Missions->MissionSystem.FindRecord(Carried->MissionEventId)->MedevacDelivered;
	Traffic->UpdateServiceEntrances(1.1f);
	TestEqual(TEXT("Entrance admission credited only once"),Missions->MissionSystem.FindRecord(Carried->MissionEventId)->MedevacDelivered,Delivered);
	auto* Dropped=PatientAt(Entrance+FVector(-90,0,0));
	Traffic->UpdateServiceEntrances(1.1f);
	TestTrue(TEXT("Entrance also accepts dropped patient"),Dropped->HasMissionResolutionReported());
	auto* Above=PatientAt(Entrance+FVector(0,0,500));
	Traffic->UpdateServiceEntrances(1.1f);
	TestFalse(TEXT("Patient above entrance is not admitted remotely"),Above->HasMissionResolutionReported());
	auto* FX=Missions->FindComponentByClass<USimCopterParticleFXComponent>();
	TestNotNull(TEXT("Mission particle pool"),FX);
	auto* Car=World->SpawnActor<ASimCopterGroundAgent>(); Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle,TEXT(""),TEXT(""),0);
	const int32 Before=FX ? FX->GetActiveCount(ESimCopterEffectPool::Wash20) : 0;
	Car->ApplyHelicopterVehicleImpact(FVector(6000,6000,50));
	const int32 After=FX ? FX->GetActiveCount(ESimCopterEffectPool::Wash20) : 0;
	TestEqual(TEXT("One accepted impact emits fourteen sparks"),After-Before,14);
	Car->ApplyHelicopterVehicleImpact(FVector(6000,6000,50));
	TestEqual(TEXT("Continuous contact does not repeat sparks"),FX ? FX->GetActiveCount(ESimCopterEffectPool::Wash20) : 0,After);
	auto* Sound=LoadObject<USoundWave>(nullptr,TEXT("/Game/Generated/CityAtlas/S_HelicopterVehicleImpact.S_HelicopterVehicleImpact"));
	TestTrue(TEXT("Original impact cue imported with expected duration"),Sound && Sound->Duration>.5f && Sound->Duration<.6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterHospitalCityTest,"SimCopter.ServicePosts.ActualHospitalGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterHospitalCityTest::RunTest(const FString& Parameters)
{
	const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* City=World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(),FTransform::Identity);
	City->bLoadOnConstruction=false;City->bRenderProceduralMapExtension=false;City->bRenderStreetLightSpotLights=false;
	City->GetRootComponent()->SetMobility(EComponentMobility::Static);
	City->CityFile.FilePath=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("../Reference/SimCopterOriginalGame/cities/career/city1.sc2"));
	City->FinishSpawning(FTransform::Identity);City->RebuildCity();
	int32 HospitalMeshes=0;
	TArray<UInstancedStaticMeshComponent*> Components;City->GetComponents(Components);
	for(auto* Component:Components)
	{
		bool bFacade=false,bPad=false,bRoof=false;
		for(int32 M=0;M<Component->GetNumMaterials();++M) if(auto* Material=Component->GetMaterial(M))
		{
			bFacade|=Material->GetName()==TEXT("M_HospitalFacade");
			bPad|=Material->GetName()==TEXT("M_HospitalHelipad");
			bRoof|=Material->GetName()==TEXT("M_HospitalRoof");
		}
		if(!bFacade) continue;
		++HospitalMeshes;TestTrue(TEXT("Hospital binds dedicated roof and crisp helipad materials"),bPad&&bRoof);
		TestTrue(TEXT("Hospital retains cooked building collision"),Component->GetStaticMesh()->GetBodySetup()->TriMeshGeometries.Num()>0);
	}
	TestEqual(TEXT("One shared hospital mesh services the city's placements"),HospitalMeshes,1);
	auto* Traffic=World->SpawnActor<ASimCopterTrafficSystemActor>();
	TestTrue(TEXT("Actual city route and terrain data loads"),Traffic->RebuildSpawnData());
	TArray<ASimCopterTrafficSystemActor::FHospitalSite> Sites;Traffic->GetHospitalSites(Sites);
	TestEqual(TEXT("Islandtown has two hospitals"),Sites.Num(),2);
	for(const auto& Site:Sites)
	{
		FVector Roof,Entrance;float Extent;
		TestTrue(TEXT("Original hospital roof is still a valid deck"),Traffic->TryGetBuildingRoofPost(Site.OriginTile.X,Site.OriginTile.Y,Roof,Extent));
		TestTrue(TEXT("Actual hospital has accessible ground entrance"),Traffic->TryGetBuildingEntrancePost(Site.OriginTile.X,Site.OriginTile.Y,Entrance));
		TestTrue(TEXT("Entrance is below the preserved roof"),Roof.Z-Entrance.Z>500);
		TestNotNull(TEXT("Eight medics can occupy real hospital geometry"),Traffic->EnsureHospitalParamedicAtTile(Site.OriginTile.X,Site.OriginTile.Y,8));
		TestNotNull(TEXT("Medic on ground beside real hospital"),Traffic->EnsureBuildingEntranceCrew(Site.OriginTile.X,Site.OriginTile.Y));
		TestFalse(TEXT("Entrance lies outside the building's collision bounds"),City->IsInsideStandingBuildingBounds(Entrance,32));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterGroundMedicTest, "SimCopter.ServicePosts.GroundMedicPatrolAndBoarding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterGroundMedicTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	auto* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Floor = World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Box); Box->SetBoxExtent(FVector(10000,10000,10));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-10));
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(7,FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0,FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0,FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d,FSimCity2000City::TileCount);
	auto& Node = Traffic->PedestrianNodes.AddDefaulted_GetRef();
	Node.FileX=64; Node.FileY=64; Node.BuildingId=0xD1; Node.PeopleFootprintSize=3; Node.Location=FVector(200,-200,0);
	Traffic->XbldTileIds[64*128+64]=0xD1;
	Traffic->PedestrianNodeIndexByTile.Add(FIntPoint(64,64),0);
	FVector Entrance;
	if (!TestTrue(TEXT("Ground entrance resolves"),Traffic->TryGetBuildingEntrancePost(64,64,Entrance))) return false;
	TArray<UActorComponent*> Components; Traffic->GetComponents(Components);
	TestFalse(TEXT("Hospital has no entrance sign text, panel or pole"),Components.ContainsByPredicate([](const auto* Component)
		{ return Component->ComponentHasTag(TEXT("ServiceEntranceSign")); }));
	auto* Medic = Traffic->EnsureBuildingEntranceCrew(64,64);
	if (!TestNotNull(TEXT("Ground medic posted"),Medic)) return false;
	auto* Missions = World->SpawnActor<ASimCopterMissionSystemActor>();
	Missions->MissionSystem.Initialize(nullptr,1); Missions->MissionSystem.BeginSession();
	const FVector Home = Medic->GetActorLocation();
	float FurthestIdle = 0;
	for (int32 Frame=0; Frame<3600; ++Frame)
	{
		Medic->Tick(1.0f/60);
		const FVector Offset = Medic->GetActorLocation()-Home;
		FurthestIdle=FMath::Max(FurthestIdle,float(FMath::Max(FMath::Abs(Offset.X),FMath::Abs(Offset.Y))));
	}
	TestTrue(TEXT("One minute of idle walking stays within 1.3 metres of entrance on each axis"),FurthestIdle<=130);
	// A crowd/traffic displacement must not turn an entrance worker into an unbounded walker.
	Medic->SetActorLocation(Home+FVector(1000,0,0));
	Medic->Tick(0);
	TestTrue(TEXT("Displaced ground medic retains its post"),Medic->bHasHospitalRoofPost);
	TestTrue(TEXT("Displaced ground medic stays inside the nearby service area"),FVector::Dist2D(Medic->GetActorLocation(),Home)<570);
	Medic->SetHospitalRoofPost(Entrance,280); Medic->SetActorLocation(Home);
	TArray<uint8> SavedPost;
	TestTrue(TEXT("Ground post saves using the existing runtime format"),Medic->CaptureRuntimeSaveState(SavedPost));
	Medic->SetHospitalRoofPost(Entrance+FVector(0,0,1200),600);
	TestTrue(TEXT("Ground post restores"),Medic->RestoreRuntimeSaveState(SavedPost));
	Medic->SetActorLocation(Home+FVector(350,-350,0));
	const int32 PostedCount=Traffic->PedestrianAgents.Num();
	TestTrue(TEXT("Approaching worker still represents hospital ground service"),static_cast<ISimCopterBehaviorWorld*>(Medic)->GetCurrentTileBuildingId()==0xD1 && static_cast<ISimCopterBehaviorWorld*>(Medic)->IsCurrentTileServiceable());
	TestTrue(TEXT("Staffing retains the worker approaching beyond the old search circle"),Traffic->EnsureBuildingEntranceCrew(64,64)==Medic);
	TestEqual(TEXT("Approach does not spawn duplicate entrance crew"),Traffic->PedestrianAgents.Num(),PostedCount);
	Medic->SetActorLocation(Home);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SetActorLocation(Entrance+FVector(380,-380,Heli->GetSimpleCollisionHalfHeight()));
	Heli->GroundClearanceCm=7.5f;
	ISimCopterBehaviorWorld& Actions=*Medic;
	TestFalse(TEXT("No medical call keeps ground medic on duty"),Actions.SelectOwningVehicle(Medic->BehaviorContext));
	auto* Patient=World->SpawnActor<ASimCopterGroundAgent>();
	Patient->SetOwner(Traffic); Patient->BehaviorModel=Medic->BehaviorModel; Patient->bBehaviorActive=true;
	Patient->BehaviorContext.ResetToState(0);
	Patient->SetActorLocation(Home+FVector(2500,0,0)); Traffic->PedestrianAgents.Add(Patient);
	TestTrue(TEXT("Waiting patient creates a medical call"),Missions->CreateIncidentMedevacForVictim(Patient));
	TestTrue(TEXT("Ground medic can select nearby helicopter for medical call"),Actions.SelectOwningVehicle(Medic->BehaviorContext));
	const FVector Landing=Heli->GetActorLocation();
	Heli->SetActorLocation(Landing+FVector(800,0,0));
	TestFalse(TEXT("Ground medic does not chase distant helicopter"),Actions.SelectOwningVehicle(Medic->BehaviorContext));
	Heli->SetActorLocation(Landing+FVector(0,0,1200)); Heli->GroundClearanceCm=1200;
	TestFalse(TEXT("Ground medic cannot board an airborne helicopter"),Medic->BoardCarrier(Heli,false));
	Heli->SetActorLocation(Landing); Heli->GroundClearanceCm=7.5f;
	Medic->BehaviorContext.ResetToState(5);
	const int32 EmptySeats=Heli->GetAvailablePassengerSeats();
	for (int32 Frame=0; Frame<7200 && Medic->GetBehaviorCarrier()!=Heli; ++Frame) Medic->Tick(1.0f/60);
	TestTrue(TEXT("Original medic program walks to nearby helicopter and boards"),Medic->GetBehaviorCarrier()==Heli);
	TestEqual(TEXT("Ground medic consumes one cabin seat"),Heli->GetAvailablePassengerSeats(),EmptySeats-1);
	TestFalse(TEXT("Boarding releases entrance confinement"),Medic->bHasHospitalRoofPost);
	if (Medic->GetBehaviorCarrier()==Heli)
	{
		Heli->SetActorLocation(Landing+FVector(2000,0,1200)); Heli->GroundClearanceCm=1200;
		Medic->Tick(1.0f/60);
		TestTrue(TEXT("Boarded medic travels with aircraft instead of returning to post"),Medic->GetBehaviorCarrier()==Heli && FVector::Dist2D(Medic->GetActorLocation(),Home)>1500);
	}
	AddInfo(FString::Printf(TEXT("Idle extent %.1f cm; final offset %s; carrier %s"),FurthestIdle,
		*(Medic->GetActorLocation()-Home).ToString(),*GetNameSafe(Medic->GetBehaviorCarrier())));
	return true;
}
#endif
