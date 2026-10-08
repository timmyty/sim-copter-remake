#if WITH_DEV_AUTOMATION_TESTS
#include "City/SimCity2000CityActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterWaterSupplyGeometryTest, "SimCopter.Water.SupplyGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterWaterSupplyGeometryTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	auto* City = World->SpawnActor<ASimCity2000CityActor>();
	City->AddWaterSupplyTriangle(FVector(0,0,50), FVector(100,0,50), FVector(0,100,50));
	float Z = 0; uint8 Terrain = 255;
	TestTrue(TEXT("Pool polygon supplies water even on non-water terrain"), City->TryGetBucketWaterSurface(FVector(25,25,50), Z, Terrain));
	TestEqual(TEXT("Pool uses visible elevation"), Z, 50.0f);
	TestEqual(TEXT("Pool classified as scoopable water"), Terrain, uint8(5));
	TestFalse(TEXT("Land in the same tile stays dry"), City->TryGetBucketWaterSurface(FVector(90,90,50), Z, Terrain));
	City->AddWaterSupplyTriangle(FVector(50000,0,30), FVector(50100,0,30), FVector(50000,100,30));
	TestTrue(TEXT("Rendered ocean beyond map edge supplies water"), City->TryGetBucketWaterSurface(FVector(50020,20,30), Z, Terrain));
	TestEqual(TEXT("Map edge water uses rendered height"), Z, 30.0f);
	TestFalse(TEXT("No fabricated water outside rendered polygons"), City->TryGetBucketWaterSurface(FVector(51000,20,30), Z, Terrain));
	World->DestroyWorld(false);
	return true;
}
#endif
