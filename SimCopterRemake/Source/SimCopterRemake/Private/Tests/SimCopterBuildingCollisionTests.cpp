#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "City/SimCopterRuntimeStaticMesh.h"
#include "Ground/SimCopterGroundAgent.h"
#include "Ground/SimCopterTrafficSystemActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimCopterBuildingCollisionTest,
	"SimCopter.Collision.BuildingWalls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimCopterBuildingCollisionTest::RunTest(const FString& Parameters)
{
	const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
		.CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	// A real runtime building wall, shared by two ISM placements, with open space above it.
	FSimCopterRuntimeMeshSection Wall;
	Wall.Vertices = { FVector(0,-200,0), FVector(0,200,0), FVector(0,200,250), FVector(0,-200,250) };
	Wall.Triangles = {0,1,2, 0,2,3};
	UStaticMesh* Mesh = SimCopterRuntimeStaticMesh::Build(World, {Wall}, true);
	if (!TestNotNull(TEXT("Runtime building mesh builds"), Mesh)) return false;
	TestTrue(TEXT("Building has cooked triangle collision"), Mesh->GetBodySetup() && Mesh->GetBodySetup()->TriMeshGeometries.Num() > 0);
	auto* Building = World->SpawnActor<AActor>();
	auto* Instances = NewObject<UInstancedStaticMeshComponent>(Building);
	Building->SetRootComponent(Instances);
	Instances->SetStaticMesh(Mesh);
	Instances->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Instances->SetCollisionObjectType(ECC_WorldStatic);
	Instances->SetCollisionResponseToAllChannels(ECR_Block);
	Instances->RegisterComponent();
	Instances->AddInstance(FTransform(FVector(0,0,0)));
	Instances->AddInstance(FTransform(FVector(600,0,0)));

	FHitResult Hit;
	TestTrue(TEXT("Simple capsule sweep hits rendered building wall"), World->SweepSingleByChannel(Hit,
		FVector(-100,0,60), FVector(100,0,60), FQuat::Identity, ECC_Camera, FCollisionShape::MakeCapsule(8,20)));
	TestTrue(TEXT("Back side of building wall also blocks"), World->SweepSingleByChannel(Hit,
		FVector(100,0,60), FVector(-100,0,60), FQuat::Identity, ECC_Camera, FCollisionShape::MakeCapsule(8,20)));
	TestFalse(TEXT("Space above the wall stays open"), World->SweepSingleByChannel(Hit,
		FVector(-100,0,300), FVector(100,0,300), FQuat::Identity, ECC_Camera, FCollisionShape::MakeCapsule(8,20)));

	auto* Person = World->SpawnActor<ASimCopterGroundAgent>();
	Person->SetActorLocation(FVector(-100,0,Person->GetCapsuleHalfHeightCm()));
	TestTrue(TEXT("Walk step refuses the building"), Person->IsPedestrianStepBlockedByGeometry(
		Person->GetActorLocation(), FVector(100,0,Person->GetCapsuleHalfHeightCm()), 0));
	// BHAV 308 may relax legacy cell rules, but must never disable physical building walls.
	Person->SetActorLocation(FVector(-80,0,Person->GetCapsuleHalfHeightCm()));
	Person->BehaviorContext.Attributes[EBhavAttr::MoveThroughWalls] = 1;
	Person->ContainOutsideBuildingGeometry();
	Person->SetActorLocation(FVector(80,0,Person->GetCapsuleHalfHeightCm()));
	Person->ContainOutsideBuildingGeometry();
	TestTrue(TEXT("Recovery behavior cannot walk through a building"), Person->GetActorLocation().X < 0);
	Person->SetActorLocation(FVector(-80,0,Person->GetCapsuleHalfHeightCm()));
	Person->MoveAgainstCityGeometry(FVector(1000,0,0), FQuat::Identity);
	TestTrue(TEXT("Long frame or external push cannot tunnel through building"), Person->GetActorLocation().X < 0);
	for (int32 Frame = 0; Frame < 30; ++Frame)
		Person->MoveAgainstCityGeometry(FVector(0.2,0,0), FQuat::Identity);
	TestTrue(TEXT("Repeated tiny steps stay outside wall"), Person->GetActorLocation().X < 0);
	Person->SetActorLocation(FVector(-2,0,Person->GetCapsuleHalfHeightCm()));
	Person->MoveAgainstCityGeometry(FVector(10,0,0), FQuat::Identity);
	TestTrue(TEXT("Initial penetration cannot move deeper"), Person->GetActorLocation().X <= -2);
	Person->MoveAgainstCityGeometry(FVector(-30,0,0), FQuat::Identity);
	TestTrue(TEXT("Initially overlapping pedestrian can escape outward"), Person->GetActorLocation().X < -20);

	Instances->RemoveInstance(0);
	TestFalse(TEXT("Demolished building releases its collision"), World->SweepSingleByChannel(Hit,
		FVector(-100,0,60), FVector(100,0,60), FQuat::Identity, ECC_Camera, FCollisionShape::MakeCapsule(8,20)));
	TestTrue(TEXT("Other building still blocks after instance removal"), World->SweepSingleByChannel(Hit,
		FVector(500,0,60), FVector(700,0,60), FQuat::Identity, ECC_Camera, FCollisionShape::MakeCapsule(8,20)));
	return true;
}
#endif
