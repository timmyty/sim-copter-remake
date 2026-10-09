#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "City/SimCity2000CityActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Formats/SimCity2000Reader.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPatientPlacementTest,
	"SimCopter.Safety.OutdoorPatients", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterPatientPlacementTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto Box = [&](const FVector& Center, const FVector& Extent)
	{
		auto* Actor = World->SpawnActor<AActor>();
		auto* Component = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Component);
		Component->SetBoxExtent(Extent);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Component->SetCollisionObjectType(ECC_WorldStatic);
		Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->RegisterComponent();
		Actor->SetActorLocation(Center);
		return Actor;
	};
	Box(FVector(0, 0, -10), FVector(10000, 10000, 10));
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->ActiveTileSize = 400;
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);
	Traffic->XbldTileIds.Init(0x1d, FSimCity2000City::TileCount);
	Traffic->bRequireOriginalPopulationMeshes = false;
	FVector Center;
	Traffic->TryGetTileCenterWorldLocation(64, 64, Center);
	TestTrue(TEXT("Clear ground permits a patient"), Traffic->IsOutdoorPatientSpawnValid(Center + FVector(0,0,92)));

	// A building overhang can cover tiles marked as road. Generic placement permits that
	// road exception, but medical placement must keep searching outside its actual geometry.
	AActor* Roof = Box(Center + FVector(0,0,600), FVector(550,550,100));
	TestTrue(TEXT("Legacy road exception accepts this covered location"), Traffic->IsMissionGroundSpawnValid(Center + FVector(0,0,92)));
	TestFalse(TEXT("Road under a roof is not outdoor ground"), Traffic->IsOutdoorPatientSpawnValid(Center + FVector(0,0,92)));
	TestFalse(TEXT("On top of a roof is also rejected"), Traffic->IsOutdoorPatientSpawnValid(Center + FVector(0,0,792)));
	for (int32 Index = 0; Index < 8; ++Index)
	{
		ASimCopterGroundAgent* Patient = nullptr;
		if (TestTrue(TEXT("Callout searches outside the building"), Traffic->TrySpawnMissionPerson(6, -1, 64, 64, 800 + Index, FString(), &Patient)) && Patient)
		{
			const FVector At = Patient->GetActorLocation();
			TestTrue(TEXT("Final patient lies outside roof footprint"), FMath::Abs(At.X-Center.X) > 550 || FMath::Abs(At.Y-Center.Y) > 550);
			TestTrue(TEXT("Final patient stands on ground, not rooftop"), FMath::Abs(At.Z - Patient->GetCapsuleHalfHeightCm() - 1) < 2);
		}
	}
	TestEqual(TEXT("Unsafe world-location spawn has no forced fallback"),
		Traffic->SpawnMissionPeopleAtWorldLocation(2, Center, 900, 6, -1, 100), 0);
	Roof->Destroy();

	// Hollow/missing triangle collision must not admit the occupied child tiles of a 4x4 building.
	for (int32 Y = 62; Y <= 65; ++Y)
		for (int32 X = 62; X <= 65; ++X) Traffic->PeopleTileClasses[Y*128+X] = 13;
	ASimCopterGroundAgent* Patient = nullptr;
	if (TestTrue(TEXT("Large footprint searches out to open ground"), Traffic->TrySpawnMissionPerson(6, -1, 64, 64, 901, FString(), &Patient)) && Patient)
		TestEqual(TEXT("Patient is outside every occupied child tile"), Traffic->GetPeopleTileClassAtWorldLocation(Patient->GetActorLocation()), 7);
	Traffic->PeopleTileClasses.Init(13, FSimCity2000City::TileCount);
	TestFalse(TEXT("Fully blocked neighborhood refuses the callout"), Traffic->TrySpawnMissionPerson(6, -1, 64, 64, 902));
	Traffic->PeopleTileClasses.Init(7, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(1, FSimCity2000City::TileCount);
	TestFalse(TEXT("Water is not patient ground"), Traffic->TrySpawnMissionPerson(6, -1, 64, 64, 903));
	Traffic->WaterTileFlags.Init(0, FSimCity2000City::TileCount);

	// A raised mesh has no overlap at the old 92 cm sample, but its complete horizontal
	// bounds still exclude the patient. The ordinary 3D bounds query remains unchanged.
	auto* City = World->SpawnActorDeferred<ASimCity2000CityActor>(ASimCity2000CityActor::StaticClass(), FTransform::Identity);
	City->bLoadOnConstruction = false;
	City->bLoadOnBeginPlay = false;
	City->FinishSpawning(FTransform::Identity);
	City->TileSize = 400;
	City->TileBuildingIds.Init(INDEX_NONE, FSimCity2000City::TileCount);
	City->TileBuildingIds[64*128+64] = 0;
	auto* Instances = NewObject<UInstancedStaticMeshComponent>(City);
	Instances->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Instances->RegisterComponent();
	const int32 Instance = Instances->AddInstance(FTransform(FRotator::ZeroRotator, Center+FVector(0,0,800), FVector(12,12,4)));
	City->BuildingInstanceComponents.Add(Instances);
	FSimCopterCityBuilding Building;
	Building.OriginTile = FIntPoint(64,64);
	Building.Parts.Add({0, Instance});
	City->Buildings.Add(Building);
	Traffic->SourceCityActor = City;
	const FVector Overhang = Center + FVector(500,0,92);
	TestFalse(TEXT("Fixture misses old vertical bounds test"), City->IsInsideStandingBuildingBounds(Overhang,32));
	TestTrue(TEXT("Horizontal building guard includes raised overhang"), City->IsInsideStandingBuildingBounds(Overhang,32,true));
	TestFalse(TEXT("Patient below neighboring raised building rejected"), Traffic->IsOutdoorPatientSpawnValid(Overhang));
	TestFalse(TEXT("Patient above neighboring building roof rejected"), Traffic->IsOutdoorPatientSpawnValid(Overhang+FVector(0,0,1200)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterParkedAircraftTrafficTest,
	"SimCopter.Safety.ParkedAircraftTraffic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterParkedAircraftTrafficTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); };
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	auto* Aircraft = World->SpawnActor<ASimCopterHelicopterPawn>();
	Aircraft->SetActorLocation(FVector(1000,0,150));
	Aircraft->SetActorRotation(FRotator(0,90,0));
	Aircraft->SetActorScale3D(FVector(3));
	const FBox Bounds = Aircraft->GetParkingWorldBounds();
	if (!TestTrue(TEXT("Aircraft has actual parking geometry"), Bounds.IsValid != 0)) return false;
	auto* Car = World->SpawnActor<ASimCopterGroundAgent>();
	Car->ConfigureAgent(ESimCopterGroundAgentKind::Vehicle, TEXT(""), TEXT(""), 600);
	Car->SetOwner(Traffic);
	Traffic->VehicleAgents.Add(Car);
	const FVector Start(Bounds.Min.X-500, Bounds.GetCenter().Y, Bounds.Min.Z+Car->GetCapsuleHalfHeightCm());
	Car->SetMoveTarget(Start+FVector(4000,0,0));
	for (ESimCopterTrafficAiMode Mode : {ESimCopterTrafficAiMode::Original, ESimCopterTrafficAiMode::Modernized})
	{
		Traffic->TrafficAiMode = Mode;
		Car->SetActorLocation(Start);
		Car->CurrentVelocityCmPerSec = FVector(600,0,0);
		Traffic->UpdateTrafficInteractions(1.0f/60.0f);
		TestTrue(TEXT("Unoccupied helicopter stops traffic in both AI modes"), Car->IsAircraftTrafficBlocked());
		for (float Step : {1.0f/60.0f, 0.1f, 0.5f})
		{
			Car->SetTrafficSpeedScale(1.75f); // Dispatch/speeders cannot override the safety hold.
			Car->UpdateMovement(Step);
			Car->MoveByTrafficSeparation(FVector(50,0,0));
			TestTrue(TEXT("Stopped car cannot coast or be pushed into aircraft"), Car->GetActorLocation().Equals(Start,0.01));
		}
	}
	// A lane clear of the entire body/rotor span must remain usable.
	Traffic->VehicleAgents.Reset();
	Car->SetAircraftTrafficBlocked(false);
	Traffic->ApplyAircraftRoadBlocking(0.1f);
	TestTrue(TEXT("Responder outside ambient pool still stops"), Car->IsAircraftTrafficBlocked());
	Traffic->VehicleAgents.Add(Car);
	Car->SetActorLocation(Start+FVector(0, Bounds.GetSize().Y+1000, 0));
	Traffic->ApplyAircraftRoadBlocking(0.1f);
	TestFalse(TEXT("Aircraft beside the road does not stop clear lane"), Car->IsAircraftTrafficBlocked());
	Car->SetActorLocation(FVector(Bounds.Max.X+500,Start.Y,Start.Z));
	Traffic->ApplyAircraftRoadBlocking(0.1f);
	TestFalse(TEXT("Aircraft behind a car does not stop it"), Car->IsAircraftTrafficBlocked());
	Car->SetActorLocation(Start);
	Aircraft->AddActorWorldOffset(FVector(0,0,3000));
	Traffic->UpdateTrafficInteractions(0.1f);
	TestFalse(TEXT("Aircraft overhead releases road traffic"), Car->IsAircraftTrafficBlocked());
	Car->UpdateMovement(0.1f);
	TestTrue(TEXT("Car resumes its original route after takeoff"), Car->GetActorLocation().X > Start.X);
	Aircraft->AddActorWorldOffset(FVector(0,0,-3000));
	Car->SetActorLocation(Start);
	Traffic->ApplyAircraftRoadBlocking(0.1f);
	TestTrue(TEXT("Landing blocks the road again"), Car->IsAircraftTrafficBlocked());
	Aircraft->Destroy();
	Traffic->UpdateTrafficInteractions(0.1f);
	TestFalse(TEXT("Removing aircraft releases the hold"), Car->IsAircraftTrafficBlocked());
	return true;
}
#endif
