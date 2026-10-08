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
#endif
