#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/World.h"
#include "Camera/CameraComponent.h"
#include "City/SimCity2000CityActor.h"
#include "Flight/SimCopterHelicopterPawn.h"
#include "Ground/SimCopterAmbientVehicles.h"
#include "Ground/SimCopterTrafficSystemActor.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Formats/SimCity2000Reader.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterSpotlightCameraRuntimeTest, "SimCopter.Spotlight.DedicatedCamera",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterSpotlightCameraRuntimeTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	auto* Heli = World->SpawnActor<ASimCopterHelicopterPawn>();
	Heli->SetActorLocation(FVector(0, 0, 3000));
	Heli->SpotlightTarget.bValid = true;
	Heli->SpotlightTarget.WorldLocation = FVector(600, 300, 0);
	Heli->SetCameraMode(ESimCopterCameraMode::Spotlight);
	Heli->UpdateCameraForReplay(0);
	TestTrue(TEXT("Dedicated belly camera is active"), Heli->SpotlightCameraComponent->IsActive());
	TestFalse(TEXT("Chase camera is inactive"), Heli->CameraComponent->IsActive());
	const FVector Expected = (Heli->SpotlightTarget.WorldLocation - Heli->SpotlightCameraComponent->GetComponentLocation()).GetSafeNormal();
	TestTrue(TEXT("Camera centres the exact spotlight target"), Heli->SpotlightCameraComponent->GetForwardVector().Equals(Expected, 0.0001));
	TArray<uint8> Save;
	TestTrue(TEXT("Can save spotlight camera"), Heli->CaptureRuntimeSaveState(Save));
	TestTrue(TEXT("Can restore spotlight camera without changing save layout"), Heli->RestoreRuntimeSaveState(Save));
	TestEqual(TEXT("Spotlight mode restored"), Heli->GetCameraMode(), ESimCopterCameraMode::Spotlight);
	Heli->SetCameraMode(ESimCopterCameraMode::Chase);
	Heli->UpdateCameraForReplay(0);
	TestFalse(TEXT("Dedicated camera deactivates on return"), Heli->SpotlightCameraComponent->IsActive());
	TestTrue(TEXT("Normal camera returns"), Heli->CameraComponent->IsActive());
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterPlaneDeckRescueTest, "SimCopter.Missions.PlaneDeckRescue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimCopterPlaneDeckRescueTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	auto* City = World->SpawnActor<ASimCity2000CityActor>();
	auto* Traffic = World->SpawnActor<ASimCopterTrafficSystemActor>();
	Traffic->SourceCityActor = City;
	Traffic->PeopleTileClasses.Init(0, FSimCity2000City::TileCount);
	Traffic->TileCenterWorldZ.Init(0, FSimCity2000City::TileCount);
	Traffic->WaterTileFlags.Init(1, FSimCity2000City::TileCount);
	Traffic->ActiveTileSize = 400;
	auto* Ambient = World->SpawnActor<ASimCopterAmbientVehiclesActor>();
	Ambient->PendingPlaneRescueWreck = Ambient->AddWreck(SimCopterAmbientVehicles::PlaneObjectId,
		FVector(200,-200,0), FVector::ForwardVector, 0, 42, false, 0);
	if (TestNotNull(TEXT("Original plane wreck mesh loads"), Ambient->PendingPlaneRescueWreck))
	{
		int32 X = -1, Y = -1;
		TestTrue(TEXT("Plane owns rescue spawn without needing boat slot"), Ambient->TryActivateBoatRescue(42, 300, 64, 64, X, Y));
		TestEqual(TEXT("Marker remains at plane tile"), FIntPoint(X,Y), FIntPoint(64,64));
		TestTrue(TEXT("Visible survivors were created"), Traffic->PedestrianAgents.Num() > 0);
		for (const auto& Weak : Traffic->PedestrianAgents)
		{
			const auto* Person = Weak.Get();
			if (!Person) continue;
			TestFalse(TEXT("Survivor is visible"), Person->IsHidden());
			const FVector Feet = Person->GetActorLocation() - FVector(0,0,Person->GetCapsuleHalfHeightCm());
			FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(PlaneDeckTest), true);
			TestTrue(TEXT("Survivor's feet are on actual wreck triangles"), Ambient->PendingPlaneRescueWreck->Mesh->LineTraceComponent(
				Hit, Feet + FVector(0,0,10), Feet - FVector(0,0,10), Query));
			float WaterZ = 0;
			Ambient->TryGetWaterSurfaceZ(Person->GetActorLocation(), WaterZ);
			TestTrue(TEXT("Survivor stands above the swimming band"), Feet.Z > WaterZ + 40);
		}
		Ambient->PendingPlaneRescueWreck = nullptr;
	}
	World->DestroyWorld(false);
	return true;
}
#endif
